/**
 * Copyright (c) 2020 by Paul-Louis Ageneau
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "signaling.hpp"
#include "session.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

using namespace chubby;
using namespace std::literals;
using std::byte;
using std::string;

std::list<Session> sessions;

void showUsage(const string &name) {
	std::cerr << "Usage: " << name << " <options> LOCAL [REMOTE1, REMOTE2,...]" << std::endl
	          << "Options:" << std::endl
	          << "\t-h, --help\t\tShow this help message" << std::endl
	          << "\t-s, --sig URL\t\tSpecify the signaling server URL" << std::endl
	          << "\t-d, --data ADDRESS\tSpecify the data UDP socket address" << std::endl
	          << "\t-m, --media ADDRESS\tSpecify the media UDP socket address" << std::endl;
}

int udpSocket(const string &name, struct sockaddr_storage &addr, socklen_t &addrlen) {
	string local, host, service;
	size_t p1 = name.find_first_of(':');
	local = name.substr(0, p1);
	if (p1 != string::npos) {
		size_t p2 = name.find_last_of(':');
		if (p2 != string::npos) {
			host = name.substr(p1 + 1, p2 - (p1 + 1));
			service = name.substr(p2 + 1);
		} else {
			host = "localhost";
			service = name.substr(p1 + 1);
		}
	} else {
		host = "localhost";
		std::ostringstream oss;
		oss << std::stol(service) + 1;
		service = oss.str();
	}

	struct addrinfo hints = {};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_ADDRCONFIG;

	struct addrinfo *res = nullptr;
	if (getaddrinfo(host.c_str(), service.c_str(), &hints, &res) != 0 || !res)
		throw std::runtime_error("Failed to resolve remote address");

	std::memcpy(&addr, res->ai_addr, res->ai_addrlen);
	addrlen = res->ai_addrlen;

	freeaddrinfo(res);

	hints.ai_family = addr.ss_family;
	res = nullptr;
	if (getaddrinfo(nullptr, local.c_str(), &hints, &res) != 0 || !res)
		throw std::runtime_error("Failed to resolve local address");

	int sock;
	try {
		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock == -1)
			throw std::runtime_error("Failed to create socket");

		if (bind(sock, reinterpret_cast<struct sockaddr *>(res->ai_addr), res->ai_addrlen) == -1)
			throw std::runtime_error("Failed to bind socket");
	} catch (...) {
		freeaddrinfo(res);
		throw;
	}

	freeaddrinfo(res);
	return sock;
}

int main(int argc, char *argv[]) {
	string url = "ws://localhost:8000";
	string dataName = "8001:localhost:8002";
	string mediaName = "8003:localhost:8004";

	try {
		std::list<string> ids;
		for (int i = 1; i < argc; ++i) {
			const string arg = argv[i];
			if (arg == "-h" || arg == "--help") {
				showUsage(argv[0]);
				return 0;
			} else if (arg == "-s" || arg == "--signaling") {
				if (i + 1 < argc) {
					url = argv[++i];
				} else {
					std::cerr << "--signaling option requires URL as argument." << std::endl;
					return 1;
				}
			} else if (arg == "-d" || arg == "--data") {
				if (i + 1 < argc) {
					dataName = argv[++i];
				} else {
					std::cerr << "--data option requires socket address as argument." << std::endl;
					return 1;
				}
			} else if (arg == "-m" || arg == "--media") {
				if (i + 1 < argc) {
					mediaName = argv[++i];
				} else {
					std::cerr << "--media option requires socket address as argument." << std::endl;
					return 1;
				}
			} else {
				ids.push_back(argv[i]);
			}
		}

		if (ids.empty()) {
			showUsage(argv[0]);
			return 1;
		}

		struct sockaddr_storage dataAddr;
		socklen_t dataAddrLen = sizeof(dataAddr);
		int dataSock = udpSocket(dataName, dataAddr, dataAddrLen);

		auto dataFunc = [dataSock, dataAddr, dataAddrLen](const byte *data, size_t size) {
			int ret = sendto(dataSock, data, size, 0,
			                 reinterpret_cast<const struct sockaddr *>(&dataAddr), dataAddrLen);
			if (ret < 0)
				throw std::runtime_error("send failed");
		};

		struct sockaddr_storage mediaAddr;
		socklen_t mediaAddrLen = sizeof(dataAddr);
		int mediaSock = udpSocket(mediaName, mediaAddr, mediaAddrLen);

		auto mediaFunc = [mediaSock, mediaAddr, mediaAddrLen](const byte *data, size_t size) {
			int ret = sendto(mediaSock, data, size, 0,
			                 reinterpret_cast<const struct sockaddr *>(&mediaAddr), mediaAddrLen);
			if (ret < 0)
				throw std::runtime_error("send failed");
		};

		string localId = std::move(ids.front());
		ids.pop_front();

		std::shared_ptr<Signaling> signaling;
		signaling = std::make_shared<Signaling>([&](Message msg) {
			sessions.emplace_back(signaling, msg.id, dataFunc, mediaFunc).processSignaling(msg);
		});

		if (url.empty() || url.back() != '/')
			url.push_back('/');

		signaling->connect(url + localId);

		while (!ids.empty()) {
			sessions.emplace_back(signaling, ids.front(), dataFunc, mediaFunc).open();
			ids.pop_front();
		}

		while (true) {
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(dataSock, &readfds);
			FD_SET(mediaSock, &readfds);
			int n = std::max(dataSock, mediaSock) + 1;
			if (select(n, &readfds, NULL, NULL, NULL) == -1)
				throw std::runtime_error("select failed");

			const size_t size = 4096;
			char buffer[size];

			if (FD_ISSET(dataSock, &readfds)) {
				struct sockaddr_storage addr;
				socklen_t addrlen = sizeof(addr);
				int ret = recvfrom(dataSock, buffer, size, MSG_DONTWAIT,
				                   reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
				if (ret == 0) {
					break;
				} else if (ret < 0) {
					if (ret != EAGAIN && ret != EWOULDBLOCK)
						throw std::runtime_error("recv failed");
				} else {
					for (auto &s : sessions)
						s.sendData(reinterpret_cast<byte *>(buffer), ret);
				}
			}

			if (FD_ISSET(mediaSock, &readfds)) {
				struct sockaddr_storage addr;
				socklen_t addrlen = sizeof(addr);
				int ret = recvfrom(mediaSock, buffer, size, MSG_DONTWAIT,
				                   reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
				if (ret == 0) {
					break;
				} else if (ret < 0) {
					if (ret != EAGAIN && ret != EWOULDBLOCK)
						throw std::runtime_error("recv failed");
				} else {
					for (auto &s : sessions)
						s.sendMedia(reinterpret_cast<byte *>(buffer), ret);
				}
			}
		}

	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return 2;
	}

	return 0;
}
