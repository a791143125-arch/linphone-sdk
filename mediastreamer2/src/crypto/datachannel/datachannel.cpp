/*
 * Copyright (c) 2025 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/ms_datachannel.h"

#ifdef HAVE_DATACHANNEL
#include "common.hpp"
#include "sctptransport.hpp"
#include "datachannel.hpp"
#include "message.hpp"

using namespace rtc::impl;



namespace {
enum class State : int {
	New,
	Connecting,
	Connected,
	Disconnected,
	Failed,
	Closed,
};
std::ostream &operator<<(std::ostream &os, State state) {
	switch (state) {
		case State::New:
			return os << "New";
		case State::Connecting:
			return os << "Connecting";
		case State::Connected:
			return os << "Connected";
		case State::Disconnected:
			return os << "Disconnected";
		case State::Failed:
			return os << "Failed";
		case State::Closed:
			return os << "Closed";
		default:
			return os << "Unknown";
	}
}

} // anonymous namespace
struct MSDataChannel::Impl : public std::enable_shared_from_this<MSDataChannel::Impl> {
	std::shared_ptr<rtc::impl::SctpTransport> mSctpTransport; /**< Sctp transport layer */
	MSDataChannelParams mParams; /**< configuration parameters */
	std::map<uint16_t, std::shared_ptr<DataChannel>> mChannels; /**< existing channels indexed by their id */
	State mState = State::New;
	rtc::synchronized_callback<State> mStateChangeCallback;

	void startDatachannelOnDtls(MSDtlsSrtpContext *DtlsCtx);
	Impl(MSDataChannelParams &&params) : mParams(std::move(params)) {}
	~Impl() {
		if (mSctpTransport != nullptr) {
			mSctpTransport->stop();
		}
	}
	bool changeState(State newState);
	void remoteCloseDataChannels();
	void openDataChannels();
	std::pair<std::shared_ptr<DataChannel>, bool> findDataChannel(uint16_t id);
	bool send(uint16_t id, const std::vector<std::byte> &msg);
	bool onMessage(uint16_t id, dtcMessageCallback callback);

};


MSDataChannel::MSDataChannel(struct _MSDtlsSrtpContext *dtls_ctx, MSDataChannelParams &&params) : pImpl(std::make_shared<Impl>(std::move(params))) {
	// init after the instanciation of pImpl so we can extract a weak_ptr
	pImpl->startDatachannelOnDtls(dtls_ctx);
}
MSDataChannel::~MSDataChannel() { }


void MSDataChannel::Impl::remoteCloseDataChannels() {
	for (auto &[id, channel] : mChannels) {
		channel->remoteClose();
	}
}
void MSDataChannel::Impl::openDataChannels() {
	for (auto &[id, channel] : mChannels) {
		channel->open();
	}
}

bool MSDataChannel::Impl::changeState(State newState) {

	if (mState == newState || mState == State::Closed) {
		return false;
	}
	mState = newState;
	PLOG_INFO << "Changed state to " << newState;

	mStateChangeCallback(newState);

	return true;
}

std::pair<std::shared_ptr<DataChannel>, bool> MSDataChannel::Impl::findDataChannel(uint16_t id) {
	if (auto it = mChannels.find(id); it != mChannels.end())
		return std::make_pair(it->second, true);
	else
		return std::make_pair(nullptr, false);
}

