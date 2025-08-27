/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#include "bctoolbox/defs.h"
#include "bctoolbox/list.h"
#include "bctoolbox/tester.h"
#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msaudiomixer.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msnoisesuppressor.h"
#include "mediastreamer2/msutils.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "ortp/port.h"

// FIXME FHA
#define PLAY_DURATION_MS 60000

static MSFactory *msFactory = NULL;

static int tester_before_all(void) {
	msFactory = ms_tester_factory_new();
	ms_factory_enable_statistics(msFactory, TRUE);
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(msFactory);
	return 0;
}

static void
fileplay_eof(void *user_data, BCTBX_UNUSED(MSFilter *f), unsigned int event, BCTBX_UNUSED(void *event_data)) {
	if (event == MS_FILE_PLAYER_EOF) {
		int *done = (int *)user_data;
		*done = TRUE;
	}
}

static bool_t noise_test_create_player(MSFilter **player, char *input_file) {
	if (!input_file) {
		BC_FAIL("no file to play");
		return FALSE;
	}
	*player = ms_factory_create_filter(msFactory, MS_FILE_PLAYER_ID);
	ms_filter_call_method_noarg(*player, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(*player, MS_FILE_PLAYER_OPEN, input_file);
	if (!BC_ASSERT_PTR_NOT_NULL(*player)) {
		return FALSE;
	}
	return TRUE;
}

static void play_noisy_audio(char *model,
                             char *noise_file,
                             char *output_file,
                             int input_sample_rate,
                             float play_duration_ms,
                             int start_noise_ms,
                             float noise_gain) {
	char *talk_file;
	if (input_sample_rate == 48000) {
		talk_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	} else {
		talk_file = bc_tester_res("sounds/farend_simple_talk.wav");
	}
	unlink(output_file);
	char *output_file_with_noise = bc_tester_file("output_noisy_audio.wav");
	unlink(output_file_with_noise);

	int nchannels = 1;
	int filter_nchannels = 0;
	int noise_duration_ms = 0;
	int talk_duration_ms = 0;
	int audio_done = 0;
	unsigned int filter_mask = FILTER_MASK_FILEPLAY | FILTER_MASK_SOUNDWRITE;
	MSFilter *player_noise = NULL;
	MSFilter *player_talk = NULL;
	MSFilter *resampler_noise = NULL;
	MSFilter *resampler_talk = NULL;
	MSFilter *resampler_ns = NULL;
	MSFilter *mixer = NULL;
	MSFilter *noise_suppressor = NULL;
	MSFilter *tee_for_two = NULL;
	MSFilter *sound_rec = NULL;
	MSFilter *sound_rec_with_noise = NULL;

	char *filter_name;
	if (strcmp(model, "webrtcns") == 0) {
		filter_name = "MSWebRTCNoiseSuppressor";
	} else if (strcmp(model, "rnnoise") == 0) {
		filter_name = "MSNoiseSuppressor";
	} else {
		BC_FAIL("unknown filter name for denoising");
		goto end;
	}

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);

	// talk
	if (!noise_test_create_player(&player_talk, talk_file)) {
		BC_FAIL("cannot create talk player");
		goto end;
	}
	int signal_input_rate = 0;
	int nchannels_input = 0;
	ms_filter_call_method(player_talk, MS_FILTER_GET_SAMPLE_RATE, &signal_input_rate);
	ms_filter_call_method(player_talk, MS_FILTER_GET_NCHANNELS, &nchannels_input);
	ms_message("audio parameters read are: sample rate = %d Hz, mono/stereo = %d", signal_input_rate, nchannels_input);

	// noise
	if (!noise_test_create_player(&player_noise, noise_file)) {
		BC_FAIL("cannot create noise player");
		goto end;
	}
	int noise_input_rate = 0;
	ms_filter_call_method(player_noise, MS_FILTER_GET_SAMPLE_RATE, &noise_input_rate);
	ms_filter_call_method(player_noise, MS_FILTER_GET_NCHANNELS, &filter_nchannels);
	ms_message("audio parameters read are: sample rate = %d Hz, mono/stereo = %d", noise_input_rate, filter_nchannels);
	if ((noise_input_rate != signal_input_rate) || (filter_nchannels != nchannels_input)) {
		resampler_noise = ms_factory_create_filter(msFactory, MS_RESAMPLE_ID);
		ms_filter_call_method(resampler_noise, MS_FILTER_SET_SAMPLE_RATE, &noise_input_rate);
		ms_filter_call_method(resampler_noise, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &signal_input_rate);
		ms_filter_call_method(resampler_noise, MS_FILTER_SET_NCHANNELS, &filter_nchannels);
		ms_filter_call_method(resampler_noise, MS_FILTER_SET_OUTPUT_NCHANNELS, &nchannels_input);
		ms_message("resample noise audio at rate %d to get %d Hz", noise_input_rate, signal_input_rate);
	}

	mixer = ms_factory_create_filter(msFactory, MS_AUDIO_MIXER_ID);
	ms_filter_call_method(mixer, MS_FILTER_SET_SAMPLE_RATE, &signal_input_rate);
	ms_filter_call_method(mixer, MS_FILTER_SET_NCHANNELS, &nchannels);

	MSAudioMixerCtl gainctl;
	gainctl.pin = 0;
	gainctl.param.gain = noise_gain;
	ms_filter_call_method(mixer, MS_AUDIO_MIXER_SET_INPUT_GAIN, &gainctl);
	gainctl.pin = 1;
	gainctl.param.gain = 1.;
	ms_filter_call_method(mixer, MS_AUDIO_MIXER_SET_INPUT_GAIN, &gainctl);

	tee_for_two = ms_factory_create_filter(msFactory, MS_TEE_ID);

	MSFilterDesc *ns_desc = ms_factory_lookup_filter_by_name(msFactory, filter_name);
	if (!ns_desc) {
		ms_error("Filter description not found for %s", filter_name);
		BC_FAIL("Filter description not found.");
		goto end;
	}
	noise_suppressor = ms_factory_create_filter_from_desc(msFactory, ns_desc);
	int ns_sample_rate = 0;
	int ns_nchannels = 0;
	if (strcmp(model, "webrtcns") == 0) {
		ms_filter_call_method(noise_suppressor, MS_FILTER_SET_SAMPLE_RATE, &signal_input_rate);
	}
	ms_filter_call_method(noise_suppressor, MS_FILTER_GET_SAMPLE_RATE, &ns_sample_rate);
	ms_filter_call_method(noise_suppressor, MS_FILTER_GET_NCHANNELS, &ns_nchannels);
	if ((ns_sample_rate != signal_input_rate) || (ns_nchannels != nchannels_input)) {
		ms_message("wrong sampling rate, RNNoise requires %d Hz.", ns_sample_rate);
		resampler_ns = ms_factory_create_filter(msFactory, MS_RESAMPLE_ID);
		ms_filter_call_method(resampler_ns, MS_FILTER_SET_SAMPLE_RATE, &signal_input_rate);
		ms_filter_call_method(resampler_ns, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &ns_sample_rate);
		ms_filter_call_method(resampler_ns, MS_FILTER_SET_NCHANNELS, &nchannels_input);
		ms_filter_call_method(resampler_ns, MS_FILTER_SET_OUTPUT_NCHANNELS, &ns_nchannels);

		ms_message("resample audio before noise suppressor at rate %d Hz to get %d Hz and %d channel(s)",
		           signal_input_rate, ns_sample_rate, ns_nchannels);
	}
	ms_filter_call_method(noise_suppressor, MS_FILTER_GET_SAMPLE_RATE, &ns_sample_rate);
	ms_filter_call_method(noise_suppressor, MS_FILTER_GET_NCHANNELS, &ns_nchannels);

	sound_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_SAMPLE_RATE, &ns_sample_rate);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_NCHANNELS, &ns_nchannels);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(sound_rec, MS_FILE_REC_OPEN, output_file);

	sound_rec_with_noise = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(sound_rec_with_noise, MS_FILTER_SET_SAMPLE_RATE, &ns_sample_rate);
	ms_filter_call_method(sound_rec_with_noise, MS_FILTER_SET_NCHANNELS, &ns_nchannels);
	ms_filter_call_method_noarg(sound_rec_with_noise, MS_FILE_REC_CLOSE);
	ms_filter_call_method(sound_rec_with_noise, MS_FILE_REC_OPEN, output_file_with_noise);

	MSConnectionHelper h;
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, player_noise, -1, 0);
	if (resampler_noise) ms_connection_helper_link(&h, resampler_noise, 0, 0);
	ms_connection_helper_link(&h, mixer, 0, 0);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, player_talk, -1, 0);
	ms_connection_helper_link(&h, mixer, 1, 0);
	if (resampler_ns) ms_connection_helper_link(&h, resampler_ns, 0, 0);
	ms_connection_helper_link(&h, tee_for_two, 0, 0);
	ms_connection_helper_link(&h, noise_suppressor, 0, 0);
	ms_connection_helper_link(&h, sound_rec, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, tee_for_two, -1, 1);
	ms_connection_helper_link(&h, sound_rec_with_noise, 0, -1);

	ms_ticker_attach(ms_tester_ticker, noise_suppressor);

	ms_filter_call_method(player_noise, MS_PLAYER_GET_DURATION, &noise_duration_ms);
	ms_message("noise duration is %d ms", noise_duration_ms);
	printf("noise duration is %fs\n", (float)noise_duration_ms / 1000.);
	if (start_noise_ms > 0) ms_filter_call_method(player_noise, MS_PLAYER_SEEK_MS, &start_noise_ms);
	ms_filter_call_method(player_talk, MS_PLAYER_GET_DURATION, &talk_duration_ms);
	ms_message("talk duration is %d ms", talk_duration_ms);
	printf("talk duration is %fs\n", (float)talk_duration_ms / 1000.);
	ms_filter_add_notify_callback(player_noise, fileplay_eof, &audio_done, TRUE);
	ms_filter_call_method_noarg(player_noise, MS_PLAYER_START);
	ms_filter_call_method_noarg(player_talk, MS_PLAYER_START);
	int wait_ms = 0;
	ms_filter_call_method(player_talk, MS_FILE_PLAYER_LOOP, &wait_ms);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_START);
	ms_filter_call_method_noarg(sound_rec_with_noise, MS_FILE_REC_START);

	// real time
	int time_step_usec = 10000;
	struct timeval start_time;
	struct timeval now;
	float elapsed = 0.;
	bctbx_gettimeofday(&start_time, NULL);
	while (audio_done != 1 && elapsed < play_duration_ms) {
		bctbx_gettimeofday(&now, NULL);
		elapsed = ((now.tv_sec - start_time.tv_sec) * 1000.0f) + ((now.tv_usec - start_time.tv_usec) / 1000.0f);
		ms_usleep(time_step_usec);
	}

	ms_filter_call_method_noarg(player_noise, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method_noarg(player_talk, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	ms_filter_call_method_noarg(sound_rec_with_noise, MS_FILE_REC_CLOSE);
	ms_ticker_detach(ms_tester_ticker, noise_suppressor);

	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, player_noise, -1, 0);
	if (resampler_noise) ms_connection_helper_unlink(&h, resampler_noise, 0, 0);
	ms_connection_helper_unlink(&h, mixer, 0, 0);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, player_talk, -1, 0);
	ms_connection_helper_unlink(&h, mixer, 1, 0);
	if (resampler_ns) ms_connection_helper_unlink(&h, resampler_ns, 0, 0);
	ms_connection_helper_unlink(&h, tee_for_two, 0, 0);
	ms_connection_helper_unlink(&h, noise_suppressor, 0, 0);
	ms_connection_helper_unlink(&h, sound_rec, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, tee_for_two, -1, 1);
	ms_connection_helper_unlink(&h, sound_rec_with_noise, 0, -1);

