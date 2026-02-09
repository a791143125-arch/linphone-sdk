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
#include <memory>

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

struct _MSDataChannelContext : public std::enable_shared_from_this<_MSDataChannelContext> {
	std::shared_ptr<_MSDataChannelContext> mKeepAlive; /**< make sure we keep a ref on ourselves : TODO do we really need this ?*/
	std::shared_ptr<rtc::impl::SctpTransport> mSctpTransport; /**< Sctp transport layer */
	MSDataChannelParams mParams; /**< configuration parameters */
	std::map<uint16_t, std::shared_ptr<DataChannel>> mChannels; /**< existing channels indexed by their id */

	_MSDataChannelContext(MSDataChannelParams &&params) : mParams(std::move(params)) {
		mSctpTransport = nullptr;
	}
	~_MSDataChannelContext() {
		mKeepAlive.reset();
	}
	void init() {
		mKeepAlive = std::shared_ptr<_MSDataChannelContext>(this, [](_MSDataChannelContext *) {});
	}
	void startDatachannelOnDtls(MSDtlsSrtpContext *DtlsCtx);
	bool changeState(State newState);
	void remoteCloseDataChannels();
	void openDataChannels();
	std::pair<std::shared_ptr<DataChannel>, bool> findDataChannel(uint16_t id);

	State state = State::New;
	rtc::synchronized_callback<State> mStateChangeCallback;
};

void _MSDataChannelContext::remoteCloseDataChannels() {
	ms_error("DTC: remoteCloseDataChannels");
	for (auto &[id, channel] : mChannels) {
		channel->remoteClose();
	}
}
void _MSDataChannelContext::openDataChannels() {
	ms_error("DTC: openDataChannels");
	for (auto &[id, channel] : mChannels) {
		channel->open();
	}
}

bool _MSDataChannelContext::changeState(State newState) {

	if (state == newState || state == State::Closed) {
		return false;
	}
	state = newState;

	std::ostringstream s;
	s << newState;
	PLOG_INFO << "Changed state to " << s.str();

	mStateChangeCallback(newState);

	return true;
}

std::pair<std::shared_ptr<DataChannel>, bool> _MSDataChannelContext::findDataChannel(uint16_t id) {
	if (auto it = mChannels.find(id); it != mChannels.end())
		return std::make_pair(it->second, true);
	else
		return std::make_pair(nullptr, false);
}

void _MSDataChannelContext::startDatachannelOnDtls(MSDtlsSrtpContext *DtlsCtx) {
	rtc::impl::SctpTransport::Ports ports{mParams.sctp_local_port, mParams.sctp_remote_port};
	rtc::impl::SctpTransport::Configuration config = {}; // TODO: get configuration somewhere MTU and max message size

	// TODO: Open the data channels from params
	//DataChannel::DataChannel(weak_ptr<_MSDataChannelContext> msdtc, uint16_t id, string label, string protocol,
        //	                 Reliability reliability)

	ms_error("DTC: startDatachannel on Dtls after handshake done on %p, create a Sctp transport", DtlsCtx);
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
				    	ms_error("DTC recv message callback");
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
				    [](uint16_t streamId, size_t amount) {
					    ms_error("DTC amount callback : %d, %ld", streamId, amount);
				    },
				    [weakThis](rtc::impl::SctpTransport::State state) {
					    PLOG_ERROR << "DTC state change callback dtc : " << state;
					    auto shared_this = weakThis.lock();
					    if (!shared_this) return;

					    switch (state) {
						    case SctpTransport::State::Connected:
							    {
							    shared_this->changeState(State::Connected);
							    // Data channels are only negotiated so they are open in any case
							    // Just tag them as open so we can now process the message sending
							    shared_this->openDataChannels();
							    // Send a Hello just to try it
							    std::byte byteArray[] = { std::byte{0x68}, std::byte{0x65}, std::byte{0x6C}, std::byte{0x6C}, std::byte{0x6F} };
							    ms_datachannel_send(shared_this.get(), 1, byteArray, 5);
							    }
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

	DataChannel::DataChannel(weak_ptr<_MSDataChannelContext> msdtc, uint16_t id, string label, string protocol,
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
		mIsOpen = true;
	}

	void DataChannel::close() {
		PLOG_VERBOSE << "Closing DataChannel "<<mId;

		shared_ptr<_MSDataChannelContext> ctx;
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

	optional<message_variant> DataChannel::receive() {
		auto next = mRecvQueue.pop();
		return next ? std::make_optional(to_variant(std::move(**next))) : nullopt;
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
		PLOG_ERROR << "DTC: DataChannel outgoing message";
		shared_ptr<_MSDataChannelContext> ctx;
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
		PLOG_ERROR << "DTC: DataChannel incoming message";
		if (!message || mIsClosed)
			return;

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
void ms_datachannel_create(struct _MSMediaStreamSessions *sessions, MSDataChannelParams &&params) {
	if (sessions->datachannel_context == NULL) {
		ms_message("Creating Data Channel context on stream sessions [%p] attached to dtls context %p", sessions,
	        	   sessions->dtls_context);
		auto context = new _MSDataChannelContext(std::move(params));
		context->init();
		context->startDatachannelOnDtls(sessions->dtls_context);
		sessions->datachannel_context = context;
	} else {
		ms_warning("Create datachannel but context already exists: ignore it");
	}
}

extern "C" void ms_datachannel_context_destroy(MSDataChannelContext *ctx) {
	ms_message("Datachannel context destroy %p", ctx);
	if (ctx == NULL) {
		ms_warning("Datachannel context destroy but no context given\n");
		return;
	}
	if (ctx->mSctpTransport != NULL) {
		ctx->mSctpTransport->stop();
		ms_message("DTC datachannel context destroy Sctp stop done");
		ctx->mSctpTransport = nullptr;
	}

	delete ctx;
	ms_message("Datachannel context destroyed");
}
bool ms_datachannel_send(MSDataChannelContext *ctx, uint16_t id, const std::byte *msg, size_t size) {
	// Is there a data channel with this id
	if (!ctx) {
		PLOG_ERROR << "ms_datachannel_send but given context is null, ignore";
		return false;
	}

	auto channel = ctx->mChannels.find(id);
	if (channel == ctx->mChannels.end()) {
		PLOG_ERROR << "ms_datachannel_send but given channel id "<<id<<" does not match any channel, ignore";
		return false;
	}
	return channel->second->outgoing(std::make_shared<rtc::Message>(msg, msg+size, rtc::Message::Binary));
}
#else  // HAVE_DATACHANNEL
bool ms_datachannel_supported() {
	return false;
}
bool ms_datachannel_send(MSDataChannelContext *ctx, uint16_t id, const std::byte *msg, size_t size) {}
void ms_datachannel_create(struct _MSMediaStreamSessions *sessions, MSDataChannelParams &&params) {}
extern "C" void ms_datachannel_context_destroy(BCTBX_UNUSED(MSDataChannelContext *ctx)) {}
#endif // HAVE_DATACHANNEL
