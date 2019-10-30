#ifndef HAMPOO_AUIDO_HW_H
#define HAMPOO_AUIDO_HW_H
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <linux/videodev2.h>
#include <system/audio.h>
#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>
#include <audio_route/audio_route.h>
#include <speex/speex.h>
#include <speex/speex_preprocess.h>
#include <poll.h>
#include <linux/fb.h>
#include <hardware_legacy/uevent.h>

__BEGIN_DECLS

#define LED_DEMO_HARDWARE_MODULE_ID   "hampoo_audio_hal"
#define LED_DEMO_HARDWARE_DEVICE_ID   "hampoo_audio"


struct hampoo_audio_module_t
{
	struct hw_module_t common;
};

struct hampoo_audio_device_t
{
	struct hw_device_t common;
};

__END_DECLS




#endif

