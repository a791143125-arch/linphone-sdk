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
#include <bctoolbox/defs.h>

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msrtp.h"
#include "private.h"

#include <sys/types.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif

static void application_stream_free(ApplicationStream *stream) {
	media_stream_free(&stream->ms);
	ms_free(stream);
}

static void application_stream_process_rtcp(BCTBX_UNUSED(MediaStream *media_stream), BCTBX_UNUSED(const mblk_t *m)) {
}

ApplicationStream *application_stream_new_with_sessions(MSFactory *factory, const MSMediaStreamSessions *sessions) {
	ApplicationStream *stream = (ApplicationStream *)ms_new0(ApplicationStream, 1);

	stream->ms.type = MSApplication;
	media_stream_init(&stream->ms, factory, sessions);

	ms_factory_enable_statistics(factory, TRUE);
	ms_factory_reset_statistics(factory);

	rtp_session_resync(stream->ms.sessions.rtp_session);
	/*some filters are created right now to allow configuration by the application before start() */
	stream->ms.rtpsend = ms_factory_create_filter(factory, MS_RTP_SEND_ID);
	stream->ms.ice_check_list = NULL;
	stream->ms.qi = ms_quality_indicator_new(stream->ms.sessions.rtp_session);
	ms_quality_indicator_set_label(stream->ms.qi, "text");
	stream->ms.process_rtcp = application_stream_process_rtcp;

	return stream;
}

ApplicationStream *application_stream_new(MSFactory *factory, int loc_rtp_port, int loc_rtcp_port, bool_t ipv6) {
	return application_stream_new2(factory, ipv6 ? "::" : "0.0.0.0", loc_rtp_port, loc_rtcp_port);
}

ApplicationStream *application_stream_new2(MSFactory *factory, const char *ip, int loc_rtp_port, int loc_rtcp_port) {
	ApplicationStream *stream;
	MSMediaStreamSessions sessions = {0};
	sessions.rtp_session = ms_create_duplex_rtp_session(ip, loc_rtp_port, loc_rtcp_port, ms_factory_get_mtu(factory));
	stream = application_stream_new_with_sessions(factory, &sessions);
	stream->ms.owns_sessions = TRUE;
	return stream;
}

ApplicationStream *application_stream_start(ApplicationStream *stream,
                              RtpProfile *profile,
                              const char *rem_rtp_addr,
                              int rem_rtp_port,
                              const char *rem_rtcp_addr,
                              int rem_rtcp_port,
                              int payload_type /* ignored */) {
	RtpSession *rtps = stream->ms.sessions.rtp_session;
	MSConnectionHelper h;

	rtp_session_set_profile(rtps, profile);
	if (rem_rtp_port > 0)
		rtp_session_set_remote_addr_full(rtps, rem_rtp_addr, rem_rtp_port, rem_rtcp_addr, rem_rtcp_port);
	if (rem_rtcp_port > 0) {
		rtp_session_enable_rtcp(rtps, TRUE);
	} else {
		rtp_session_enable_rtcp(rtps, FALSE);
	}

	rtp_session_set_payload_type(rtps, payload_type);

	if (rem_rtp_port > 0) ms_filter_call_method(stream->ms.rtpsend, MS_RTP_SEND_SET_SESSION, rtps);
	stream->ms.rtprecv = ms_factory_create_filter(stream->ms.factory, MS_RTP_RECV_ID);
	ms_filter_call_method(stream->ms.rtprecv, MS_RTP_RECV_SET_SESSION, rtps);
	stream->ms.sessions.rtp_session = rtps;

	if (stream->ms.sessions.ticker == NULL) media_stream_start_ticker(&stream->ms);

	ms_connection_helper_start(&h);
	//ms_connection_helper_link(&h, stream->rttsource, -1, 0);
	ms_connection_helper_link(&h, stream->ms.rtpsend, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, stream->ms.rtprecv, -1, 0);
	//ms_connection_helper_link(&h, stream->rttsink, 0, -1);

	//ms_ticker_attach_multiple(stream->ms.sessions.ticker, stream->rttsource, stream->ms.rtprecv, NULL);

	stream->ms.start_time = stream->ms.last_packet_time = ms_time(NULL);
	stream->ms.is_beginning = TRUE;
	stream->ms.state = MSStreamStarted;
	return stream;
}

void application_stream_stop(ApplicationStream *stream) {
	if (stream->ms.sessions.ticker) {
		if (stream->ms.state == MSStreamPreparing) {
			application_stream_unprepare(stream);
		} else if (stream->ms.state == MSStreamStarted) {
			MSConnectionHelper h;
			stream->ms.state = MSStreamStopped;
			//ms_ticker_detach(stream->ms.sessions.ticker, stream->rttsource);
			ms_ticker_detach(stream->ms.sessions.ticker, stream->ms.rtprecv);

			ms_message("Stopping ApplicationStream");
			media_stream_print_summary(&stream->ms);

			ms_connection_helper_start(&h);
			//ms_connection_helper_unlink(&h, stream->rttsource, -1, 0);
			ms_connection_helper_unlink(&h, stream->ms.rtpsend, 0, -1);
			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h, stream->ms.rtprecv, -1, 0);
			//ms_connection_helper_unlink(&h, stream->rttsink, 0, -1);
		}
	}
	ms_factory_log_statistics(stream->ms.factory);
	application_stream_free(stream);
}

void application_stream_iterate(ApplicationStream *stream) {
	media_stream_iterate(&stream->ms);
}

void application_stream_prepare(ApplicationStream *stream) {
	application_stream_unprepare(stream);
	stream->ms.rtprecv = ms_factory_create_filter(stream->ms.factory, MS_RTP_RECV_ID);
	rtp_session_set_payload_type(stream->ms.sessions.rtp_session, 0);
	ms_filter_call_method(stream->ms.rtprecv, MS_RTP_RECV_SET_SESSION, stream->ms.sessions.rtp_session);
	stream->ms.voidsink = ms_factory_create_filter(stream->ms.factory, MS_VOID_SINK_ID);
	ms_filter_link(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
	media_stream_start_ticker(&stream->ms);
	ms_ticker_attach(stream->ms.sessions.ticker, stream->ms.rtprecv);
	stream->ms.state = MSStreamPreparing;
}

static void stop_preload_graph(ApplicationStream *stream) {
	ms_ticker_detach(stream->ms.sessions.ticker, stream->ms.rtprecv);
	ms_filter_unlink(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
	ms_filter_destroy(stream->ms.voidsink);
	ms_filter_destroy(stream->ms.rtprecv);
	stream->ms.voidsink = stream->ms.rtprecv = NULL;
}

void application_stream_unprepare(ApplicationStream *stream) {
	if (stream->ms.state == MSStreamPreparing) {
		stop_preload_graph(stream);
		stream->ms.state = MSStreamInitialized;
	}
}
