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
#include "linphone/api/c-transcription-cbs.h"
#include "linphone/api/c-transcription.h"
#include "linphone/api/c-types.h"
#include "linphone/core.h"
#include "private_functions.h"
#include <cstdint>
#include <cstring>
#include <stdlib.h>
#include <vector>

void on_transcription_display(BCTBX_UNUSED(LinphoneTranscription *transcription)) {
	uint32_t lastId = linphone_transcription_get_last_sentence_id(transcription);
	uint32_t id = 1;
	if (system("clear")) std::cout << "DIDNT CLEAR!" << std::endl;
	while (id <= lastId) {
		std::cout << "on_transcription_display ID : " << id << "  NAME : ["
		          << linphone_transcription_get_name_by_id(transcription, id) << "] "
		          << linphone_transcription_get_sentence_by_id(transcription, id) << std::endl;
		id++;
	}
}

int levenshtein_distance(std::vector<std::string> &ref, std::vector<std::string> &hyp) {

	if (ref.empty() || hyp.empty()) return -1;

	size_t m = ref.size() + 1;
	size_t n = hyp.size() + 1;
	std::vector<std::vector<int>> dp(m, std::vector<int>(n, 0));
	for (int i = 0; i < static_cast<int>(m); ++i)
		dp[i][0] = i;
	for (int j = 0; j < static_cast<int>(n); ++j)
		dp[0][j] = j;

	for (size_t i = 1; i < m; ++i) {
		for (size_t j = 1; j < n; ++j) {
			if (ref[i - 1] == hyp[j - 1]) {
				dp[i][j] = dp[i - 1][j - 1];
			} else {
				int del = dp[i - 1][j] + 1;
				int ins = dp[i][j - 1] + 1;
				int sub = dp[i - 1][j - 1] + 1;
				dp[i][j] = (del < ins) ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
			}
		}
	}
	return dp[m - 1][n - 1];
}

static std::vector<std::string> getWords(const std::string &input) {
	// lowercase without punctuation
	std::string result;
	std::copy_if(input.begin(), input.end(), std::back_inserter(result),
	             [](char c) { return std::isalnum(c) || std::isspace(c); });
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
	// split into words
	std::vector<std::string> words;
	std::istringstream iss(result);
	std::string word;
	while (iss >> word) {
		words.push_back(word);
	}
	return words;
}

float compute_wer(std::string &reference, std::string &hypothesis) {
	auto words_hyp = getWords(hypothesis);
	auto words_ref = getWords(reference);
	if (words_ref.empty()) {
		if (words_hyp.empty()) {
			ms_message("No text.");
			return 1.0f;
		} else {
			ms_message("transcription failed: no text expected but transcription is not empty.");
			return 0.f;
		}
	} else if (words_hyp.empty()) {
		ms_message("transcription failed: transcription is empty.");
		return 0.f;
	}

	int dist = levenshtein_distance(words_ref, words_hyp);
	ms_message("Levenshtein distance is %d", dist);
	if (dist > 0) {
		ms_message("reference was:");
		for (size_t i = 0; i < words_ref.size(); i++) {
			ms_message("%d - %s", (int)i, words_ref[i].c_str());
		}
		ms_message("transcription was:");
		for (size_t i = 0; i < words_hyp.size(); i++) {
			ms_message("%d - %s", (int)i, words_hyp[i].c_str());
		}
	}

	if (dist == -1) return 0.f;
	return static_cast<float>(dist) / static_cast<float>(words_ref.size());
}

static void check_transcription(LinphoneTranscription *crawfish_transcription, const char *method) {
	std::string expected_answer = "";
	float wer_lim = 0.;
	expected_answer =
	    "TAKE 30 CRAWFISH FROM WHICH REMOVE THE GUT CONTAINING THE GALL IN THE FOLLOWING MANNER TAKE FIRM "
	    "HOLD OF THE CRAWFISH WITH THE LEFT HAND SO AS TO AVOID BEING PINCHED BY ITS CLAWS WITH THE THUMB AND "
	    "FOREFINGER OF THE RIGHT HAND PINCH THE EXTREME END OF THE CENTRAL FIN OF THE TAIL AND WITH A SUDDEN "
	    "JERK THE GUT WILL BE WITHDRAWN";
	if (strcmp(method, "whispercpp_overlap") == 0) {
		wer_lim = 0.15; // threshold set for about 10 wrong
	} else if (strcmp(method, "vosk") == 0) {
		expected_answer =
		    expected_answer +
		    "TAKE THIRTY CRAWFISH"; // TODO stop playing audio at the end of the text. Handle lost words at start.
		wer_lim = 0.17;             // threshold set for about 11 wrong
	} else {
		BC_FAIL("Unknow transcription model");
		return;
	}

	std::string text = "";
	uint32_t lastId = linphone_transcription_get_last_sentence_id(crawfish_transcription);
	for (uint32_t i = 0; i < lastId; i++) {
		auto sentence = linphone_transcription_get_sentence_by_id(crawfish_transcription, i + 1);
		text = text + std::string(sentence);
	}
	float wer = compute_wer(expected_answer, text);
	ms_message("WER = %f", wer);
	BC_ASSERT_LOWER(wer, wer_lim, float, "%f");
}

