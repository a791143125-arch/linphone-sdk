#include <bctoolbox/defs.h>

#include "filter-wrapper/filter-wrapper-base.h"

#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/msvideo.h"

namespace mediastreamer {

class BackgroundReplacerFilterImpl : public FilterBase {
public:
	BackgroundReplacerFilterImpl(MSFilter *f) : FilterBase(f) {
	}

	void preprocess() override {
	}

	void process() override {
		mblk_t *m;
		while ((m = ms_queue_get(getInput(0))) != nullptr) {
			MSPicture pic;
			ms_yuv_buf_init_from_mblk(&pic, m);

            memset(pic.planes[1], 128, pic.strides[1] * (pic.h / 2));
            memset(pic.planes[2], 128, pic.strides[2] * (pic.h / 2));

			ms_queue_put(getOutput(0), m);
		}
	}

	void postprocess() override {
	}
};

}

using namespace mediastreamer;

static MSFilterMethod ms_BackgroundReplacer_methods[] = {{0, nullptr}};

MS_FILTER_WRAPPER_FILTER_DESCRIPTION_BASE(BackgroundReplacer,
                                          MS_BACKGROUND_REPLACER_ID,
                                          "YUV420P background replacer",
                                          MS_FILTER_OTHER,
                                          nullptr, /* enc_fmt */
                                          1,       /* ninputs */
                                          1,       /* noutputs */
                                          BackgroundReplacerFilterImpl,
                                          0 /* flags */)
