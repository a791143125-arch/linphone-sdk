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

#ifdef ENABLE_WEBRTC_AEC
extern void libmswebrtc_init(MSFactory *factory);
#endif

// FIXME FHA
#define PLAY_DURATION_MS 21000

#define NS_DUMP 1

typedef struct _denoising_test_config {
	char *speech_file;
	char *noise_file;
	char *clean_file;
	char *noisy_speech_file;
	int sample_rate_Hz;
	float play_duration_ms;
	int start_noise_ms;
	float noise_gain;
} denoising_test_config;

static void init_denoising_test_config(denoising_test_config *config) {
	config->speech_file = NULL;
	config->noise_file = NULL;
	config->clean_file = NULL;
	config->noisy_speech_file = NULL;
	config->sample_rate_Hz = 48000;
	config->play_duration_ms = PLAY_DURATION_MS;
	config->start_noise_ms = 0;
	config->noise_gain = 0.1;
}

static void uninit_denoising_test_config(denoising_test_config *config) {
	if (config->speech_file) ms_free(config->speech_file);
	if (config->noise_file) ms_free(config->noise_file);
#if NS_DUMP != 1
	if (config->clean_file) unlink(config->clean_file);
	if (config->noisy_speech_file) unlink(config->noisy_speech_file);
#endif
	if (config->clean_file) ms_free(config->clean_file);
	if (config->noisy_speech_file) ms_free(config->noisy_speech_file);
	config->speech_file = NULL;
	config->noise_file = NULL;
	config->clean_file = NULL;
	config->noisy_speech_file = NULL;
}

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

