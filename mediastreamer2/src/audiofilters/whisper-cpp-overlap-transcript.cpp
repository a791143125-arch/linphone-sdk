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

#include "mediastreamer2/whisper-cpp-overlap-transcript.h"
#include "mediastreamer2/abstract-transcript.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/mstranscript.h"

WhisperCPPOverlapTranscript::WhisperCPPOverlapTranscript(std::string modelPath, int samplerate)
    : AbstractTranscript(modelPath, samplerate), kChunkDuration(3.0f), kOverlapDuration(1.0f) {
}

WhisperCPPOverlapTranscript::WhisperCPPOverlapTranscript(std::string modelPath,
                                                         int samplerate,
                                                         float chunkDuration,
                                                         float overlapDuration)
    : AbstractTranscript(modelPath, samplerate), kChunkDuration(chunkDuration), kOverlapDuration(overlapDuration) {
}

std::vector<float> WhisperCPPOverlapTranscript::prepareChunkAndOverlap() {
	std::vector<float> audioF32;

	if (mOverlap.empty() && mBuf->size >= uint(kOverlapDuration * mSampleRate) * 2) {
		size_t readSize = int(mSampleRate * kOverlapDuration) * 2;
		std::vector<float> audio = readAndFormatAudioBufferForTranscription(mBuf, readSize);

		mOverlap.assign(std::begin(audio), std::end(audio));
	}

	// If there is an mOverlap, we take enough audio to complete the chunk
	else if ((mBuf->size >= (uint(mSampleRate * kChunkDuration) - uint(mSampleRate * kOverlapDuration)) * 2 &&
	          !mOverlap.empty()) ||
	         mEndOfAudio) {

		// If we are at the end of the audio, we take the remaining data even if we do not reach CHUNK_DURATION to not
		// lose words
		if (mEndOfAudio) {
			size_t readSize = mBuf->size;
			audioF32 = readAndFormatAudioBufferForTranscription(mBuf, readSize);
		}

		else {
			size_t readSize = (int(mSampleRate * kChunkDuration) - int(mSampleRate * kOverlapDuration)) * 2;
			audioF32 = readAndFormatAudioBufferForTranscription(mBuf, readSize);
		}

		size_t newAudioSize = audioF32.size();

		audioF32.insert(std::begin(audioF32), std::begin(mOverlap), std::end(mOverlap));

		mOverlap.clear();
		mOverlap.assign(std::begin(audioF32) + newAudioSize, std::end(audioF32));
	}
	return audioF32;
};

std::vector<Token> WhisperCPPOverlapTranscript::getTokens() {
	std::vector<Token> vectorOfTokens;
	int nSegments = whisper_full_n_segments(mCtx);
	for (int i = 0; i < nSegments; i++) {
		int nTokens = whisper_full_n_tokens(mCtx, i);
		for (int j = 0; j < nTokens; ++j) {
			const auto tokenData = whisper_full_get_token_data(mCtx, i, j);
			std::string word = whisper_token_to_str(mCtx, tokenData.id);
			float dtw = float(tokenData.t_dtw) / 100 + mGlobalOffset;
			vectorOfTokens.push_back({word, dtw});
		}
	}
	return vectorOfTokens;
}

std::vector<MSTranscription> WhisperCPPOverlapTranscript::tokensIntoWords(std::vector<Token> tokenList) {
	std::vector<MSTranscription> currentWords;
	float endWord = -1;
	std::string completeWord = "";
	bool_t end_of_sentence = false;
	for (Token token : tokenList) {
		std::string word = token.token;
		float dtw = token.timestamp;

		MSTranscription transcription = default_transcription_object();
		strncpy(transcription.transcribed_word, completeWord.c_str(), sizeof(transcription.transcribed_word));
		transcription.transcribed_word[sizeof(transcription.transcribed_word) - 1] = '\0'; // Ensure null-termination
		transcription.timestamp = endWord;
		transcription.end_of_sentence = end_of_sentence;
		end_of_sentence = false;
		transcription.is_final = true;
		// If the token starts with a space, we consider the previous word finished and start a new one. We also
		// keep the timestamp of the last "word" token for the word and not the timestamp associated to its
		// puncuation.
		if (word[0] == ' ') {
			if (transcription.timestamp != -1) currentWords.push_back(transcription);
			word.erase(0, 1);
			completeWord = word;
			endWord = dtw;

			// If we encounter a special token, we finish the previous word. We ignore the special token.
		} else if ((word[0] == '[' || word[0] == '_')) {
			if (transcription.timestamp != -1) currentWords.push_back(transcription);
			completeWord = "";
			endWord = -1;

			// If we have neither a space nor a special token, we add the current token to the unfinished word
			// or start a new one if the word was not started.
		} else if (!(word[0] == '[' || word[0] == '_')) {
			if (!ispunct(word[0])) {
				endWord = dtw;
			} else {
				end_of_sentence = true;
			}
			if (completeWord == "") {
				completeWord = word;
			} else completeWord += word;
		}
	}
	return currentWords;
};

