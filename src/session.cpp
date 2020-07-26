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

#include "session.hpp"
#include "message.hpp"

#include <functional>

namespace chubby {

using namespace std::placeholders;
using std::shared_ptr;


Session::Session(shared_ptr<Signaling> signaling, string id, RecvCallback dataCallback,
                 RecvCallback mediaCallback)
    : mSignaling(std::move(signaling)), mId(std::move(id)), mDataCallback(std::move(dataCallback)) {

	rtc::InitLogger(rtc::LogLevel::Warning);
	std::cout << "Creating session " << mId << std::endl;

	mToken = mSignaling->recv(mId, std::bind(&Session::processSignaling, this, _1));

	rtc::Configuration config;
	mPeerConnection = std::make_shared<rtc::PeerConnection>(config);

	mPeerConnection->onStateChange(std::bind(&Session::onStateChange, this, _1));
	mPeerConnection->onLocalDescription(std::bind(&Session::onLocalDescription, this, _1));
	mPeerConnection->onLocalCandidate(std::bind(&Session::onLocalCandidate, this, _1));
	mPeerConnection->onDataChannel(std::bind(&Session::onDataChannel, this, _1));

	mPeerConnection->onMedia([mediaCallback = std::move(mediaCallback)](const rtc::binary &bin) {
		mediaCallback(bin.data(), bin.size());
	});
}

Session::~Session() { std::cout << "Destroying session " << mId << std::endl; }

void Session::open() {
	const string label = "data";
	std::cout << "Creating DataChannel \"" << label << "\"" << std::endl;

	const string sdp = "m=audio 54609 UDP/TLS/RTP/SAVPF 109\r\n"
	                   "a=mid:audio\r\n"
	                   "a=sendrecv\r\n"
	                   "m=video 54609 UDP/TLS/RTP/SAVPF 120\r\n"
	                   "a=mid:video\r\n"
	                   "a=sendrecv\r\n";
	mPeerConnection->setLocalDescription(rtc::Description{sdp, rtc::Description::Type::Offer});

	mDataChannel = mPeerConnection->createDataChannel(label);

	mDataChannel->onOpen(std::bind(&Session::onOpen, this));
	mDataChannel->onClosed(std::bind(&Session::onClosed, this));
	mDataChannel->onMessage(std::bind(&Session::onMessage, this, _1));
}

void Session::sendData(const byte *data, size_t size) {
	if (mDataChannel)
		mDataChannel->send(data, size);
}

void Session::sendMedia(const byte *data, size_t size) { mPeerConnection->sendMedia(data, size); }

void Session::processSignaling(Message msg) {
	std::cout << "Processing signaling message, type=\"" << msg.type << "\"" << std::endl;

	if (msg.type == "offer" || msg.type == "answer") {
		mPeerConnection->setRemoteDescription(rtc::Description{msg.body, msg.type});
		return;
	}
	if (msg.type == "candidate") {
		const std::string mid = !msg.params.empty() ? msg.params.front() : "";
		mPeerConnection->addRemoteCandidate(rtc::Candidate{msg.body, mid});
		return;
	}
}

void Session::onStateChange(rtc::PeerConnection::State state) {
	std::cout << "State: " << state << std::endl;
}

void Session::onLocalDescription(const rtc::Description &desc) {
	std::cout << "Description: " << desc << std::endl;
	mSignaling->send(Message{mId, desc.typeString(), string(desc)});
}

void Session::onLocalCandidate(const rtc::Candidate &cand) {
	std::cout << "Candidate: " << cand << std::endl;
	mSignaling->send(Message{mId, "candidate", string(cand), {cand.mid()}});
}

void Session::onDataChannel(std::shared_ptr<rtc::DataChannel> dc) {
	std::cout << "Received DataChannel \"" << dc->label() << "\"" << std::endl;
	mDataChannel = std::move(dc);
	mDataChannel->onClosed(std::bind(&Session::onClosed, this));
	mDataChannel->onMessage(std::bind(&Session::onMessage, this, _1));
	onOpen();
}

void Session::onOpen() { std::cout << "Open" << std::endl; }

void Session::onClosed() { std::cout << "Closed" << std::endl; }

void Session::onMessage(const std::variant<rtc::binary, rtc::string> &message) {
	std::cout << "Message" << std::endl;
	if (std::holds_alternative<rtc::binary>(message)) {
		const auto &bin = std::get<rtc::binary>(message);
		mDataCallback(bin.data(), bin.size());
	}
}

} // namespace chubby
