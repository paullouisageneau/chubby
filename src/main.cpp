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
#include <stdexcept>
#include <thread>

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
	          << "\t-d, --data FILE\t\tSpecify the data socket" << std::endl
	          << "\t-m, --media FILE\t\tSpecify the media socket" << std::endl;
}

int unixSocket(const string &name) {
	int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock == -1)
		throw std::runtime_error("Failed to create unix socket");

	struct sockaddr_un sun;
	std::memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	std::strncpy(sun.sun_path, name.c_str(), sizeof(sun.sun_path) - 1);

	unlink(name.c_str());
	if (bind(sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		throw std::runtime_error("Failed to open unix socket " + name);

	return sock;
}

int main(int argc, char *argv[]) {
	string url = "ws://localhost:8000";
	string dataName = "data";
	string mediaName = "media";

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
					std::cerr << "--data option requires unix socket as argument." << std::endl;
					return 1;
				}
			} else if (arg == "-m" || arg == "--media") {
				if (i + 1 < argc) {
					mediaName = argv[++i];
				} else {
					std::cerr << "--media option requires unix socket as argument." << std::endl;
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

		int dataSock = unixSocket(dataName);
		int mediaSock = unixSocket(mediaName);

		auto dataFunc = [dataSock](const byte *data, size_t size) {
			int ret = send(dataSock, data, size, 0);
			if (ret < 0)
				throw std::runtime_error("send failed");
		};

		auto mediaFunc = [mediaSock](const byte *data, size_t size) {
			int ret = send(mediaSock, data, size, 0);
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
				int ret = recv(dataSock, buffer, size, MSG_DONTWAIT);
				if (ret == 0) {
					break;
				} else if (ret < 0) {
					if (ret == EAGAIN || ret == EWOULDBLOCK)
						throw std::runtime_error("recv failed");
				} else {
					for (auto &s : sessions)
						s.sendData(reinterpret_cast<byte *>(buffer), ret);
				}
			}

			if (FD_ISSET(mediaSock, &readfds)) {
				int ret = recv(mediaSock, buffer, size, MSG_DONTWAIT);
				if (ret == 0) {
					break;
				} else if (ret < 0) {
					if (ret == EAGAIN || ret == EWOULDBLOCK)
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
