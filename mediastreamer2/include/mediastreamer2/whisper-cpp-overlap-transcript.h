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

#ifndef whisper_cpp_overlap_transcript_h
#define whisper_cpp_overlap_transcript_h

#include "bctoolbox/tester.h"
#include <bctoolbox/defs.h>
#include <mediastreamer2/abstract-transcript.h>
#include <mediastreamer2/msfilter.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mstranscript.h"
#include <cmath>
#include <vector>
#include <whisper.h>

struct Token {
	std::string token;
	float timestamp;
};

/** Class heriting from AbstractTranscript implementing transcription with wispercpp according to the overlap strategy.
 * Transcribes chunks of audio with a bit of overlaping audio between each chunk to avoid having trucanted words on
 * chunks boundaries. */
class WhisperCPPOverlapTranscript : public AbstractTranscript {

public:
	/**
	 * @brief initialise the transcription model and thread.
	 * @param f Associated MSFilter (MSTranscript here).
	 */
	int init(MSFilter *f) override;

	/**
	 * @brief	Deals with the transcription. A transcription is launched on a thread when enough audio is gathered
	 * (kOverlapDuration s).
	 * @param f Associated MSFilter (MSTranscript here).
	 * @return
	 */
	std::vector<MSTranscription> process(MSFilter *f) override;

	/**
	 * @brief	Deals with the transcription of the end of the audio, we transcribe all the remaining audio even if we
	 * gathered less than kOverlapDuration s.
	 * @param f Associated MSFilter (MSTranscript here).
	 * @return
	 */
	std::vector<MSTranscription> postProcess(MSFilter *f) override;

	/**
	 * @brief	Frees the model and thread.
	 * @param f Associated MSFilter (MSTranscript here).
	 */
	void uninit(MSFilter *f) override;

	/**
	 * @brief Constructor of WhisperCPPOverlapTranscript
	 * @param modelPath Path to the transcription model
	 * @param samplerate Samplerate used for the model
	 */
	WhisperCPPOverlapTranscript(std::string modelPath, int samplerate);

	/**
	 * @brief Constructor of WhisperCPPOverlapTranscript
	 * @param modelPath Path to the transcription model
	 * @param samplerate Samplerate used for the model
	 * @param chunkDuration Duration of the audio chunk transcribed.
	 * @param overlapDuration Duration of the overlap between transcribed chunks.
	 */
	WhisperCPPOverlapTranscript(std::string modelPath, int samplerate, float chunkDuration, float overlapDuration);

	~WhisperCPPOverlapTranscript();

private:
	/**
	 * @brief prepareChunkAndOverlap If there is enough audio, prepare the audio chunk from the mBuf to an exloitable
	 * form for whispercpp according to the overlap strategy of transcription.
	 * @return Return a vector<float> ready to be used in whisper_full if there was enough audio in mBuf, return an
	 * empty vector if there was not enough audio stored.
	 */
	std::vector<float> prepareChunkAndOverlap();

	/**
	 * @brief Get the token transcribed by whispercpp and the timestamp and store them in a vector.
	 * @return A vector containing the tokens transcribed by whispercpp and their associated timestamp.
	 */
	std::vector<Token> getTokens();

	/**
	 * @brief tokensIntoWords Whisper cpp return transcription in the form of tokens (fragment of words or special
	 * tokens). This method transform the tokens into words and remove the special tokens.
	 * @param tokenList List of the tokens (token and associated timestamp) transcribed by the whispercpp model.
	 * @return Return a vector of Transcription (a struct containing the word transcribed (char*) associated to its
	 * timestamp (double)).
	 */
	std::vector<MSTranscription> tokensIntoWords(std::vector<Token> tokenList);

	/**
	 * @brief selectWordsToPrint The overlaping strategy lead to the model transcribing the overlaping audio data
	 * multiple times, leading to duplicated words. This method chose which words to keep from all the transcribed words
	 * to avoid this problem and store them in mVectorTranscription.
	 * @param currentWords Vector of Transcription containing all the transcribed words.
	 */
	void selectWordsToPrint(std::vector<MSTranscription> currentWords);

	/**
	 * @brief transcriptProcessAsync Method containing the part of the transcription process that takes a lot of
	 * resources that we put in a separate thread for parallelization.
	 * @return Return true if everything went well, -1 else.
	 */
	bool_t transcriptProcessAsync();

	/**
	 * @brief asyncWrapper Workaround to be able to use a class method associated to the instance of the class in
	 * ms_worker_thread_add_waitable_task.
	 * @param data Containt a pointer the the instance of this class.
	 * @return 0 if everything went well, -1 otherwise.
	 */
	static bool_t asyncWrapper(void *data);

	MSBufferizer *mBuf = NULL;    /** Buffer containing raw audio from the packets. */
	whisper_context *mCtx = NULL; /** whispercpp use this object to store the parameters and the transcription */
	whisper_full_params mParams;  /** Params used for transcription in whispercpp. */
	/** Audio for overlap: end of the chunk of the last iteration and beginning  of the current one. */
	std::vector<float> mOverlap;
	std::string mLastValidatedWord = ""; /** Last validated word at the last iteration, used in selectWordsToPrint. */
	float mLastTime = -1;                /** Timestamp of the last validated word, useful in selectWordsToPrint. */
	float mPreviousEnd = 0;       /** Timestamp from which we start validating words (if everything goes right...) */
	const float kChunkDuration;   /** Duration of the audio chunk (in s). */
	const float kOverlapDuration; /** Duration of the overlap (in s). */
	std::vector<MSTranscription> mFinalWords; /** List of words that were not validated last iteration. */
	bool mStartOfTranscription = true;        /** Booleen at true if the audio just started. */
	float mGlobalOffset = 0.0f;               /** Offset to be added to the timestamps generated by whisper (in s). */
	bool mEndOfAudio = false;                 /** Booleen at true if we reached the end of the audio. */
	std::string mLastDiscardedWord;           /** First word that was discarded in the previous loop. */
	std::vector<float> mAudioF32;             /** Vector containing the audio to be processed by whisper_full. */
	MSTask *mTask = NULL;                     /** Task currently running.*/
	MSWorkerThread *wth = NULL;               /** Worker thread containing the transcription tasks.*/

	friend class whisperOverlapTester; /** This class is to be used only for tests */
};

#endif