static void play_and_denoise_audio(char *model, denoising_test_config *config) {

#ifdef NS_DUMP
	unlink(config->clean_file);
	unlink(config->noisy_speech_file);
#endif

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
	if (!noise_test_create_player(&player_talk, config->speech_file)) {
		BC_FAIL("cannot create talk player");
		goto end;
	}
	int signal_input_rate = 0;
	int nchannels_input = 0;
	ms_filter_call_method(player_talk, MS_FILTER_GET_SAMPLE_RATE, &signal_input_rate);
	ms_filter_call_method(player_talk, MS_FILTER_GET_NCHANNELS, &nchannels_input);
	ms_message("audio parameters read are: sample rate = %d Hz, mono/stereo = %d", signal_input_rate, nchannels_input);

	// noise
	if (!noise_test_create_player(&player_noise, config->noise_file)) {
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
	gainctl.param.gain = config->noise_gain;
	ms_filter_call_method(mixer, MS_AUDIO_MIXER_SET_INPUT_GAIN, &gainctl);
	gainctl.pin = 1;
	gainctl.param.gain = 1.;
	ms_filter_call_method(mixer, MS_AUDIO_MIXER_SET_INPUT_GAIN, &gainctl);
	ms_message("Noise added with gain = %f", config->noise_gain);

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
	ms_filter_call_method(sound_rec, MS_FILE_REC_OPEN, config->clean_file);

	sound_rec_with_noise = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(sound_rec_with_noise, MS_FILTER_SET_SAMPLE_RATE, &ns_sample_rate);
	ms_filter_call_method(sound_rec_with_noise, MS_FILTER_SET_NCHANNELS, &ns_nchannels);
	ms_filter_call_method_noarg(sound_rec_with_noise, MS_FILE_REC_CLOSE);
	ms_filter_call_method(sound_rec_with_noise, MS_FILE_REC_OPEN, config->noisy_speech_file);

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
	if (config->start_noise_ms > 0) ms_filter_call_method(player_noise, MS_PLAYER_SEEK_MS, &config->start_noise_ms);
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
	while (audio_done != 1 && elapsed < config->play_duration_ms) {
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
}

static void check_audio_quality(char *clean_file,
                                char *reference_file,
                                char *noisy_file,
                                int start_comparison_ms,
                                float energy_threshold,
                                float similarity_threshold) {
	double similar = 0.;
	double energy = 0.;
	MSAudioDiffParams audio_cmp_params;
	audio_cmp_params.chunk_size_ms = 0;
	audio_cmp_params.max_shift_percent = 5;
	ms_message("*** compare clean audio with reference ***");
	ms_message("compare %s", clean_file);
	ms_message("compare %s", reference_file);
	ms_message("Try to align output on reference by computing cross correlation with a maximal shift of %d percent",
	           audio_cmp_params.max_shift_percent);
	BC_ASSERT_EQUAL(ms_audio_compare_silence_and_speech(reference_file, clean_file, &similar, &energy,
	                                                    &audio_cmp_params, NULL, NULL, 500, 1500, start_comparison_ms),
	                0, int, "%d");
	ms_message("energy in silence = %f - max = %f", energy, energy_threshold);
	ms_message("similarity in talk = %f - min = %f", similar, similarity_threshold);
	BC_ASSERT_GREATER(similar, similarity_threshold, double, "%f");
	BC_ASSERT_LOWER(similar, 1.0, double, "%f");
	BC_ASSERT_LOWER(energy, energy_threshold, double, "%f");

	float k = 0.5;
	ms_message("*** check that noise is reduced by at least %f ***", k);
	ms_message("compare %s", noisy_file);
	ms_message("compare %s", reference_file);
	double similar_noisy = 0.;
	double energy_noisy = 0.;
	BC_ASSERT_EQUAL(ms_audio_compare_silence_and_speech(reference_file, noisy_file, &similar_noisy, &energy_noisy,
	                                                    &audio_cmp_params, NULL, NULL, 500, 1500, start_comparison_ms),
	                0, int, "%d");
	ms_message("energy in silence of noisy audio = %f, -> %f for comparison", energy_noisy,
	           energy_noisy - k * energy_noisy);
	BC_ASSERT_LOWER_STRICT(energy, energy_noisy - k * energy_noisy, double, "%f");
}

static void talk_with_very_slight_noise_48kHz_mswebrtcns(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_very_slight_noise_48kHz_mswebrtcns.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_very_slight_noise_48kHz_mswebrtcns.wav");
	config.noise_gain = 0.1;
	char *model = "webrtcns";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.5,
	                    0.97);
	uninit_denoising_test_config(&config);
}

static void talk_with_very_slight_noise_48kHz_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_very_slight_noise_48kHz_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_very_slight_noise_48kHz_rnnoise.wav");
	config.noise_gain = 0.1;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.05,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_real_noise_48kHz_1_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_real_noise_48kHz_1_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_real_noise_48kHz_1_rnnoise.wav");
	config.noise_gain = 0.3;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.05,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_real_noise_48kHz_2_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_real_noise_48kHz_2_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_real_noise_48kHz_2_rnnoise.wav");
	config.noise_gain = 0.4;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.05,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_real_noise_48kHz_1_mswebrtcns(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_real_noise_48kHz_1_mswebrtcns.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_real_noise_48kHz_1_mswebrtcns.wav");
	config.noise_gain = 0.3;
	char *model = "webrtcns";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.05,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_real_noise_48kHz_2_mswebrtcns(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_real_noise_48kHz_2_mswebrtcns.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_real_noise_48kHz_2_mswebrtcns.wav");
	config.noise_gain = 0.4;
	char *model = "webrtcns";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.05,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_white_noise_48kHz_1_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/white_noise_48000Hz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_white_noise_48kHz_1_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_white_noise_48kHz_1_rnnoise.wav");
	config.noise_gain = 0.1;
	config.play_duration_ms = 10000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.01,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_white_noise_48kHz_5_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/white_noise_48000Hz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_white_noise_48kHz_5_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_white_noise_48kHz_5_rnnoise.wav");
	config.noise_gain = 0.2;
	config.play_duration_ms = 10000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.01,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_white_noise_48kHz_6_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/white_noise_48000Hz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_white_noise_48kHz_6_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_white_noise_48kHz_6_rnnoise.wav");
	config.noise_gain = 0.3;
	config.play_duration_ms = 10000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.01,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_white_noise_48kHz_7_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/white_noise_48000Hz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_white_noise_48kHz_7_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_white_noise_48kHz_7_rnnoise.wav");
	config.noise_gain = 0.4;
	config.play_duration_ms = 10000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.01,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_white_noise_48kHz_2_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/white_noise_48000Hz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_white_noise_48kHz_2_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_white_noise_48kHz_2_rnnoise.wav");
	config.noise_gain = 0.5;
	config.play_duration_ms = 10000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.01,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_white_noise_48kHz_3_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/white_noise_48000Hz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_white_noise_48kHz_3_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_white_noise_48kHz_3_rnnoise.wav");
	config.noise_gain = 1.;
	config.play_duration_ms = 10000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.01,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_white_noise_48kHz_4_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/white_noise_48000Hz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_white_noise_48kHz_4_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_white_noise_48kHz_4_rnnoise.wav");
	config.noise_gain = 1.5;
	config.play_duration_ms = 10000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.01,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_slight_noise_48kHz_mswebrtcns(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_slight_noise_48kHz_mswebrtcns.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_slight_noise_48kHz_mswebrtcns.wav");
	config.noise_gain = 0.2;
	char *model = "webrtcns";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 2., 0.95);
	uninit_denoising_test_config(&config);
}

static void talk_with_slight_noise_48kHz_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_slight_noise_48kHz_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_slight_noise_48kHz_rnnoise.wav");
	config.noise_gain = 0.2;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.03,
	                    0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_medium_noise_48kHz_mswebrtcns(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_medium_noise_48kHz_mswebrtcns.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_medium_noise_48kHz_mswebrtcns.wav");
	config.noise_gain = 0.5;
	char *model = "webrtcns";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 12.5,
	                    0.87);
	uninit_denoising_test_config(&config);
}

static void talk_with_medium_noise_48kHz_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_4_linux_medium_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_medium_noise_48kHz_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_medium_noise_48kHz_rnnoise.wav");
	config.noise_gain = 0.5;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	int start_comparison_ms = config.play_duration_ms - 4000;
	check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.02,
	                    0.96);
	uninit_denoising_test_config(&config);
}

