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

#include <variant>

namespace chubby {

using namespace std::placeholders;
using std::nullopt;
using std::shared_ptr;

Signaling::Signaling(std::function<void(Message message)> defaultRecvCallback)
    : mDefaultCallback(std::move(defaultRecvCallback)) {}

Signaling::~Signaling() {}

void Signaling::connect(string url) {
	auto ws = std::make_shared<rtc::WebSocket>();
	ws->onOpen(std::bind(&Signaling::onOpen, this));
	ws->onClosed(std::bind(&Signaling::onClosed, this));
	ws->onError(std::bind(&Signaling::onError, this, _1));
	ws->onMessage(std::bind(&Signaling::onMessage, this, _1));
	ws->open(url);

	std::atomic_store(&mWebSocket, ws);
	mUrl = std::move(url);
}

void Signaling::disconnect() {
	auto ws = std::atomic_exchange(&mWebSocket, shared_ptr<rtc::WebSocket>(nullptr));
	ws->close();
}

void Signaling::send(Message message) {
	{
		std::lock_guard lock(mMutex);
		mOutgoing.emplace(std::move(message));
	}
	flush();
}

Signaling::Token Signaling::recv(string id, Callback recvCallback) {
	auto shared = std::make_shared<Callback>(std::move(recvCallback));
	mCallbacks.emplace(std::move(id), shared);
	return shared;
}

void Signaling::onOpen() {
	std::cout << "Signaling open" << std::endl;
	flush();
}

void Signaling::onClosed() {
	std::cout << "Signaling closed" << std::endl;
	retry();
}

void Signaling::onError(const std::string &error) {
	std::cout << "Signaling failed: " << error << std::endl;
	retry();
}

void Signaling::onMessage(const std::variant<rtc::binary, rtc::string> &data) {
	std::cout << "Receiving signaling message" << std::endl;
	if (std::holds_alternative<rtc::string>(data))
		dispatch(Message::Parse(std::get<rtc::string>(data)));
}

void Signaling::flush() {
	auto ws = std::atomic_load(&mWebSocket);
	if (!ws || ws->isClosed())
		return;
	try {
		std::lock_guard lock(mMutex);
		while (!mOutgoing.empty()) {
			ws->send(std::move(mOutgoing.front()));
			mOutgoing.pop();
		}
	} catch (const std::exception &e) {
		// TODO
	}
}

void Signaling::retry() {
	// TODO
}

void Signaling::dispatch(Message message) {
	std::shared_ptr<Callback> locked;
	{
		std::lock_guard lock(mMutex);
		auto it = mCallbacks.find(message.id);
		if (it != mCallbacks.end())
			locked = it->second.lock();
	}

	if (locked) {
		auto &cb = *locked;
		cb(std::move(message));
	} else {
		mDefaultCallback(std::move(message));
	}
}

} // namespace chubby
