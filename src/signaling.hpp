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

#ifndef CHUBBY_SIGNALING_H
#define CHUBBY_SIGNALING_H

#include "message.hpp"

#include "rtc/rtc.hpp"

#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace chubby {

using std::string;

class Signaling {
public:
	using Callback = std::function<void(Message message)>;
	using Token = std::shared_ptr<void>;

	Signaling(Callback defaultRecvCallback);
	~Signaling();

	void connect(string url);
	void disconnect();

	void send(Message message);
	Token recv(string id, Callback recvCallback);

private:
	void onOpen();
	void onClosed();
	void onError(const std::string &error);
	void onMessage(const std::variant<rtc::binary, rtc::string> &data);

	void flush();
	void retry();
	void dispatch(Message message);

	std::shared_ptr<rtc::WebSocket> mWebSocket;
	string mUrl;

	std::unordered_map<string, std::weak_ptr<Callback>> mCallbacks;
	Callback mDefaultCallback;

	std::queue<string> mOutgoing;

	std::mutex mMutex;
};

} // namespace chubby

#endif