end:
	ms_factory_log_statistics(msFactory);
	if (player_noise) ms_filter_destroy(player_noise);
	if (player_talk) ms_filter_destroy(player_talk);
	if (resampler_noise) ms_filter_destroy(resampler_noise);
	if (resampler_talk) ms_filter_destroy(resampler_talk);
	if (resampler_ns) ms_filter_destroy(resampler_ns);
	if (mixer) ms_filter_destroy(mixer);
	if (noise_suppressor) ms_filter_destroy(noise_suppressor);
	if (tee_for_two) ms_filter_destroy(tee_for_two);
	if (sound_rec) ms_filter_destroy(sound_rec);
	if (sound_rec_with_noise) ms_filter_destroy(sound_rec_with_noise);
	if (filter_mask) ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
	bctbx_free(talk_file);
	bctbx_free(output_file);
	bctbx_free(output_file_with_noise);
}

static void talk_with_very_slight_noise_48kHz_mswebrtcns(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_very_slight_noise_48kHz_mswebrtcns.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.1;
	char *model = "webrtcns";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_very_slight_noise_48kHz_rnnoise(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_very_slight_noise_48kHz_rnnoise.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.1;
	char *model = "rnnoise";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_slight_noise_48kHz_mswebrtcns(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_slight_noise_48kHz_mswebrtcns.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.2;
	char *model = "webrtcns";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_slight_noise_48kHz_rnnoise(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_slight_noise_48kHz_rnnoise.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.2;
	char *model = "rnnoise";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_medium_noise_mswebrtcns(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_16kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_medium_noise_mswebrtcns.wav");
	int rate_Hz = 16000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "webrtcns";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_medium_noise_rnnoise(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_16kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_medium_noise_rnnoise.wav");
	int rate_Hz = 16000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "rnnoise";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_medium_noise_48kHz_mswebrtcns(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_medium_noise_48kHz_mswebrtcns.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "webrtcns";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_medium_noise_48kHz_rnnoise(void) {
	char *noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_medium_noise_48kHz_rnnoise.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "rnnoise";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_increasing_noise_mswebrtcns(void) {
	char *noise_file = bc_tester_res("sounds/echo_8_linux_inscreasing_noise_1min_16kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_increasing_noise_mswebrtcns.wav");
	int rate_Hz = 16000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "webrtcns";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_increasing_noise_rnnoise(void) {
	char *noise_file = bc_tester_res("sounds/echo_8_linux_inscreasing_noise_1min_16kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_increasing_noise_rnnoise.wav");
	int rate_Hz = 16000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "rnnoise";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_increasing_noise_48kHz_mswebrtcns(void) {
	char *noise_file = bc_tester_res("sounds/echo_8_linux_inscreasing_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_increasing_noise_48kHz_mswebrtcns.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "webrtcns";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_increasing_noise_48kHz_rnnoise(void) {
	char *noise_file = bc_tester_res("sounds/echo_8_linux_inscreasing_noise_1min_48kHz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_increasing_noise_48kHz_rnnoise.wav");
	int rate_Hz = 48000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 0;
	float noise_gain = 0.5;
	char *model = "rnnoise";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_strong_increasing_noise_mswebrtcns(void) {
	char *noise_file = bc_tester_res("sounds/increasing_noise_16000Hz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_strong_increasing_noise_mswebrtcns.wav");
	int rate_Hz = 16000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 45000;
	float noise_gain = 1.;
	char *model = "webrtcns";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static void talk_with_strong_increasing_noise_rnnoise(void) {
	char *noise_file = bc_tester_res("sounds/increasing_noise_16000Hz.wav");
	char *output_file = bc_tester_file("output_audio_talk_with_strong_increasing_noise_rnnoise.wav");
	int rate_Hz = 16000;
	float play_duration_ms = PLAY_DURATION_MS;
	int start_noise_ms = 45000;
	float noise_gain = 1.;
	char *model = "rnnoise";
	play_noisy_audio(model, noise_file, output_file, rate_Hz, play_duration_ms, start_noise_ms, noise_gain);
	bctbx_free(noise_file);
}

static test_t tests[] = {
    TEST_NO_TAG("Talk with very slight noise 48kHz, MSWebRTCNS", talk_with_very_slight_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with very slight noise 48kHz, RNNoise", talk_with_very_slight_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with slight noise 48kHz, MSWebRTCNS", talk_with_slight_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with slight noise 48kHz, RNNoise", talk_with_slight_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with medium noise, MSWebRTCNS", talk_with_medium_noise_mswebrtcns),
    TEST_NO_TAG("Talk with medium noise, RNNoise", talk_with_medium_noise_rnnoise),
    TEST_NO_TAG("Talk with medium noise 48kHz, MSWebRTCNS", talk_with_medium_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with medium noise 48kHz, RNNoise", talk_with_medium_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with increasing noise, MSWebRTCNS", talk_with_increasing_noise_mswebrtcns),
    TEST_NO_TAG("Talk with increasing noise, RNNoise", talk_with_increasing_noise_rnnoise),
    TEST_NO_TAG("Talk with increasing noise 48kHz, MSWebRTCNS", talk_with_increasing_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with increasing noise 48kHz, RNNoise", talk_with_increasing_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with strong increasing noise, MSWebRTCNS", talk_with_strong_increasing_noise_mswebrtcns),
    TEST_NO_TAG("Talk with strong increasing noise, RNNoise", talk_with_strong_increasing_noise_rnnoise),
};

test_suite_t noise_suppression_in_audio_test_suite = {"Noise suppression in audio",
                                                      tester_before_all,
                                                      tester_after_all,
                                                      NULL,
                                                      NULL,
                                                      sizeof(tests) / sizeof(tests[0]),
                                                      tests,
                                                      0};
