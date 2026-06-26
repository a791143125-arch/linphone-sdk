#ifndef msbackgroundformater_h
#define msbackgroundformater_h
#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/msfilter.h"

typedef enum { MSBackgroundSame, MSBackgroundImage, MSBackgroundVideo, MSBackgroundBlur } MSBackgroundType;

#define MS_BACKGROUND_FORMATER_SET_TYPE MS_FILTER_METHOD(MS_BACKGROUND_FORMATER_ID, 0, int)
#define MS_BACKGROUND_FORMATER_GET_TYPE MS_FILTER_METHOD(MS_BACKGROUND_FORMATER_ID, 1, int)
#define MS_BACKGROUND_REPLACER_SET_BYPASS MS_FILTER_METHOD(MS_BACKGROUND_REPLACER_ID, 0, int)
#define MS_BACKGROUND_REPLACER_GET_BYPASS MS_FILTER_METHOD(MS_BACKGROUND_REPLACER_ID, 1, int)
#define MS_BACKGROUND_FORMATER_SET_PATH MS_FILTER_METHOD(MS_BACKGROUND_FORMATER_ID, 2, const char)

#endif
