/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#include "liblinphone_tester.h"
#include "linphone/api/c-call.h"
#include "linphone/api/c-transcription.h"
#include "linphone/api/c-types.h"
#include "linphone/core.h"
#include "linphone/types.h"
#include "mediastreamer2/msutils.h"
#include "mediastreamer2/msvolume.h"
#include "tester_utils.h"
#include <cstdint>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#define unlink _unlink
#ifndef F_OK
#define F_OK 00 /*visual studio does not define F_OK*/
#endif
#endif

// cb that I give using the api to display the transcription
void transcription_cb(LinphoneTranscription *transcription) {
	uint32_t lastId = linphone_transcription_get_last_sentence_id(transcription);
	uint32_t id = 1;
	if (system("clear")) std::cout << "DIDNT CLEAR!" << std::endl;
	while (id <= lastId) {
		std::cout << "sentence from cb ID : " << id << "  NAME : ["
		          << linphone_transcription_get_name_by_id(transcription, id) << "] "
		          << linphone_transcription_get_sentence_by_id(transcription, id) << std::endl;
		id++;
	}
}

static void call_with_file_player(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphonePlayer *player;
	LinphonePlayerCbs *cbs = NULL;
	// char *hellopath = bc_tester_res("sounds/hello8000.wav"); // 8000 Hz works fine
	// char *hellopath = bc_tester_res("sounds/61-70970-0002.wav");
	char *hellopath = bc_tester_res("sounds/367-130732-0026.wav");
	// char *hellopath = bc_tester_res("sounds/hello16000-1s.wav");
	// char *hellopath = bc_tester_res("sounds/hello44100.wav"); // 44100 Hz fails

	// char *recordpath = bc_tester_file("record-call_with_transcription_8000.wav");
	char *recordpath = bc_tester_file("record-call_16000_crawfish_no_threads.wav");

	bool_t call_ok;
	int attempts = 0;
	double similar = 1;
	const double threshold = 0.9;

	/*this test is actually attempted three times in case of failure, because the audio comparison at the end is very
	 * sensitive to jitter buffer drifts, which sometimes happen if the machine is unable to run the test in good
	 * realtime conditions */
	for (attempts = 0; attempts < 1; attempts++) {
		reset_counters(&marie->stat);
		reset_counters(&pauline->stat);
		linphone_core_reset_tone_manager_stats(marie->lc);
		linphone_core_reset_tone_manager_stats(pauline->lc);

		/*make sure the record file doesn't already exists, otherwise this test will append new samples to it*/
		unlink(recordpath);

		/*make sure we have opus, and force the usage of it in stereo, 48kHz at 60 kb/s */
		LinphonePayloadType *pt;
		pt = linphone_core_get_payload_type(marie->lc, "speex", 16000, 1);
		if (!pt) {
			ms_warning("speex 16000 not available, skip test");
			linphone_core_manager_destroy(marie);
			linphone_core_manager_destroy(pauline); // FIXME comment this to keep transcription files, but core dump
			bc_free(recordpath);
			bc_free(hellopath);
			return;
		}
		// linphone_payload_type_set_recv_fmtp(pt, "stereo=1;sprop-stereo=1");
		linphone_payload_type_set_normal_bitrate(pt, 60);
		linphone_payload_type_unref(pt);
		disable_all_audio_codecs_except_one(marie->lc, "speex", 16000);

		pt = linphone_core_get_payload_type(pauline->lc, "speex", 16000, 1);
		// linphone_payload_type_set_recv_fmtp(pt, "stereo=1;sprop-stereo=1");
		linphone_payload_type_set_normal_bitrate(pt, 60);
		linphone_payload_type_unref(pt);
		disable_all_audio_codecs_except_one(pauline->lc, "speex", 16000);

		/*caller uses files instead of soundcard in order to avoid mixing soundcard input with file played using call's
		 * player*/
		linphone_core_set_use_files(marie->lc, TRUE);
		linphone_core_set_play_file(marie->lc, NULL);

		/*callee is recording and plays file*/
		linphone_core_set_use_files(pauline->lc, TRUE);
		linphone_core_set_play_file(pauline->lc, NULL);
		linphone_core_set_record_file(pauline->lc, recordpath);
		LinphoneTranscription *transcription = linphone_core_get_transcription(pauline->lc);
		linphone_transcription_set_display_cb(transcription, transcription_cb);
		BC_ASSERT_TRUE((call_ok = call(marie, pauline)));
		if (!call_ok) {
			linphone_core_manager_destroy(marie);
			linphone_core_manager_destroy(pauline); // FIXME comment this to keep transcription files, but core dump
			bc_free(recordpath);
			bc_free(hellopath);
			return;
		};
		player = linphone_call_get_player(linphone_core_get_current_call(marie->lc));
		BC_ASSERT_PTR_NOT_NULL(player);
		if (player) {
			cbs = linphone_factory_create_player_cbs(linphone_factory_get());
			linphone_player_cbs_set_eof_reached(cbs, on_player_eof);
			linphone_player_cbs_set_user_data(cbs, marie);
			linphone_player_add_callbacks(player, cbs);
			BC_ASSERT_EQUAL(linphone_player_open(player, hellopath), 0, int, "%d");
			BC_ASSERT_EQUAL(linphone_player_start(player), 0, int, "%d");
		}
		// linphone_transcription_activate(transcription, true);
		// sleep(1);
		// linphone_transcription_activate(transcription, false);
		// sleep(10);
		// linphone_transcription_activate(transcription, true);
		/* This assert should be modified to be at least as long as the WAV file */
		BC_ASSERT_TRUE(wait_for_until(pauline->lc, marie->lc, &marie->stat.number_of_player_eof, 1, 60000000));
		/*wait one second more for transmission to be fully ended (transmission time + jitter buffer)*/
		wait_for_until(pauline->lc, marie->lc, NULL, 0, 1000);

		end_call(marie, pauline);
		BC_ASSERT_EQUAL(ms_audio_diff(hellopath, recordpath, &similar, &audio_cmp_params, NULL, NULL), 0, int, "%d");
		if (cbs) linphone_player_cbs_unref(cbs);
		if (similar >= threshold) break;
	}
	BC_ASSERT_GREATER(similar, threshold, double, "%g");
	BC_ASSERT_LOWER(similar, 1.0, double, "%g");
	if (similar >= threshold && similar <= 1.0) { // FIXME comment this to keep transcription files
		remove(recordpath);
	}

	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline); // FIXME comment this to keep transcription files, but core dump
	bc_free(recordpath);
	bc_free(hellopath);
}