static void call_transcription(const char *method, const char *model_path) {
	LinphoneCoreManager *marie;
	LinphoneCoreManager *pauline;
	char *speechfile = bc_tester_res("sounds/crawfish.wav");
	char *recordpath = bc_tester_file("transcription-record.wav");
	bool_t audio_cmp_failed = FALSE;

	LinphoneTranscription *transcription = nullptr;
	LinphoneTranscriptionCbs *transcription_cbs = nullptr;

	marie = linphone_core_manager_new("marie_rc");
	pauline = linphone_core_manager_new("pauline_rc");

	linphone_core_set_use_files(marie->lc, TRUE);
	linphone_core_set_play_file(marie->lc, speechfile);
	linphone_core_set_use_files(pauline->lc, TRUE);
	linphone_core_set_record_file(pauline->lc, recordpath);

	/*make sure the record file doesn't already exists, otherwise this test will append new samples to it*/
	unlink(recordpath);

	LinphonePayloadType *pt_marie;
	pt_marie = linphone_core_get_payload_type(marie->lc, "speex", 16000, 1);
	if (!pt_marie) {
		ms_warning("speex 16000 not available, skip test");
		goto end;
	}
	linphone_payload_type_unref(pt_marie);
	disable_all_audio_codecs_except_one(marie->lc, "speex", 16000);

	LinphonePayloadType *pt_pauline;
	pt_pauline = linphone_core_get_payload_type(pauline->lc, "speex", 16000, 1);
	if (!pt_pauline) {
		ms_warning("speex 16000 not available, skip test");
		goto end;
	}
	linphone_payload_type_unref(pt_pauline);
	disable_all_audio_codecs_except_one(pauline->lc, "speex", 16000);

	linphone_core_enable_transcription(pauline->lc, TRUE);
	linphone_core_set_transcription_method(pauline->lc, method);
	linphone_core_set_transcription_model_path(pauline->lc, model_path);

	if (!BC_ASSERT_TRUE(call(pauline, marie))) {
		goto end;
	} else {
		transcription = linphone_core_get_transcription(pauline->lc);
		BC_ASSERT_PTR_NOT_NULL(transcription);
		if (!transcription) {
			goto end;
		}

		transcription_cbs = linphone_factory_create_transcription_cbs(linphone_factory_get());
		linphone_transcription_cbs_set_transcription_display(transcription_cbs, on_transcription_display);
		linphone_transcription_set_user_data(transcription, nullptr);
		linphone_transcription_add_callbacks(transcription, transcription_cbs);
		_linphone_transcription_notify_result_to_display_available(transcription);
		linphone_transcription_cbs_unref(transcription_cbs);

		wait_for_until(marie->lc, pauline->lc, NULL, 0, 20000);
		end_call(pauline, marie);

		double similar;
		double min_threshold = .75f;
		double max_threshold = 1.f;
		BC_ASSERT_EQUAL(liblinphone_tester_audio_diff(speechfile, recordpath, &similar, &audio_cmp_params, NULL, NULL),
		                0, int, "%d");
		BC_ASSERT_GREATER(similar, min_threshold, double, "%g");
		BC_ASSERT_LOWER(similar, max_threshold, double, "%g");
		if (similar < min_threshold || similar > max_threshold) {
			audio_cmp_failed = TRUE;
		}

		if (!audio_cmp_failed) {
			unlink(recordpath);
		}

		check_transcription(transcription, method);
	}
end:
	if (transcription) linphone_transcription_unref(transcription);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	ms_free(speechfile);
	bc_free(recordpath);
}

static void transcribe_with_vosk(void) {
	const char *method = "vosk";
	const char *model_path = "vosk-model-en-us-0.22-lgraph";
	call_transcription(method, model_path);
}

static void transcribe_with_whispercpp_overlap(void) {
	const char *method = "whispercpp_overlap";
	const char *model_path = "linphone-sdk/desktop/whisper.cpp/models/ggml-base.en-q8_0.bin";
	call_transcription(method, model_path);
}

static test_t transcription_tests[] = {
    TEST_NO_TAG("Transcribe with vosk", transcribe_with_vosk),
    TEST_NO_TAG("Transcribe with whisper.cpp and overlap", transcribe_with_whispercpp_overlap)};

test_suite_t transcription_test_suite = {"Call with transcription",
                                         NULL,
                                         NULL,
                                         liblinphone_tester_before_each,
                                         liblinphone_tester_after_each,
                                         sizeof(transcription_tests) / sizeof(transcription_tests[0]),
                                         transcription_tests,
                                         0};
