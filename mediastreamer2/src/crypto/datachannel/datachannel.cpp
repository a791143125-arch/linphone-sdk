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

} // namespace

struct _MSDataChannelContext : public std::enable_shared_from_this<_MSDataChannelContext> {
	std::shared_ptr<_MSDataChannelContext> mKeepAlive; /**< make sure we keep a ref on ourselves*/
	std::shared_ptr<rtc::impl::SctpTransport> mSctpTransport;
	void init() {
		mKeepAlive = std::shared_ptr<_MSDataChannelContext>(this, [](_MSDataChannelContext *) {});
	}
	_MSDataChannelContext() {
		mSctpTransport = nullptr;
	}
	~_MSDataChannelContext() {

		mKeepAlive.reset();
	}
	void attachDtlsHdskCb(MSDtlsSrtpContext *DtlsCtx);
	bool changeState(State newState);

	State state = State::New;
	rtc::synchronized_callback<State> mStateChangeCallback;
};

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

void _MSDataChannelContext::attachDtlsHdskCb(MSDtlsSrtpContext *DtlsCtx) {
	auto weakThis = weak_from_this();
	ms_dtls_srtp_set_handshake_cb(DtlsCtx, [weakThis](MSDtlsSrtpContext *DtlsCtx) {
		if (auto self = weakThis.lock()) {
			rtc::impl::SctpTransport::Ports ports = {}; // TODO: get Sctp ports from SDP through some parameters?
			rtc::impl::SctpTransport::Configuration config =
			    {}; // TODO: get configuration somewhere MTU and max message size

			ms_error("DTC: DTLS handshake done on %p, create a Sctp transport", DtlsCtx);
			try {
				self->mSctpTransport = std::make_shared<rtc::impl::SctpTransport>(
				    DtlsCtx, config, ports,
				    // recv callback
				    [](rtc::message_ptr) { ms_error("DTC recv message callback"); },
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
							    shared_this->changeState(State::Connected);
							    // shared_this->assignDataChannels();
							    // mProcessor.enqueue(&PeerConnection::openDataChannels, shared_from_this());
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
				self->mSctpTransport->start();
			} catch (std::exception const &e) {
				ms_error("failed to create and start Sctp: %s", e.what());
			}
		}
	});
}

/**
 * Check if Datachannel is supported
 * @return true if Datachannel is supported
 */
bool ms_datachannel_supported() {
	return true;
}

extern "C" MSDataChannelContext *ms_datachannel_context_new(struct _MSMediaStreamSessions *sessions) {
	ms_message("Creating Data Channel context on stream sessions [%p] attached to dtls context %p", sessions,
	           sessions->dtls_context);
	auto context = new _MSDataChannelContext();
	context->init();
	context->attachDtlsHdskCb(sessions->dtls_context);
	return context;
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
#else  // HAVE_DATACHANNEL
bool ms_datachannel_supported() {
	return false;
}

extern "C" MSDataChannelContext *ms_datachannel_context_new(struct _MSMediaStreamSessions *sessions) {
	return nullptr;
}
extern "C" void ms_datachannel_context_destroy(MSDataChannelContext *ctx) {
}
#endif // HAVE_DATACHANNEL
