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

#include "mediastreamer2/abstract-transcript.h"
#include <algorithm>

std::vector<float> AbstractTranscript::readAndFormatAudioBufferForTranscription(MSBufferizer *buffer, size_t readSize) {
	std::vector<float> audioF32;
	std::vector<uint8_t> buf_int8(readSize);
	ms_bufferizer_read(buffer, buf_int8.data(), readSize);
	if (readSize % sizeof(int16_t) != 0) {
		ms_error("transcription: wrong data size.");
		return audioF32;
	}
	int16_t *samples = reinterpret_cast<int16_t *>(buf_int8.data());
	audioF32.resize(readSize / sizeof(int16_t));
	for (size_t i = 0; i < audioF32.size(); ++i) {
		audioF32[i] = samples[i] / 32768.0f; // normalization
	}
	return audioF32;
}

void AbstractTranscript::clean_word(std::string &word) {
	std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return std::tolower(c); });
	const std::string punct = ",.?;\"!";
	word.erase(std::remove_if(word.begin(), word.end(), [&](char c) { return punct.find(c) != std::string::npos; }),
	           word.end());
};

AbstractTranscript::AbstractTranscript() {
	mModelPath = std::string(MODEL_PATH) + "/ggml-" + std::string(MODEL_NAME) + ".bin";
	mSampleRate = 16000;
}

AbstractTranscript::AbstractTranscript(std::string path, int samplerate) {
	mModelPath = path;
	mSampleRate = samplerate;
}

void AbstractTranscript::set_model_path(char *path) {
	mModelPath = std::string(path);
}

const char *AbstractTranscript::get_model_path() {
	return mModelPath.c_str();
}

void AbstractTranscript::setFileDuration(int fileDuration) {
	mFileDurationMs = fileDuration;
}

AbstractTranscript::~AbstractTranscript() = default;
