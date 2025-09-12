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

#include "mediastreamer2/vosk-transcript.h"
#include "jsoncpp/json/json.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/mstranscript.h"
#include <cstdint>
#include <string>
#include <unistd.h>
#include <vosk_api.h>

VoskTranscript::VoskTranscript(std::string modelPath, int samplerate) : AbstractTranscript(modelPath, samplerate) {
}

VoskTranscript::~VoskTranscript() {
}

int VoskTranscript::init(BCTBX_UNUSED(MSFilter *f)) {
	mModel = vosk_model_new(mModelPath.c_str());
	if (!mModel) {
		ms_error("Failed to load model");
		return -1;
	}
	mRecognizer = vosk_recognizer_new(mModel, 16000.0);
	vosk_recognizer_set_words(mRecognizer, 1);
	vosk_recognizer_set_partial_words(mRecognizer, 1);
	if (!mRecognizer) {
		ms_error("Failed to create recognizer");
		return -1;
	}
	return 0;
}

std::vector<MSTranscription> VoskTranscript::jsonToMSTranscript(std::string sentence, bool_t isPartial) {
	std::vector<MSTranscription> currentIteration;

	for (size_t i = 1; i + 1 < sentence.size(); ++i) {
		if (isdigit(sentence[i - 1]) && sentence[i] == ',' && isdigit(sentence[i + 1])) {
			sentence[i] = '.';
		}
	}

	Json::CharReaderBuilder reader;
	Json::Value result;
	std::string errs;

	std::istringstream s(sentence);
	if (!Json::parseFromStream(reader, s, &result, &errs)) {
		throw std::runtime_error("Failed to parse JSON: " + errs);
	}
	const Json::Value &result_field = result[isPartial ? "partial_result" : "result"];
	for (const auto &item : result_field) {
		std::string word = item["word"].asString();
		float end = item["end"].asFloat();
		float start = item["start"].asFloat();
		float confidence = item["conf"].asFloat();

		MSTranscription transcription = default_transcription_object();
		strncpy(transcription.transcribed_word, word.c_str(), sizeof(transcription.transcribed_word));
		transcription.transcribed_word[sizeof(transcription.transcribed_word) - 1] = '\0';
		transcription.timestamp = end;
		transcription.beggining = start;
		transcription.confidence = confidence;
		currentIteration.push_back(transcription);
	}
	return currentIteration;
}

std::vector<MSTranscription> VoskTranscript::process(MSFilter *f) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	mBuf = transcript->buf;
	int final;
	std::vector<MSTranscription> res;

	size_t readSize = mBuf->size;

	if (float(readSize) * sizeof(char) / 16000 / 2 > 0.2) {
		std::vector<uint8_t> buf_int8(readSize);
		ms_bufferizer_read(mBuf, buf_int8.data(), readSize);
		char *samples = reinterpret_cast<char *>(buf_int8.data());
		final = vosk_recognizer_accept_waveform(mRecognizer, samples, buf_int8.size());
		std::vector<MSTranscription> currentIteration;
		if (final) {
			std::string sentence = std::string(vosk_recognizer_result(mRecognizer));

			currentIteration = jsonToMSTranscript(sentence, false);
		} else {
			std::string sentence = std::string(vosk_recognizer_partial_result(mRecognizer));

			currentIteration = jsonToMSTranscript(sentence, true);
		}

		for (uint8_t i = 0; i < currentIteration.size(); i++) {
			if (currentIteration[i].timestamp > mLastTime &&
			    (mLastWord != std::string(currentIteration[i].transcribed_word))) {
				res.push_back(currentIteration[i]);
				mLastTime = currentIteration[i].timestamp;
				mLastWord = std::string(currentIteration[i].transcribed_word);
			} else {
				currentIteration[i].correction = true;
				res.push_back(currentIteration[i]);
			}
		}
		currentIteration.clear();
	}
	return res;
}

std::vector<MSTranscription> VoskTranscript::postProcess(BCTBX_UNUSED(MSFilter *f)) {
	std::vector<MSTranscription> res;
	std::vector<MSTranscription> currentIteration;

	std::string sentence = std::string(vosk_recognizer_final_result(mRecognizer));

	currentIteration = jsonToMSTranscript(sentence, false);

	for (uint8_t i = 0; i < currentIteration.size(); i++) {
		if (currentIteration[i].timestamp > mLastTime) {
			if (mLastWord != std::string(currentIteration[i].transcribed_word)) {
				res.push_back(currentIteration[i]);
				mLastTime = currentIteration[i].timestamp;
				mLastWord = std::string(currentIteration[i].transcribed_word);
			}
		}
	}

	return res;
}

void VoskTranscript::uninit(BCTBX_UNUSED(MSFilter *f)) {
	vosk_recognizer_free(mRecognizer);
	vosk_model_free(mModel);
}