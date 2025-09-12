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

#ifndef vosk_transcript_h
#define vosk_transcript_h

#include "mediastreamer2/mstranscript.h"
#include <bctoolbox/defs.h>
#include <cstddef>
#include <mediastreamer2/abstract-transcript.h>
#include <vosk_api.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

class VoskTranscript : public AbstractTranscript {

public:
	/**
	 * @brief initialise the transcription model.
	 * @param f Associated MSFilter (MSTranscript here).
	 */
	int init(MSFilter *f) override;

	/**
	 * @brief	Deals with the transcription. It is called as soon as the previous transcription is done.
	 * @param f Associated MSFilter (MSTranscript here).
	 * @return
	 */
	std::vector<MSTranscription> process(MSFilter *f) override;

	/**
	 * @brief	Deals with the transcription of the end of the audio.
	 * @param f Associated MSFilter (MSTranscript here).
	 * @return
	 */
	std::vector<MSTranscription> postProcess(MSFilter *f) override;

	/**
	 * @brief	Frees the model
	 * @param f Associated MSFilter (MSTranscript here).
	 */
	void uninit(MSFilter *f) override;

	/**
	 * @brief Constructor of VoskTranscript
	 * @param modelPath Path to the transcription model
	 * @param samplerate Samplerate used for the model
	 */
	VoskTranscript(std::string modelPath, int samplerate);

	~VoskTranscript();

private:
	/**
	 * @brief	Transform the json output of vosk into a vector of MSTranscription containing the transcribed words.
	 * @param sentence Associated MSFilter (MSTranscript here).
	 * @param isPartial Associated MSFilter (MSTranscript here).
	 * @return	A vector of MSTranscriptions
	 */
	std::vector<MSTranscription> jsonToMSTranscript(std::string sentence, bool_t isPartial);

	/**
	 * @brief	Marks as correction the words that corrects words already transcribed. Does not marks the new words.
	 * @param currentIteration Contains all the transcriptions received this iteration.
	 * @return	A vector of MSTranscriptions marked as corrections or not.
	 */
	std::vector<MSTranscription> selectWordsToPrint(std::vector<MSTranscription> currentIteration);

	MSBufferizer *mBuf = NULL; /** Buffer containing raw audio from the packets. */
	VoskModel *mModel;
	VoskRecognizer *mRecognizer;
	std::string mLastWord = ""; /** Last word validated. */
	float mLastTime = 0;        /** Timestamp of the last word validated */

	friend class VoskTester; /** This class is to be used only for tests */
};

#endif