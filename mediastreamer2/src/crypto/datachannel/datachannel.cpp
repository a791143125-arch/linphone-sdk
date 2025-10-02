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
#include "sctptransport.hpp"
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
};

void _MSDataChannelContext::attachDtlsHdskCb(MSDtlsSrtpContext *DtlsCtx) {
	ms_error("JOHAN attach Dtls Hdsk Cb");
	std::weak_ptr<_MSDataChannelContext> weakThis = shared_from_this();
	ms_error("JOHAN attach Dtls Hdsk Cb 2");
	ms_dtls_srtp_set_handshake_cb(DtlsCtx, [weakThis](MSDtlsSrtpContext *DtlsCtx) {
		if (auto self = weakThis.lock()) {
			ms_error("Datachannel starts Sctp connection");
			rtc::impl::SctpTransport::Ports ports = {}; // TODO: get Sctp ports from SDP through some parameters?
			rtc::impl::SctpTransport::Configuration config =
			    {}; // TODO: get configuration somewhere MTU and max message size

			ms_error("JOHAN: DTLS handshake done on %p, create a Sctp transport", DtlsCtx);
			try {
				self->mSctpTransport = std::make_shared<rtc::impl::SctpTransport>(
				    DtlsCtx, config, ports,
				    // recv callback
				    [](rtc::message_ptr) { ms_error("JOHAN recv message callback"); },
				    // amount callback
				    [](uint16_t streamId, size_t amount) {
					    ms_error("JOHAN amount callback : %d, %ld", streamId, amount);
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
	auto context = new MSDataChannelContext();
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
		ms_message("JOHAN datachannel context destroy Sctp stop done");
		ctx->mSctpTransport = nullptr;
	}

	delete ctx;
	ms_message("Datachannel context destroyed");
}
#else  // HAVE_DATACHANNEL
bool ms_datachannel_supported() {
	return false;
}

MSDataChannelContext *ms_datachannel_context_new(struct _MSMediaStreamSessions *sessions) {
	return nullptr;
}
#endif // HAVE_DATACHANNEL
