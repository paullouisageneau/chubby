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

#ifndef CHUBBY_SESSION_H
#define CHUBBY_SESSION_H

#include "signaling.hpp"

#include "rtc/rtc.hpp"

#include <memory>

namespace chubby {

using std::byte;

class Session {
public:
	using RecvCallback = std::function<void(const byte *, size_t)>;
	Session(std::shared_ptr<Signaling> signaling, std::string id, RecvCallback dataCallback,
	        RecvCallback mediaCallback);
	~Session();

	void open();
	void sendData(const byte *data, size_t size);
	void sendMedia(const byte *data, size_t size);
	void processSignaling(Message msg);

private:
	void onStateChange(rtc::PeerConnection::State state);
	void onLocalDescription(const rtc::Description &desc);
	void onLocalCandidate(const rtc::Candidate &cand);
	void onDataChannel(std::shared_ptr<rtc::DataChannel> dc);
	void onOpen();
	void onClosed();
	void onMessage(const std::variant<rtc::binary, rtc::string> &message);

	std::shared_ptr<Signaling> mSignaling;
	std::shared_ptr<rtc::PeerConnection> mPeerConnection;
	std::shared_ptr<rtc::DataChannel> mDataChannel;

	std::string mId;
	Signaling::Token mToken;

	RecvCallback mDataCallback;
};

} // namespace chubby

#endif