void WhisperCPPOverlapTranscript::selectWordsToPrint(std::vector<MSTranscription> currentWords) {
	// time from which we stop validating the transcriptions (too close to the border of a chunk)
	// As I observed that the words near the end of the chunk usually have a better transcription than the words at the
	// begening of it, I chose to validate words closer to the end of the chunk and thus, further from the begenning of
	// the next (the kOverlapDuration/3).
	float timeToEnd = mGlobalOffset + kChunkDuration - kOverlapDuration / 3;
	bool printFromNow = false;
	mLastDiscardedWord = "";
	if (!mFinalWords.empty()) mLastDiscardedWord = (mFinalWords)[0].transcribed_word;
	mFinalWords.clear();
	clean_word(mLastValidatedWord); // we will compare this last_word with current words, we clean it to compare them
	                                // independently of case and punctuation.

	for (MSTranscription transcription : currentWords) {
		std::string text = transcription.transcribed_word;
		clean_word(mLastDiscardedWord);
		clean_word(text);

		// If it is a new audio or we find the last validated word near the left border of the chunk, we validate
		// the following words even if they are still close to the border
		if (((text == mLastValidatedWord) && transcription.timestamp < mPreviousEnd)) {
			printFromNow = true;
		}

		// lot of conditions that prevent duplications and disapearing words. A lot of these conditions are
		// necessary because of variations between timstamps for the same word or variation between transcribed
		// words with the "same" timestamp between iterations
		if ((transcription.timestamp >= mPreviousEnd || printFromNow || text == mLastDiscardedWord) &&
		    (transcription.timestamp < timeToEnd || mEndOfAudio) && text != mLastValidatedWord &&
		    transcription.timestamp > mLastTime) {

			mLastValidatedWord = transcription.transcribed_word;
			mLastTime = transcription.timestamp;
			if (transcription.timestamp < float(mFileDurationMs) / 1000 || mFileDurationMs < 0) {
				mVectorTranscription.push_back(transcription);
			} else {
				ms_message("Transcription hallucination at the end of the file : %s", transcription.transcribed_word);
			}
		}

		// we store the discarded words for the next iteration
		if (transcription.timestamp >= timeToEnd) {
			mFinalWords.push_back(transcription);
		}
	}

	mPreviousEnd = timeToEnd;
};

int WhisperCPPOverlapTranscript::init(BCTBX_UNUSED(MSFilter *f)) {
	whisper_context_params cparams = whisper_context_default_params();

	cparams.dtw_aheads_preset = WHISPER_AHEADS_BASE_EN;

	cparams.dtw_token_timestamps = true;

	whisper_context *ctx_init = whisper_init_from_file_with_params(mModelPath.c_str(), cparams);
	if (!ctx_init) {
		std::cerr << "Failed to load Whisper model.\n";
		ms_error("Failed to load Whisper model. Make sure you have the right model path.");
		return -1;
	}
	mCtx = ctx_init;

	mParams = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
	mParams.beam_search.beam_size = 5;
	mParams.print_special = false;
	mParams.print_progress = false;
	mParams.print_realtime = false;
	mParams.print_timestamps = false;
	mParams.suppress_nst = true;
	std::cout << "Running whisper with " << mParams.n_threads << " threads.\n";

	wth = ms_worker_thread_new("Whisper Full");
	return 0;
}

bool_t WhisperCPPOverlapTranscript::transcriptProcessAsync() {

	if (whisper_full(mCtx, mParams, mAudioF32.data(), mAudioF32.size()) != 0) {
		std::cerr << "Whisper failed on chunk.\n";
		ms_error("Whisper failed on chunck.");
		return -1;
	}
	// mAudioF32.clear();

	std::string text = "";
	if (!mStartOfTranscription) text = whisper_full_get_segment_text(mCtx, 0);

	// If we have a blank audio, we display all the words not displayed from the previous iteration.
	if (text == " [BLANK_AUDIO]") {
		for (MSTranscription transcription : mFinalWords) {
			mLastValidatedWord = transcription.transcribed_word;
			mLastTime = transcription.timestamp;
			mVectorTranscription.push_back(transcription);
		}
	}

	std::vector<MSTranscription> currentWords;
	currentWords = tokensIntoWords(getTokens());

	selectWordsToPrint(currentWords);

	if (!currentWords.empty()) mStartOfTranscription = false;
	mGlobalOffset += (kChunkDuration - kOverlapDuration);

	return true;
}

bool_t WhisperCPPOverlapTranscript::asyncWrapper(void *data) {
	auto *self = static_cast<WhisperCPPOverlapTranscript *>(data);
	return self->transcriptProcessAsync();
}

std::vector<MSTranscription> WhisperCPPOverlapTranscript::process(MSFilter *f) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	mBuf = transcript->buf;
	// If there is no mOverlap stored, we take kOverlapDuration audio and store it in the mOverlap buffer

	std::vector<float> audioF32 = prepareChunkAndOverlap();
	if (!audioF32.empty()) {

		if (mTask) {
			ms_task_wait_completion(mTask);
			ms_free(mTask);
		}
		mAudioF32 = audioF32;
		mTask = ms_worker_thread_add_waitable_task(wth, asyncWrapper, this);
		if (mEndOfAudio) ms_task_wait_completion(mTask);
		audioF32.clear();
	}
	std::vector<MSTranscription> ret;
	if (!mVectorTranscription.empty()) {
		ret = mVectorTranscription;
		mVectorTranscription.clear();
	}
	return ret;
}

std::vector<MSTranscription> WhisperCPPOverlapTranscript::postProcess(MSFilter *f) {
	if (mTask) ms_task_wait_completion(mTask);
	mEndOfAudio = true;
	return WhisperCPPOverlapTranscript::process(f);
}

void WhisperCPPOverlapTranscript::uninit(MSFilter *f) {
	if (wth) ms_worker_thread_destroy(wth, true);
	if (mCtx) whisper_free(mCtx);
	if (mTask) ms_free(mTask);
	f->data = nullptr;
}

WhisperCPPOverlapTranscript::~WhisperCPPOverlapTranscript() {
}