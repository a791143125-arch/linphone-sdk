/*
 * Copyright (c) 2025 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
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
#include "bctoolbox/defs.h"

#include "c-wrapper/c-wrapper.h"
#include "call/call.h"
#include "conference/params/media-session-params-p.h"
#include "conference/participant.h"
#include "core/core.h"
#include "media-session-p.h"
#include "media-session.h"
#include "ms2-streams.h"

#include "linphone/core.h"

using namespace ::std;

LINPHONE_BEGIN_NAMESPACE

/*
 * MS2ApplicationStream implementation.
 */

MS2ApplicationStream::MS2ApplicationStream(StreamsGroup &sg, const OfferAnswerContext &params) : MS2Stream(sg, params) {
	string bindIp = getBindIp();
	mStream = application_stream_new2(getCCore()->factory, bindIp.empty() ? nullptr : bindIp.c_str(), mPortConfig.rtpPort,
	                           mPortConfig.rtcpPort);
	initializeSessions(&mStream->ms);
}

void MS2ApplicationStream::configure(BCTBX_UNUSED(const OfferAnswerContext &params)) {
}

bool MS2ApplicationStream::prepare() {
	MS2Stream::prepare();
	if (isTransportOwner()) {
		application_stream_prepare(mStream);
	}
	return false;
}

void MS2ApplicationStream::finishPrepare() {
	MS2Stream::finishPrepare();
	application_stream_unprepare(mStream);
}

void MS2ApplicationStream::render(const OfferAnswerContext &params, CallSession::State targetState) {
	bool basicChangesHandled = handleBasicChanges(params, targetState);

	if (basicChangesHandled) {
		if (getState() == Running) MS2Stream::render(params, targetState);
		return;
	}

	MS2Stream::render(params, targetState);
	getMediaSessionPrivate().getCurrentParams()->enableDataChannel(ms_datachannel_supported());
	mInternalStats.number_of_starts++;
}

void MS2ApplicationStream::stop() {
	MS2Stream::stop();
	/* In mediastreamer2, stop actually stops and destroys. We immediately need to recreate the stream object for later
	 * use, keeping the sessions (for RTP, SRTP, ZRTP etc) that we setup at the beginning. */
	mStream = application_stream_new_with_sessions(getCCore()->factory, &mSessions);
}

void MS2ApplicationStream::finish() {
	if (mStream) {
		application_stream_stop(mStream);
		mStream = nullptr;
	}
	MS2Stream::finish();
}

MS2ApplicationStream::~MS2ApplicationStream() {
	finish();
}

MediaStream *MS2ApplicationStream::getMediaStream() const {
	return &mStream->ms;
}

void MS2ApplicationStream::handleEvent(BCTBX_UNUSED(const OrtpEvent *ev)) {
}

void MS2ApplicationStream::initZrtp() {
}

std::string MS2ApplicationStream::getLabel() const {
	return std::string();
}

void MS2ApplicationStream::startZrtp() {
}

LINPHONE_END_NAMESPACE
