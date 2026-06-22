#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

namespace mediastreamer {

class BackgroundFormater {
public:
	BackgroundFormater() {
	}

	~BackgroundFormater() {
	}

	void process(MSFilter *f) {
		mblk_t *m;

		while ((m = ms_queue_get(f->inputs[0])) != nullptr) {
			MSPicture src;
			ms_yuv_buf_init_from_mblk(&src, m);
            
			MSPicture dst;
			mblk_t *out = ms_yuv_buf_alloc(&dst, src.w, src.h);

			for (int y = 0; y < src.h; ++y)
				memcpy(dst.planes[0] + y * dst.strides[0],
				       src.planes[0] + y * src.strides[0], src.w);

			// chroma neutre -> niveaux de gris
			memset(dst.planes[1], 128, dst.strides[1] * (src.h / 2));
			memset(dst.planes[2], 128, dst.strides[2] * (src.h / 2));

			freemsg(m);
			ms_queue_put(f->outputs[0], out);
		}
	}

private:
};
} // namespace mediastreamer

using namespace mediastreamer;

static void ms_background_replacer_init(MSFilter *f) {
	f->data = new BackgroundFormater();
}

static void ms_background_replacer_uninit(MSFilter *f) {
	delete reinterpret_cast<BackgroundFormater *>(f->data);
}

static void ms_background_replacer_process(MSFilter *f) {
	reinterpret_cast<BackgroundFormater *>(f->data)->process(f);
}

static MSFilterMethod ms_background_replacer_methods[] = {{0, nullptr}};

extern "C" {

MSFilterDesc ms_BackgroundFormater_desc = {
    .id = MS_BACKGROUND_FORMATER_ID,
    .name = "MSBackgroundFormater",
    .text = "YUV420P background formater",
    .category = MS_FILTER_OTHER,
    .ninputs = 2,
    .noutputs = 1,
    .init = ms_background_replacer_init,
    .process = ms_background_replacer_process,
    .uninit = ms_background_replacer_uninit,
    .methods = ms_background_replacer_methods,
};
}

MS_FILTER_DESC_EXPORT(ms_BackgroundFormater_desc)
