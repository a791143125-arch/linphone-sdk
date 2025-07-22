#include "vosk_api.h"
#include <iostream>

int main() {
    // const char* model_path = "vosk-model-small-fr-0.22";  // Path to your Vosk model
    const char* model_path = "vosk-model-en-us-0.22-lgraph";  // Path to your Vosk model
    VoskModel* model = vosk_model_new(model_path);
    if (!model) {
        std::cerr << "Failed to load model\n";
        return 1;
    }

    VoskRecognizer* recognizer = vosk_recognizer_new(model, 16000.0);
    if (!recognizer) {
        std::cerr << "Failed to create recognizer\n";
        return 1;
    }

    FILE *wavin;
    char buf[3200];
    int nread, final;

    wavin = fopen("hello16000.wav", "rb");
    fseek(wavin, 44, SEEK_SET);
    while (!feof(wavin)) {
         nread = fread(buf, 1, sizeof(buf), wavin);
         final = vosk_recognizer_accept_waveform(recognizer, buf, nread);
         if (final) {
             printf("%s\n", vosk_recognizer_result(recognizer));
         } else {
             printf("%s\n", vosk_recognizer_partial_result(recognizer));
         }
    }
    printf("%s\n", vosk_recognizer_final_result(recognizer));



    // Feed recognizer with audio and read results here...

    vosk_recognizer_free(recognizer);
    vosk_model_free(model);
    return 0;
}