static void call_check_log_duration_cb(BCTBX_UNUSED(LinphoneCore *lc),
                                       LinphoneCall *call,
                                       LinphoneCallState cstate,
                                       BCTBX_UNUSED(const char *message)) {
	if (cstate == LinphoneCallStateEnd || cstate == LinphoneCallStateReleased) {
		LinphoneCallLog *call_log = linphone_call_get_call_log(call);
		BC_ASSERT_PTR_NOT_NULL(call_log);
		if (call_log) {
			BC_ASSERT_GREATER_STRICT(linphone_call_log_get_duration(call_log), 0, int, "%d");
		}
		BC_ASSERT_GREATER_STRICT(linphone_call_get_duration(call), 0, int, "%d");
	}
}

static void call_transcription(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	const LinphoneAddress *from;
	LinphoneCall *pauline_call;
	LinphoneProxyConfig *marie_cfg;
	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_call_state_changed(cbs, call_check_log_duration_cb);
	linphone_core_add_callbacks(marie->lc, cbs);
	linphone_core_add_callbacks(pauline->lc, cbs);

	/* with the account manager, we might lose the identity */
	marie_cfg = linphone_core_get_default_proxy_config(marie->lc);
	{
		BC_ASSERT_PTR_NOT_NULL(marie_cfg);
		LinphoneAddress *marie_addr = linphone_address_clone(linphone_proxy_config_get_identity_address(marie_cfg));
		char *marie_tmp_id = NULL;
		linphone_address_set_display_name(marie_addr, "Super Marie");
		marie_tmp_id = linphone_address_as_string(marie_addr);

		linphone_proxy_config_edit(marie_cfg);
		linphone_proxy_config_set_identity_address(marie_cfg, marie_addr);
		linphone_proxy_config_done(marie_cfg);

		ms_free(marie_tmp_id);
		linphone_address_unref(marie_addr);
	}

	BC_ASSERT_NOT_EQUAL(marie->stat.number_of_LinphoneCoreFirstCallStarted, 1, int, "%d");
	BC_ASSERT_NOT_EQUAL(pauline->stat.number_of_LinphoneCoreFirstCallStarted, 1, int, "%d");
	BC_ASSERT_NOT_EQUAL(marie->stat.number_of_LinphoneCoreLastCallEnded, 1, int, "%d");
	BC_ASSERT_NOT_EQUAL(pauline->stat.number_of_LinphoneCoreLastCallEnded, 1, int, "%d");

	BC_ASSERT_TRUE(call(marie, pauline));

	BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneCoreFirstCallStarted, 1, int, "%d");
	BC_ASSERT_EQUAL(pauline->stat.number_of_LinphoneCoreFirstCallStarted, 1, int, "%d");
	BC_ASSERT_NOT_EQUAL(marie->stat.number_of_LinphoneCoreLastCallEnded, 1, int, "%d");
	BC_ASSERT_NOT_EQUAL(pauline->stat.number_of_LinphoneCoreLastCallEnded, 1, int, "%d");

	pauline_call = linphone_core_get_current_call(pauline->lc);
	BC_ASSERT_PTR_NOT_NULL(pauline_call);
	/*check that display name is correctly propagated in From */
	if (pauline_call) {
		from = linphone_call_get_remote_address(linphone_core_get_current_call(pauline->lc));
		BC_ASSERT_PTR_NOT_NULL(from);
		if (from) {
			const char *dname = linphone_address_get_display_name(from);
			BC_ASSERT_PTR_NOT_NULL(dname);
			if (dname) {
				BC_ASSERT_STRING_EQUAL(dname, "Super Marie");
			}
		}

		const LinphoneCallParams *params = linphone_call_get_remote_params(pauline_call);
		bctbx_list_t *parts = linphone_call_params_get_custom_contents(params);
		BC_ASSERT_PTR_NULL(parts);
	}
}

static test_t transcription_tests[] = {TEST_NO_TAG("Call with file player", call_with_file_player),
                                       TEST_NO_TAG("Call transcription", call_transcription)};

test_suite_t transcription_test_suite = {"Transcription Call",
                                         NULL,
                                         NULL,
                                         liblinphone_tester_before_each,
                                         liblinphone_tester_after_each,
                                         sizeof(transcription_tests) / sizeof(transcription_tests[0]),
                                         transcription_tests,
                                         0};