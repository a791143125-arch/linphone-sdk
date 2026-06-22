#ifndef msbackgroundformater_h
#define msbackgroundformater_h
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/allfilters.h"

typedef enum {
	MSBackgroundSame,
	MSBackgroundImage,
	MSBackgroundImageAlpha,
	MSBackgroundVideo,
	MSBackgroundVideoAlpha,
	MSBackgroundGif,
	MSBackgroundGifAlpha,
	MSBackgroundBlur
} MSBackgroundType;

#define MS_BACKGROUND_FORMATER_SET_TYPE MS_FILTER_METHOD(MS_BACKGROUND_FORMATER_ID, 0, int)
#define MS_BACKGROUND_FORMATER_GET_TYPE MS_FILTER_METHOD(MS_BACKGROUND_FORMATER_ID, 1, int)

#endif