static void talk_with_increasing_noise_48kHz_mswebrtcns(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_8_linux_inscreasing_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_increasing_noise_48kHz_mswebrtcns.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_increasing_noise_48kHz_mswebrtcns.wav");
	config.noise_gain = 0.5;
	config.play_duration_ms = 60000;
	char *model = "webrtcns";
	play_and_denoise_audio(model, &config);
	// int start_comparison_ms = config.play_duration_ms - 4000;
	// check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.2,
	//                     0.98);
	uninit_denoising_test_config(&config);
}

static void talk_with_increasing_noise_48kHz_rnnoise(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res("sounds/farend_simple_talk_48kHz.wav");
	config.noise_file = bc_tester_res("sounds/echo_8_linux_inscreasing_noise_1min_48kHz.wav");
	config.clean_file = bc_tester_file("clean_audio_talk_with_increasing_noise_48kHz_rnnoise.wav");
	config.noisy_speech_file = bc_tester_file("noisy_audio_talk_with_increasing_noise_48kHz_rnnoise.wav");
	config.noise_gain = 0.5;
	config.play_duration_ms = 60000;
	char *model = "rnnoise";
	play_and_denoise_audio(model, &config);
	// int start_comparison_ms = config.play_duration_ms - 4000;
	// check_audio_quality(config.clean_file, config.speech_file, config.noisy_speech_file, start_comparison_ms, 0.2,
	//                     0.98);
	uninit_denoising_test_config(&config);
}

static test_t tests[] = {
    TEST_NO_TAG("Talk with very slight noise 48kHz, MSWebRTCNS", talk_with_very_slight_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with very slight noise 48kHz, RNNoise", talk_with_very_slight_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with slight noise 48kHz, MSWebRTCNS", talk_with_slight_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with slight noise 48kHz, RNNoise", talk_with_slight_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with medium noise 48kHz, MSWebRTCNS", talk_with_medium_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with medium noise 48kHz, RNNoise", talk_with_medium_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with increasing noise 48kHz, MSWebRTCNS", talk_with_increasing_noise_48kHz_mswebrtcns),
    TEST_NO_TAG("Talk with increasing noise 48kHz, RNNoise", talk_with_increasing_noise_48kHz_rnnoise),
    TEST_NO_TAG("Talk with white noise 48kHz 1, RNNoise", talk_with_white_noise_48kHz_1_rnnoise),
    TEST_NO_TAG("Talk with white noise 48kHz 2, RNNoise", talk_with_white_noise_48kHz_2_rnnoise),
    TEST_NO_TAG("Talk with white noise 48kHz 3, RNNoise", talk_with_white_noise_48kHz_3_rnnoise),
    TEST_NO_TAG("Talk with white noise 48kHz 4, RNNoise", talk_with_white_noise_48kHz_4_rnnoise),
    TEST_NO_TAG("Talk with white noise 48kHz 5, RNNoise", talk_with_white_noise_48kHz_5_rnnoise),
    TEST_NO_TAG("Talk with white noise 48kHz 6, RNNoise", talk_with_white_noise_48kHz_6_rnnoise),
    TEST_NO_TAG("Talk with white noise 48kHz 7, RNNoise", talk_with_white_noise_48kHz_7_rnnoise),
    TEST_NO_TAG("Talk with real noise 48kHz 1, RNNoise", talk_with_real_noise_48kHz_1_rnnoise),
    TEST_NO_TAG("Talk with real noise 48kHz 2, RNNoise", talk_with_real_noise_48kHz_2_rnnoise),
    TEST_NO_TAG("Talk with real noise 48kHz 1, MSWebRTCNS", talk_with_real_noise_48kHz_1_mswebrtcns),
    TEST_NO_TAG("Talk with real noise 48kHz 2, MSWebRTCNS", talk_with_real_noise_48kHz_2_mswebrtcns),
};

test_suite_t noise_suppression_in_audio_test_suite = {"Noise suppression in audio",
                                                      tester_before_all,
                                                      tester_after_all,
                                                      NULL,
                                                      NULL,
                                                      sizeof(tests) / sizeof(tests[0]),
                                                      tests,
                                                      0};
