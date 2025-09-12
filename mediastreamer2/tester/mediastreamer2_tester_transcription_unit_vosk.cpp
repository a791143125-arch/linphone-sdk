#include "bctoolbox/tester.h"
#include "mediastreamer2/mstranscript.h"
#include "mediastreamer2/vosk-transcript.h"
#include "mediastreamer2_tester.h"
#include "ortp/rtpsession.h"
#include <memory>
#include <random>

class VoskTester {

public:
	std::unique_ptr<VoskTranscript> mInstance =
	    std::make_unique<VoskTranscript>(std::string(MODEL_PATH) + "/ggml-" + std::string(MODEL_NAME) + ".bin", 16000);

	std::vector<MSTranscription> jsonToTranscript(std::string sentence, bool_t isPartial) {
		return mInstance->jsonToMSTranscript(sentence, isPartial);
	}

	std::string selectWordsToPrint(std::vector<MSTranscription> transcriptions) {
		std::string sentence;
		for (MSTranscription word : mInstance->selectWordsToPrint(transcriptions)) {
			if (!word.correction) {
				sentence += word.transcribed_word;
			}
		}
		return sentence;
	}

	std::string getCorrections(std::vector<MSTranscription> transcriptions) {
		std::string sentence;
		for (MSTranscription word : mInstance->selectWordsToPrint(transcriptions)) {
			if (word.correction) {
				sentence += word.transcribed_word;
			}
		}
		return sentence;
	}
};

// Tests the passage from the json returned by vosk and the MSTranscription struct
static void vosk_json() {
	VoskTester tester = VoskTester();

	std::string sentence = R"({
  "partial" : "take thirty",
  "result" : [{
      "conf" : 0.992589,
      "end" : 0.660000,
      "start" : 0.330000,
      "word" : "take"
    }, {
      "conf" : 0.836653,
      "end" : 0.960000,
      "start" : 0.660129,
      "word" : "thirty"
    }]
})";

	std::vector<MSTranscription> transcriptions = tester.jsonToTranscript(sentence, false);

	BC_ASSERT_TRUE(std::string(transcriptions[0].transcribed_word) == "take");
	BC_ASSERT_TRUE(std::string(transcriptions[1].transcribed_word) == "thirty");
	BC_ASSERT_TRUE(int(100 * transcriptions[0].beggining) == 33);
	BC_ASSERT_TRUE(int(100 * transcriptions[1].beggining) == 66);
	BC_ASSERT_TRUE(int(100 * transcriptions[0].timestamp) == 66);
	BC_ASSERT_TRUE(int(100 * transcriptions[1].timestamp) == 96);
	BC_ASSERT_TRUE(int(100 * transcriptions[0].confidence) == 99);
	BC_ASSERT_TRUE(int(100 * transcriptions[1].confidence) == 83);
}

static void vosk_json_partial() {
	VoskTester tester = VoskTester();

	std::string sentence = R"({
  "partial" : "take thirty",
  "partial_result" : [{
      "conf" : 0.992589,
      "end" : 0.660000,
      "start" : 0.330000,
      "word" : "take"
    }, {
      "conf" : 0.836653,
      "end" : 0.960000,
      "start" : 0.660129,
      "word" : "thirty"
    }]
})";

	std::vector<MSTranscription> transcriptions = tester.jsonToTranscript(sentence, true);

	BC_ASSERT_TRUE(std::string(transcriptions[0].transcribed_word) == "take");
	BC_ASSERT_TRUE(std::string(transcriptions[1].transcribed_word) == "thirty");
	BC_ASSERT_TRUE(int(100 * transcriptions[0].beggining) == 33);
	BC_ASSERT_TRUE(int(100 * transcriptions[1].beggining) == 66);
	BC_ASSERT_TRUE(int(100 * transcriptions[0].timestamp) == 66);
	BC_ASSERT_TRUE(int(100 * transcriptions[1].timestamp) == 96);
	BC_ASSERT_TRUE(int(100 * transcriptions[0].confidence) == 99);
	BC_ASSERT_TRUE(int(100 * transcriptions[1].confidence) == 83);
}

// basic selection of words to be printed
static void vosk_select_words() {
	VoskTester tester = VoskTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2});
	endResult += tester.selectWordsToPrint(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"3", 5});
	vectorOfWords.push_back({"4", 6});
	endResult += tester.selectWordsToPrint(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "1234", 5);
}

// we do not accept as a new word one  with a timestamp before the last one to be accepted
static void vosk_select_words_before_last_timestamp() {
	VoskTester tester = VoskTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2});
	endResult += tester.selectWordsToPrint(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2_bis", 1.5});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.selectWordsToPrint(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

// We ignore dupplicated words
static void vosk_select_words_dupplicated() {
	VoskTester tester = VoskTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2});
	endResult += tester.selectWordsToPrint(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2", 2.1});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.selectWordsToPrint(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

// We mark as corrected words that appears before the last timestamp accepted
static void vosk_get_corrections() {
	VoskTester tester = VoskTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.getCorrections(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"4", 1});
	vectorOfWords.push_back({"5", 2});
	vectorOfWords.push_back({"6", 3});
	endResult += tester.getCorrections(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "456", 4);
}

static int tester_before_all(void) {
	return 0;
}

static int tester_after_all(void) {
	return 0;
}

static test_t tests[] = {
    TEST_NO_TAG("vosk json", vosk_json),
    TEST_NO_TAG("vosk json partial", vosk_json_partial),
    TEST_NO_TAG("vosk select words", vosk_select_words),
    TEST_NO_TAG("vosk select words before last timestamp", vosk_select_words_before_last_timestamp),
    TEST_NO_TAG("vosk select words dupplicated", vosk_select_words_dupplicated),
    TEST_NO_TAG("vosk get corrections", vosk_get_corrections),
};
extern "C" {
test_suite_t vosk_unit_test_suite = {
    "Unit Vosk", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
}