void MSDataChannel::Impl::startDatachannelOnDtls(MSDtlsSrtpContext *DtlsCtx) {
	rtc::impl::SctpTransport::Ports ports{mParams.sctp_local_port, mParams.sctp_remote_port};
	rtc::impl::SctpTransport::Configuration config = {}; // TODO: get configuration somewhere MTU and max message size

	try {
		auto weakThis = weak_from_this();
		// emplace the datachannels in the context according to param, they'll be considered 
		// fully open once the SCTP connection is completed
		for (const auto &[id, channel] : mParams.channels) {
			rtc::Reliability rel{channel.ordered, (channel.max_time ? std::make_optional<std::chrono::milliseconds>(*channel.max_time):std::nullopt), channel.max_retr}; 
			mChannels.try_emplace(id, std::make_shared<DataChannel>(weakThis, id, channel.label, channel.protocol, rel));
		}
		mSctpTransport = std::make_shared<rtc::impl::SctpTransport>(
				    DtlsCtx, config, ports,
				    // recv callback
				    [weakThis](rtc::message_ptr message) {
					auto shared_this = weakThis.lock();
					if (!shared_this) return;

					if (!message) {
						shared_this->remoteCloseDataChannels();
						return;
					}
					auto [channel, found] = shared_this->findDataChannel(message->stream);
					if (!found) {
						if (message->type == rtc::Message::Reset)
							return; // ignore

						// Invalid, close the DataChannel
						PLOG_WARNING << "Got unexpected message on stream " << message->stream;
						shared_this->mSctpTransport->closeStream(message->stream);
						return;
					}

					if (message->type == rtc::Message::Reset) {
						// Incoming stream is reset, unregister it
						shared_this->mChannels.erase(message->stream);
					}

					if (channel) {
						// Forward the message
						channel->incoming(message);
					} else {
						// DataChannel was destroyed, ignore
						PLOG_DEBUG << "Ignored message on stream " << message->stream << ", DataChannel is destroyed";
					}
				    },
				    // amount callback
				    [](uint16_t , size_t) { },
				    [weakThis](rtc::impl::SctpTransport::State state) {
					    auto shared_this = weakThis.lock();
					    if (!shared_this) {
						    return;
					    }

					    switch (state) {
						    case SctpTransport::State::Connected:
							    shared_this->changeState(State::Connected);
							    // Data channels are only negotiated so they are open in any case
							    // Just tag them as open so we can now process the message sending
							    shared_this->openDataChannels();
							    break;
						    case SctpTransport::State::Failed:
							    shared_this->changeState(State::Failed);
							    // mProcessor.enqueue(&PeerConnection::remoteClose, shared_from_this());
							    break;
						    case SctpTransport::State::Disconnected:
							    shared_this->changeState(State::Disconnected);
							    // mProcessor.enqueue(&PeerConnection::remoteClose, shared_from_this());
							    break;
						    default:
							    // Ignore
							    break;
					    }
				    });
		mSctpTransport->start();
	} catch (std::exception const &e) {
		ms_error("failed to create and start Sctp: %s", e.what());
	}
}

// code from libdatachannel/src/imp/datachannel.cpp
namespace rtc::impl {
	size_t RECV_QUEUE_LIMIT = 1024; // Max per-channel queue size (messages)
	// Messages for the DataChannel establishment protocol (RFC 8832)
	// See https://www.rfc-editor.org/rfc/rfc8832.html

	enum MessageType : uint8_t {
		MESSAGE_OPEN_REQUEST = 0x00,
		MESSAGE_OPEN_RESPONSE = 0x01,
		MESSAGE_ACK = 0x02,
		MESSAGE_OPEN = 0x03
	};

	DataChannel::DataChannel(weak_ptr<MSDataChannel::Impl> msdtc, uint16_t id, string label, string protocol,
        	                 Reliability reliability)
	    : mPeerConnection(msdtc), mId(id), mLabel(std::move(label)), mProtocol(std::move(protocol)),
	      mRecvQueue(RECV_QUEUE_LIMIT, message_size_func) {
		PLOG_VERBOSE << "Instanciate DataChannel with id "<<id;
		if(reliability.maxPacketLifeTime && reliability.maxRetransmits)
			throw std::invalid_argument("Both maxPacketLifeTime and maxRetransmits are set");

	    	mReliability = std::make_shared<Reliability>(std::move(reliability));
	}


	DataChannel::~DataChannel() {
		PLOG_VERBOSE << "Destroying DataChannel";
		try {
			close();
		} catch (const std::exception &e) {
			PLOG_ERROR << e.what();
		}
	}

	void DataChannel::open() {
		PLOG_VERBOSE << "Opening DataChannel "<<mId;
		if (!mIsOpen) {
			triggerOpen();
		}
		mIsOpen = true;
	}

	void DataChannel::close() {
		PLOG_VERBOSE << "Closing DataChannel "<<mId;

		shared_ptr<MSDataChannel::Impl> ctx;
		{
			ctx = mPeerConnection.lock();
		}

		if (ctx) {
			ctx->mSctpTransport->closeStream(mId);

			triggerClosed();
			//resetCallbacks();
		}	
	}

	void DataChannel::remoteClose() { close(); }

	optional<Message> DataChannel::receive() {
		auto next = mRecvQueue.pop();
		return next ? std::make_optional(std::move(**next)) : nullopt;
	}

	optional<message_variant> DataChannel::peek() {
		auto next = mRecvQueue.peek();
		return next ? std::make_optional(to_variant(**next)) : nullopt;
	}

	size_t DataChannel::availableAmount() const { return mRecvQueue.amount(); }

	uint16_t DataChannel::id() const {
		return mId;
	}

	string DataChannel::label() const {
		return mLabel;
	}

	string DataChannel::protocol() const {
		return mProtocol;
	}

	Reliability DataChannel::reliability() const {
		return *mReliability;
	}

