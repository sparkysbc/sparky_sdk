
#ifndef LOG_H
#define LOG_H


#ifndef LOG_TAG
#define LOG_TAG "OWLVdecoder"
#endif


#include <stdio.h>
#include <string.h>
    
#define LOG_LEVEL_ERROR     "error  "
#define LOG_LEVEL_WARNING   "warning"
#define LOG_LEVEL_INFO      "info   "
#define LOG_LEVEL_VERBOSE   "verbose"
#define LOG_LEVEL_DEBUG     "debug  "
    
//#define OWLLOG(level, fmt, arg...)  \
//        printf("%s: %s <%s:%u>: "fmt"\n", level, LOG_TAG, __FILE__, __LINE__, ##arg)

#define OWLLOG(level, fmt, arg...)  \
        printf("%s: "fmt"\n", level, ##arg)


#define loge(fmt, arg...) \
    do { \
        OWLLOG(LOG_LEVEL_ERROR, "\033[40;31m"fmt"\033[0m", ##arg) ; \
    } while (0)
    
#define logw(fmt, arg...) OWLLOG(LOG_LEVEL_WARNING, fmt, ##arg)
#define logi(fmt, arg...)
#define logd(fmt, arg...) OWLLOG(LOG_LEVEL_WARNING, fmt, ##arg)
#define logv(fmt, arg...)

#endif

