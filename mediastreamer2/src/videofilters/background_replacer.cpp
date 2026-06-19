#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "onnxruntime/onnxruntime_cxx_api.h"
#include <vector>
#include <bctoolbox/defs.h>



#ifdef HAVE_LIBYUV_H
#include "libyuv/convert_argb.h"
#endif
#include <chrono>
namespace mediastreamer {

class BackgroundReplacer {
public:
    BackgroundReplacer() = default;
    ~BackgroundReplacer() = default;

    void process(MSFilter *f) {
        mblk_t *m;
        while ((m = ms_queue_get(f->inputs[0])) != nullptr) {
			
            MSPicture pic;
            ms_yuv_buf_init_from_mblk(&pic, m);
			auto tmp_trait = std::chrono::high_resolution_clock::now();

            // traitement ici
			Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "pphumanseg");
        	Ort::SessionOptions opts;
			opts.SetInterOpNumThreads(1);
        	opts.SetIntraOpNumThreads(1);
			
			const char *dir = ms_factory_get_image_resources_dir(f->factory);
			char *model_path = ms_strdup_printf("%s/pphumanseg.onnx", dir);
			std::cout << model_path << "\n";
			ms_free(model_path);


			// buffer ARGB de sortie : width * height * 4 octets
            std::vector<uint8_t> argb(pic.w * pic.h * 4);

            libyuv::I420ToARGB(
                pic.planes[0], pic.strides[0],   // Y
                pic.planes[1], pic.strides[1],   // U
                pic.planes[2], pic.strides[2],   // V
                argb.data(), pic.w * 4,          // dst ARGB + stride (4 octets/pixel)
                pic.w, pic.h);
			std::cout << "Fin conversion ARGB\n";

            memset(pic.planes[1], 128, pic.strides[1] * (pic.h / 2));
            memset(pic.planes[2], 128, pic.strides[2] * (pic.h / 2));
			auto tmp_trait2 = std::chrono::high_resolution_clock::now();
			std::cout << "temps du taitement : " << std::chrono::duration_cast<std::chrono::milliseconds>(tmp_trait2 - tmp_trait).count() << "ms\n";
			//fin traitement

            ms_queue_put(f->outputs[0], m);
        }
    }
};

}

using namespace mediastreamer;

static void ms_background_replacer_init(MSFilter *f) {
    f->data = new BackgroundReplacer();
}

static void ms_background_replacer_uninit(MSFilter *f) {
    delete reinterpret_cast<BackgroundReplacer *>(f->data);
}

static void ms_background_replacer_process(MSFilter *f) {
    reinterpret_cast<BackgroundReplacer *>(f->data)->process(f);
}

static MSFilterMethod ms_background_replacer_methods[] = {
    {0, nullptr}
};

extern "C" {

MSFilterDesc ms_BackgroundReplacer_desc = {
    .id       = MS_BACKGROUND_REPLACER_ID,
    .name     = "MSBackgroundReplacer",
    .text     = "YUV420P background replacer",
    .category = MS_FILTER_OTHER,
    .ninputs  = 1,
    .noutputs = 1,
    .init     = ms_background_replacer_init,
    .process  = ms_background_replacer_process,
    .uninit   = ms_background_replacer_uninit,
    .methods  = ms_background_replacer_methods,
};

}

MS_FILTER_DESC_EXPORT(ms_BackgroundReplacer_desc)