	//bool DataChannel::isOpen(void) const { return !mIsClosed && mIsOpen; }

	//bool DataChannel::isClosed(void) const { return mIsClosed; }


	size_t DataChannel::maxMessageSize() const {
		//auto pc = mPeerConnection.lock();
		//return pc ? pc->remoteMaxMessageSize() : DEFAULT_REMOTE_MAX_MESSAGE_SIZE;
		// TODO: the max message size is an sctp related SDP attribute
		return DEFAULT_REMOTE_MAX_MESSAGE_SIZE;
	}


	bool DataChannel::outgoing(message_ptr message) {
		shared_ptr<MSDataChannel::Impl> ctx;
		{
			ctx = mPeerConnection.lock();

			if (mIsClosed || !mIsOpen)
				throw std::runtime_error("DataChannel is not open/closed");

			if (!ctx)
				throw std::runtime_error("DataChannel not open");

			if (message->size() > maxMessageSize())
				throw std::invalid_argument("Message size exceeds limit");

			message->reliability = mReliability;
			message->stream = mId;
		}

		return ctx->mSctpTransport->send(message);
	}

	void DataChannel::incoming(message_ptr message) {
		if (!message || mIsClosed) {
			return;
		}

		switch (message->type) {
		case Message::Control: {
			if (message->size() == 0)
				break; // Ignore
			auto raw = reinterpret_cast<const uint8_t *>(message->data());
			switch (raw[0]) {
				case MESSAGE_OPEN:
				case MESSAGE_ACK:
					PLOG_ERROR << "Received OPEN or ACK DECP message but we do not support DECP: ignore it";
					// TODO: we shall respond to counterpart with a decline/close? message when receiving a DECP OPEN
				break;
				default:
					// Ignore
					break;
			}
			break;
		}
		case Message::Reset:
			remoteClose();
			break;
		case Message::String:
		case Message::Binary:
			mRecvQueue.push(message);
			triggerAvailable(mRecvQueue.size());
			break;
		default:
			// Ignore
			break;
		}
	}

}

/**
 * Check if Datachannel is supported
 * @return true if Datachannel is supported
 */
bool ms_datachannel_supported() {
	return true;
}
//TODO: move the factory function to be a class method and make the constructor private
MSDataChannelHandle *ms_datachannel_create(struct _MSDtlsSrtpContext *dtls_ctx, MSDataChannelParams &&params) {
	ms_message("Creating Data Channel context attached to dtls context %p", dtls_ctx);
	auto context = new MSDataChannel(dtls_ctx, std::move(params));
	return context;
}

extern "C" void ms_datachannel_destroy(MSDataChannelHandle *ctx) {
	ms_message("Datachannel context destroy %p", ctx);
	if (ctx == NULL) {
		ms_warning("Datachannel context destroy but no context given\n");
		return;
	}
	delete ctx;
	ms_message("Datachannel context destroyed");
}
// redirections to Impl
bool MSDataChannel::send(uint16_t id, const std::vector<std::byte> &msg) {
	return pImpl->send(id, msg);
}
bool MSDataChannel::onMessage(uint16_t id, dtcMessageCallback callback) {
	return pImpl->onMessage(id, callback);
}

bool MSDataChannel::Impl::send(uint16_t id, const std::vector<std::byte> &msg) {
	// Is there a data channel with this id
	auto channel = mChannels.find(id);
	if (channel == mChannels.end()) {
		PLOG_ERROR << "MSDataChannel::send but given channel id "<<id<<" does not match any channel, ignore";
		return false;
	}
	return channel->second->outgoing(std::make_shared<rtc::Message>(msg.begin(), msg.end(), rtc::Message::Binary));
}
bool MSDataChannel::Impl::onMessage(uint16_t id, dtcMessageCallback callback) {
	// get the channel
	auto channel = mChannels.find(id);
	if (channel == mChannels.end()) {
		PLOG_ERROR << "MSDataChannel::onMessage but given channel id "<<id<<" does not match any channel, ignore";
		return false;
	}
	channel->second->messageCallback = std::move(callback);
	return true;
}

#else  // HAVE_DATACHANNEL
bool ms_datachannel_supported() {
	return false;
}
MSDataChannelHandle *ms_datachannel_create(BCTBX_UNUSED(struct _MSDtlsSrtpContext *dtls_ctx), BCTBX_UNUSED(MSDataChannelParams &&params)) { return nullptr;}
extern "C" void ms_datachannel_destroy(BCTBX_UNUSED(MSDataChannelHandle *ctx)) {}
#endif // HAVE_DATACHANNEL
