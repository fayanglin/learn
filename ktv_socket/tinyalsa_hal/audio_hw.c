/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file    audio_hw.c
 * @brief 
 *                 ALSA Audio Git Log
 * - V0.1.0:add alsa audio hal,just support 312x now.
 * - V0.2.0:remove unused variable.
 * - V0.3.0:turn off device when do_standby.
 * - V0.4.0:turn off device before open pcm.
 * - V0.4.1:Need to re-open the control to fix no sound when suspend.
 * - V0.5.0:Merge the mixer operation from legacy_alsa.
 * - V0.6.0:Merge speex denoise from legacy_alsa.
 * - V0.7.0:add copyright.
 * - V0.7.1:add support for box audio
 * - V0.7.2:add support for dircet output
 * - V0.8.0:update the direct output for box, add the DVI mode
 * - V1.0.0:stable version
 *
 * @author  RkAudio
 * @version 1.0.5
 * @date    2015-08-24
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "AudioHardwareTiny"

#include "alsa_audio.h"
#include "audio_hw.h"
#include "audio_hw_hdmi.h"
#include <system/audio.h>
#include "sys/un.h"
#include "sys/socket.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

//#define ALSA_DEBUG
#ifdef ALSA_IN_DEBUG
FILE *in_debug;
#endif
#define LOCAL_SOCKET_NAME "/data/ktv_local_socket"

#define HAMPOO_WAKE_UP
#define HAMPOO_WAKE_UP_DEBUG 0
#define HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG 0

#define HAMPOO_WAKE_UP_RECORD_NUM 2

#ifdef HAMPOO_WAKE_UP
#define _GNU_SOURCE
#define __USE_GNU
#include <sched.h>
#endif

#ifdef HAMPOO_WAKE_UP
#include "AudioQueue.h"
#include "SharedBufferServer.h"
#include "ClientW.h"
#endif

#ifdef HAMPOO_WAKE_UP
#define QUEUE_BUFF_MULTIPLE 1000
#define PERIOD_SIZE 1024
#define MONO_BUFF_SIZE 2048
#define STEREO_BUFF_SIZE 4096

#define RESAMPLE_BUFF_SIZE 1408

typedef void* REACORD_HANDLE;
typedef struct _CAEUserData{
	audio_queue_t *queue;
	char *queue_buff;
}CAEUserData;

typedef struct _RecordData{
	pthread_t tid_pcm_read;
	int runing;
}RecordData;

#if 0
struct hampoo_resampler{

	unsigned int requested_rate;
	struct resampler_itfe *resampler;
	struct resampler_buffer_provider buf_provider;
	int16_t *buffer;
	size_t frames_in;
	int read_status;
	uint16_t ramp_vol;
	uint16_t ramp_step;
	size_t	ramp_frames;	
	unsigned int  channel_mask;
};

struct hampoo_resampler *in_resample;
static NATIVE_EVENT_HANDLE g_resample_event = NULL;
static CAEUserData resampleData;//memset


#endif
static CAEUserData userData;//memset
//static RecordData * g_record = NULL;
//static struct pcm * in_pcm_handle = NULL;
static int in_pcm = 0;

//static int in_buff_size = 0;
static volatile int in_pcm_count = 0;
//static int audio_record_status = 0;
//static NATIVE_EVENT_HANDLE g_record_event = NULL;


#if HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG
static FILE *file_audio_write = NULL;
#endif

#if HAMPOO_WAKE_UP_DEBUG
static FILE *file_in_read = NULL;
static FILE *file_audio_write = NULL;
static FILE *file_audio_callback = NULL;
#endif
#endif
int in_dump(const struct audio_stream *stream, int fd);
int out_dump(const struct audio_stream *stream, int fd);

/**
 * @brief get_output_device_id 
 *
 * @param device
 *
 * @returns 
 */
int get_output_device_id(audio_devices_t device)
{
    if (device == AUDIO_DEVICE_NONE)
        return OUT_DEVICE_NONE;

    if (popcount(device) == 2) {
        if ((device == (AUDIO_DEVICE_OUT_SPEAKER |
                       AUDIO_DEVICE_OUT_WIRED_HEADSET)) ||
                (device == (AUDIO_DEVICE_OUT_SPEAKER |
                        AUDIO_DEVICE_OUT_WIRED_HEADPHONE)))
            return OUT_DEVICE_SPEAKER_AND_HEADSET;
        else
            return OUT_DEVICE_NONE;
    }

    if (popcount(device) != 1)
        return OUT_DEVICE_NONE;

    switch (device) {
    case AUDIO_DEVICE_OUT_SPEAKER:
        return OUT_DEVICE_SPEAKER;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        return OUT_DEVICE_HEADSET;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        return OUT_DEVICE_HEADPHONES;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return OUT_DEVICE_BT_SCO;
    default:
        return OUT_DEVICE_NONE;
    }
}

/**
 * @brief get_input_source_id 
 *
 * @param source
 *
 * @returns 
 */
int get_input_source_id(audio_source_t source)
{
    switch (source) {
    case AUDIO_SOURCE_DEFAULT:
        return IN_SOURCE_NONE;
    case AUDIO_SOURCE_MIC:
        return IN_SOURCE_MIC;
    case AUDIO_SOURCE_CAMCORDER:
        return IN_SOURCE_CAMCORDER;
    case AUDIO_SOURCE_VOICE_RECOGNITION:
        return IN_SOURCE_VOICE_RECOGNITION;
    case AUDIO_SOURCE_VOICE_COMMUNICATION:
        return IN_SOURCE_VOICE_COMMUNICATION;
    default:
        return IN_SOURCE_NONE;
    }
}

/**
 * @brief force_non_hdmi_out_standby 
 * must be called with hw device outputs list, all out streams, and hw device mutexes locked
 *
 * @param adev
 */
static void force_non_hdmi_out_standby(struct audio_device *adev)
{
    enum output_type type;
    struct stream_out *out;

    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        out = adev->outputs[type];
        if (type == OUTPUT_HDMI_MULTI|| !out)
            continue;
        /* This will never recurse more than 2 levels deep. */
        do_out_standby(out);
    }
}


/**
 * @brief start_bt_sco 
 * must be called with the hw device mutex locked, OK to hold other mutexes
 *
 * @param adev
 */
static void start_bt_sco(struct audio_device *adev) {
    if (adev->sco_on_count++ > 0)
        return;

    adev->pcm_voice_out = pcm_open(PCM_CARD, PCM_DEVICE_VOICE, PCM_OUT | PCM_MONOTONIC,
                              &pcm_config_sco);
    if (adev->pcm_voice_out && !pcm_is_ready(adev->pcm_voice_out)) {
        ALOGE("pcm_open(VOICE_OUT) failed: %s", pcm_get_error(adev->pcm_voice_out));
        goto err_voice_out;
    }
    adev->pcm_sco_out = pcm_open(PCM_CARD, PCM_DEVICE_SCO, PCM_OUT | PCM_MONOTONIC,
                            &pcm_config_sco);
    if (adev->pcm_sco_out && !pcm_is_ready(adev->pcm_sco_out)) {
        ALOGE("pcm_open(SCO_OUT) failed: %s", pcm_get_error(adev->pcm_sco_out));
        goto err_sco_out;
    }
    adev->pcm_voice_in = pcm_open(PCM_CARD, PCM_DEVICE_VOICE, PCM_IN,
                                 &pcm_config_sco);
    if (adev->pcm_voice_in && !pcm_is_ready(adev->pcm_voice_in)) {
        ALOGE("pcm_open(VOICE_IN) failed: %s", pcm_get_error(adev->pcm_voice_in));
        goto err_voice_in;
    }
    adev->pcm_sco_in = pcm_open(PCM_CARD, PCM_DEVICE_SCO, PCM_IN,
                               &pcm_config_sco);
    if (adev->pcm_sco_in && !pcm_is_ready(adev->pcm_sco_in)) {
        ALOGE("pcm_open(SCO_IN) failed: %s", pcm_get_error(adev->pcm_sco_in));
        goto err_sco_in;
    }

    pcm_start(adev->pcm_voice_out);
    pcm_start(adev->pcm_sco_out);
    pcm_start(adev->pcm_voice_in);
    pcm_start(adev->pcm_sco_in);

    return;

err_sco_in:
    pcm_close(adev->pcm_sco_in);
err_voice_in:
    pcm_close(adev->pcm_voice_in);
err_sco_out:
    pcm_close(adev->pcm_sco_out);
err_voice_out:
    pcm_close(adev->pcm_voice_out);
}

/**
 * @brief stop_bt_sco 
 * must be called with the hw device mutex locked, OK to hold other mutexes 
 *
 * @param adev
 */
static void stop_bt_sco(struct audio_device *adev) {
    if (adev->sco_on_count == 0 || --adev->sco_on_count > 0)
        return;

    pcm_stop(adev->pcm_voice_out);
    pcm_stop(adev->pcm_sco_out);
    pcm_stop(adev->pcm_voice_in);
    pcm_stop(adev->pcm_sco_in);

    pcm_close(adev->pcm_voice_out);
    pcm_close(adev->pcm_sco_out);
    pcm_close(adev->pcm_voice_in);
    pcm_close(adev->pcm_sco_in);
}

/**
 * @brief start_bt_hfp
 * must be called with the hw device mutex locked, OK to hold other mutexes
 *
 * @param adev
 */
static void start_bt_hfp(struct audio_device *adev) {
    if (adev->hfp_on_count++ > 0)
        return;

    adev->pcm_hfp_out = pcm_open(PCM_CARD, PCM_DEVICE_HFP, PCM_OUT | PCM_MONOTONIC,
                              &pcm_config_hfp);
    if (adev->pcm_hfp_out && !pcm_is_ready(adev->pcm_hfp_out)) {
        ALOGE("pcm_open(HFP_OUT) failed: %s", pcm_get_error(adev->pcm_hfp_out));
        goto err_hfp_out;
    }
    adev->pcm_hfp_in = pcm_open(PCM_CARD, PCM_DEVICE_HFP, PCM_IN,
                                 &pcm_config_hfp);
    if (adev->pcm_hfp_in && !pcm_is_ready(adev->pcm_hfp_in)) {
        ALOGE("pcm_open(HFP_IN) failed: %s", pcm_get_error(adev->pcm_hfp_in));
        goto err_hfp_in;
    }

    pcm_start(adev->pcm_hfp_out);
    pcm_start(adev->pcm_hfp_in);

    return;

err_hfp_in:
    pcm_close(adev->pcm_hfp_in);
err_hfp_out:
    pcm_close(adev->pcm_hfp_out);
}

/**
 * @brief stop_bt_hfp
 * must be called with the hw device mutex locked, OK to hold other mutexes
 *
 * @param adev
 */
static void stop_bt_hfp(struct audio_device *adev) {
    if (adev->hfp_on_count == 0 || --adev->hfp_on_count > 0)
        return;

    pcm_stop(adev->pcm_hfp_out);
    pcm_stop(adev->pcm_hfp_in);

    pcm_close(adev->pcm_hfp_out);
    pcm_close(adev->pcm_hfp_in);
}

/**
 * @brief getOutputRouteFromDevice 
 *
 * @param device
 *
 * @returns 
 */
unsigned getOutputRouteFromDevice(uint32_t device)
{
    /*if (mMode != AudioSystem::MODE_RINGTONE && mMode != AudioSystem::MODE_NORMAL)
        return PLAYBACK_OFF_ROUTE;
    */
    switch (device) {
    case AUDIO_DEVICE_OUT_SPEAKER:
        return SPEAKER_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        return HEADSET_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        return HEADPHONE_NORMAL_ROUTE;
    case (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADPHONE):
    case (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADSET):
        return SPEAKER_HEADPHONE_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return BLUETOOTH_NORMAL_ROUTE;
    //case AudioSystem::DEVICE_OUT_AUX_DIGITAL:
    //	return HDMI_NORMAL_ROUTE;
    //case AudioSystem::DEVICE_OUT_EARPIECE:
    //	return EARPIECE_NORMAL_ROUTE;
    //case AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET:
    //case AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET:
    //	return USB_NORMAL_ROUTE;
    default:
        return PLAYBACK_OFF_ROUTE;
    }
}

/**
 * @brief getVoiceRouteFromDevice 
 *
 * @param device
 *
 * @returns 
 */
uint32_t getVoiceRouteFromDevice(uint32_t device)
{
    ALOGE("not support now");
    return 0;
}

/**
 * @brief getInputRouteFromDevice 
 *
 * @param device
 *
 * @returns 
 */
uint32_t getInputRouteFromDevice(uint32_t device)
{
    /*if (mMicMute) {
        return CAPTURE_OFF_ROUTE;
    }*/
   //ALOGE("%s:device:%x",__FUNCTION__,device);
    switch (device) {
        case AUDIO_DEVICE_IN_BUILTIN_MIC:
            return MAIN_MIC_CAPTURE_ROUTE;
        case AUDIO_DEVICE_IN_WIRED_HEADSET:
            return HANDS_FREE_MIC_CAPTURE_ROUTE;
        case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            return BLUETOOTH_SOC_MIC_CAPTURE_ROUTE;
        case AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET:
            return USB_CAPTURE_ROUTE;
        default:
            return CAPTURE_OFF_ROUTE;
    }
}

/**
 * @brief getRouteFromDevice 
 *
 * @param device
 *
 * @returns 
 */
uint32_t getRouteFromDevice(uint32_t device)
{
    if (device & AUDIO_DEVICE_BIT_IN)
        return getInputRouteFromDevice(device);
    else
        return getOutputRouteFromDevice(device);
}


/**
 * @brief read_hdmi_audioinfo 
 *
 * @returns 
 */
static int read_hdmi_audioinfo(void)
{
    FILE *fd = NULL;
    char buf[PROPERTY_VALUE_MAX]="";

    fd = fopen(HDMI_AUIOINFO_NODE, "r");
    memset(buf, 0, PROPERTY_VALUE_MAX);
    if(fd != NULL) {
        fread(buf, 1, PROPERTY_VALUE_MAX, fd);
        fclose(fd);
    }
    property_set(MEDIA_SINK_AUDIO, buf);
    return 0;
}

/**
 * @brief read_snd_card_info 
 *
 * @returns 
 */
static int read_snd_card_info(void)
{
    FILE *fd = NULL;
    char buf0[25] = "";
    char buf1[25] = "";
    char buf2[25] = "";


    fd = fopen(SND_CARD1_NODE,"r");
    memset(buf1, 0, sizeof(buf1));
    if (fd != NULL) {
        fread(buf1,1,sizeof(buf1),fd);
        fclose(fd);
    }
    fd = fopen(SND_CARD0_NODE,"r");
    memset(buf0, 0, sizeof(buf0));
    if (fd != NULL) {
        fread(buf0,1,sizeof(buf0),fd);
        fclose(fd);
    }

    fd = fopen(SND_CARD2_NODE,"r");
    memset(buf2, 0, sizeof(buf2));
    if (fd != NULL) {
        fread(buf2,1,sizeof(buf2),fd);
        fclose(fd);
    }
    ALOGD("read_snd_card_info buf0 = %s",buf0);
    ALOGD("read_snd_card_info buf1 = %s",buf1);
    ALOGD("read_snd_card_info buf2 = %s",buf2);
    if (strstr (buf1, "SPDIF")) {
       if (strstr(buf2, "HDMI") || strstr(buf2, "rockchiphdmi") || strstr(buf2, "rockchipcdndpfb")) {
           ALOGD("now is 3 snd card mode");
           PCM_CARD = 0;
           PCM_CARD_SPDIF = 1;
           PCM_CARD_HDMI = 2;
       } else {
           ALOGD("now is 2 snd card mode");
           PCM_CARD = 0;
           PCM_CARD_HDMI = 0;
           PCM_CARD_SPDIF = 1;
       }
    } else if (strstr(buf1, "HDMI") || strstr(buf1, "rockchiphdmi") || strstr(buf1, "rockchipcdndpfb")) {
        ALOGD("now is 3snd card mode");
        PCM_CARD = 0;
        PCM_CARD_HDMI = 1;
        PCM_CARD_SPDIF = 2;
        if (strstr(buf0, "rockchipspdif") || strstr(buf0, "SPDIF")) {
            PCM_CARD_SPDIF = 0;
            PCM_CARD = 1;
        }
    } else if (strstr(buf0, "HDMI") || strstr(buf0, "rockchiphdmi") || strstr(buf0, "rockchipcdndpfb")) {
        PCM_CARD = 0;
        PCM_CARD_HDMI = 0;
        PCM_CARD_SPDIF = 1;
        if (strstr(buf2, "rockchipspdif") || strstr(buf2, "SPDIF")) {
            PCM_CARD = 1;
            PCM_CARD_SPDIF = 2;
        }
    } else if (strstr(buf2, "HDMI") || strstr(buf2, "rockchiphdmi")) {
        PCM_CARD = 0;
        PCM_CARD_HDMI = 2;
        PCM_CARD_SPDIF = 1;
        if (strstr(buf0, "rockchipspdif") || strstr(buf0, "SPDIF")) {
            PCM_CARD = 1;
            PCM_CARD_SPDIF = 0;
        }
    }
    return 0;
}
#ifdef BOX_HAL

/**
 * @brief read_hdmi_connect_state 
 *
 * @returns 
 */
static int read_hdmi_connect_state(void)
{
    FILE *fd = NULL;
    char buf[20] = "";

    fd = fopen(HDMI_CONNECTION_NODE,"r");
    memset(buf, 0, sizeof(buf));
    if (fd != NULL) {
        fread(buf,1,sizeof(buf),fd);
        fclose(fd);
    }
    if (strstr(buf, "1"))
        return 1;
    return 0;
}

#endif

/**
 * @brief start_output_stream 
 * must be called with hw device outputs list, output stream, and hw device mutexes locked 
 *
 * @param out
 *
 * @returns 
 */
static int start_output_stream(struct stream_out *out)
{
    char value[PROPERTY_VALUE_MAX] = "";
    struct audio_device *adev = out->dev;
    int type;
    bool connect_hdmi = true;
    int ret = 0;

    ALOGD("%s",__FUNCTION__);
    if (out == adev->outputs[OUTPUT_HDMI_MULTI]) {
        force_non_hdmi_out_standby(adev);
    } else if (adev->outputs[OUTPUT_HDMI_MULTI] && !adev->outputs[OUTPUT_HDMI_MULTI]->standby) {
        out->disabled = true;
        return 0;
    }
    out->disabled = false;
    scount = 0;
    read_hdmi_audioinfo();
    property_get(MEDIA_SINK_AUDIO, value, "invalid");
    if (strstr(value,"LPCM")) {
        connect_hdmi = true;
    }
    property_get(MEDIA_AUDIO_DEVICE, value, "");
    if (atoi(value) == SPDIF_PASSTHROUGH_MODE) {
        out->device &= ~AUDIO_DEVICE_OUT_AUX_DIGITAL;
	out->device |= AUDIO_DEVICE_OUT_SPDIF;
    } else if (atoi(value) == HDMI_BITSTREAM_MODE) {
        out->device &= ~AUDIO_DEVICE_OUT_SPDIF;
	out->device |= AUDIO_DEVICE_OUT_AUX_DIGITAL;
    }
#ifdef BOX_HAL
    if (out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        /*BOX hdmi & codec use the same i2s,so only config the codec card*/
        out->device &= ~AUDIO_DEVICE_OUT_SPEAKER;
    }
#ifdef RK3228
    if (direct_mode.output_mode == HW_PARAMS_FLAG_LPCM) {
        if (out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
            out->device |= AUDIO_DEVICE_OUT_SPEAKER;
        }
    }
#endif
    out_dump(out, 0);
#endif
    connect_hdmi = true;
    out->device = 0;
    out->device = AUDIO_DEVICE_OUT_SPEAKER;
    route_pcm_open(getRouteFromDevice(out->device));

    if (out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
		 ALOGD("The current HDMI AUDIO_DEVICE_OUT_AUX_DIGITAL");
        if (connect_hdmi) {
            out->pcm[PCM_CARD_HDMI] = pcm_open(PCM_CARD_HDMI, out->pcm_device,
                                                PCM_OUT | PCM_MONOTONIC, &out->config);
            if (out->pcm[PCM_CARD_HDMI] &&
                    !pcm_is_ready(out->pcm[PCM_CARD_HDMI])) {
                ALOGE("pcm_open(PCM_CARD_HDMI) failed: %s",
                      pcm_get_error(out->pcm[PCM_CARD_HDMI]));
                pcm_close(out->pcm[PCM_CARD_HDMI]);
                return -ENOMEM;
            }
        } else {
           ALOGD("The current HDMI is DVI mode");
           out->device |= AUDIO_DEVICE_OUT_SPEAKER;
        }
    }

    if (out->device & (AUDIO_DEVICE_OUT_SPEAKER |
                       AUDIO_DEVICE_OUT_WIRED_HEADSET |
                       AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                       AUDIO_DEVICE_OUT_ALL_SCO)) {
		ALOGD("The current AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_WIRED_HEADSET.......");

        out->pcm[PCM_CARD] = pcm_open(PCM_CARD, out->pcm_device,
                                      PCM_OUT | PCM_MONOTONIC, &out->config);
        if (out->pcm[PCM_CARD] && !pcm_is_ready(out->pcm[PCM_CARD])) {
            ALOGE("pcm_open(PCM_CARD) failed: %s",
                  pcm_get_error(out->pcm[PCM_CARD]));
            pcm_close(out->pcm[PCM_CARD]);
            return -ENOMEM;
        }
		property_set("persist.sys.halspeek","1");

    }

    if (out->device & AUDIO_DEVICE_OUT_SPDIF) {
		ALOGD("The current AUDIO_DEVICE_OUT_SPDIF.......");
        out->pcm[PCM_CARD_SPDIF] = pcm_open(PCM_CARD_SPDIF, out->pcm_device,
                                            PCM_OUT | PCM_MONOTONIC, &out->config);

        if (out->pcm[PCM_CARD_SPDIF] &&
                !pcm_is_ready(out->pcm[PCM_CARD_SPDIF])) {
            ALOGE("pcm_open(PCM_CARD_SPDIF) failed: %s",
                  pcm_get_error(out->pcm[PCM_CARD_SPDIF]));
            pcm_close(out->pcm[PCM_CARD_SPDIF]);
            return -ENOMEM;
        }

    }

    adev->out_device |= out->device;

    if (out->device & AUDIO_DEVICE_OUT_ALL_SCO)
        start_bt_sco(adev);

    return 0;
}

/**
 * @brief start_input_stream 
 * must be called with input stream and hw device mutexes locked 
 *
 * @param in
 *
 * @returns 
 */
#ifdef HAMPOO_WAKE_UP
#if 0
static int resample_get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    size_t size;
    char *momobuf=NULL;
	char *buf=NULL;
	int i = 3;
	int Len;
    if (buffer_provider == NULL || buffer == NULL)
        return -1;
	buf = (char *)malloc(STEREO_BUFF_SIZE);
    if (in_resample->frames_in == 0) {
		 do
		 {
			 Len=queue_read(resampleData.queue, &momobuf,MONO_BUFF_SIZE);
			 if(Len == MONO_BUFF_SIZE){
				 break;
			 }
			 usleep(50 * 1000);
		 }while(i>0);
		in_resample->read_status = Len;
        if (in_resample->read_status == 0 ||in_resample->read_status < 0 ) {
           // printf("get_next_buffer() read error %d", in_resample->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in_resample->read_status;
        }

	    in_resample->read_status = adjust_channels(momobuf, 1,buf, 2,2, MONO_BUFF_SIZE);
		memcpy(in_resample->buffer,buf, in_resample->read_status);
		//printf("in_resample->read_status  = %d\n",in_resample->read_status);
        //fwrite(in->buffer,pcm_frames_to_bytes(in->pcm,pcm_get_buffer_size(in->pcm)),1,in_debug);
        in_resample->frames_in = PERIOD_SIZE;
		//printf("==== in->frames_in =  = %d \n",in->frames_in);

        /* Do stereo to mono conversion in place by discarding right channel */
        if (in_resample->channel_mask == 1)
        {
           // printf("channel_mask = AUDIO_CHANNEL_IN_MONO");
            for (i = 0; i < in_resample->frames_in; i++)
                in_resample->buffer[i] = in_resample->buffer[i * 2];
        }
    }

    //ALOGV("pcm_frames_to_bytes(in->pcm,pcm_get_buffer_size(in->pcm)):%d",size);
    buffer->frame_count = (buffer->frame_count > in_resample->frames_in) ?
                                in_resample->frames_in : buffer->frame_count;
    buffer->i16 = in_resample->buffer +(PERIOD_SIZE - in_resample->frames_in) *2;
	//printf("==== udio_channel_count_from_in_mask(in->channel_mask)= %d \n",2);
	free(momobuf);
	free(buf);
	momobuf =NULL;
	buf = NULL;
    return in_resample->read_status;

}
static void resample_release_buffer(struct resampler_buffer_provider *buffer_provider,
								struct resampler_buffer* buffer)
{
  if (buffer_provider == NULL || buffer == NULL)
	  return;
  in_resample->frames_in -= buffer->frame_count;
}
static ssize_t resample_read_frames(void *buffer, ssize_t frames)
{
	ssize_t frames_wr = 0;
	size_t frame_size = 4;
	//printf("====read_frames frame_size = %d \n",frame_size);
 
	while (frames_wr < frames) {
		size_t frames_rd = frames - frames_wr;
		if (in_resample->resampler != NULL) {
				//printf("====in_resample->resampler != NULL)\n");
			in_resample->resampler->resample_from_provider(in_resample->resampler,
					(int16_t *)((char *)buffer +
							frames_wr * frame_size),
					&frames_rd);
		} else {
			//printf("====in_resample->resampler == NULL)\n");
			struct resampler_buffer buf = {
					{ raw : NULL, },
					frame_count : frames_rd,
			};
			resample_get_next_buffer(&in_resample->buf_provider, &buf);
			if (buf.raw != NULL) {
				memcpy((char *)buffer +
						   frames_wr * frame_size,
						buf.raw,
						buf.frame_count * frame_size);
				frames_rd = buf.frame_count;

			}
			resample_release_buffer(&in_resample->buf_provider, &buf);
		}
	   if (in_resample->read_status == 0 ||in_resample->read_status < 0 ) {
			return in_resample->read_status;
		}
		frames_wr += frames_rd;
		
	}
	return frames_wr;
}
static int resample_read(void* buffer,size_t bytes)
{
	int ret = 0;

	size_t frames_rq = bytes / 4;
	//printf("====frames_rq==in read=%d\n",frames_rq);
	ret = resample_read_frames(buffer, frames_rq);
	if(ret == 0)
	{
	//	printf("====in_read==end=\n");
		return	ret;
	}
	if(ret < 0)
	{
		//printf("====in_read==error=\n");
		return	ret;
	}

	return bytes;
}
static int hampoo_resampler_init(void)
{
	int ret;
//	printf("====hampoo_resampler_init==in=\n");
//pcm_config->period_size * pcm_config->channels* audio_stream_in_frame_size(&in->stream));malloc(pcm_config->period_size * pcm_config->channels* audio_stream_in_frame_size(&in->stream));malloc(pcm_config->period_size * pcm_config->channels* audio_stream_in_frame_size(&in->stream));
	in_resample = (struct hampoo_resampler *)calloc(1, sizeof(struct hampoo_resampler));
	if (!in_resample)
		return -1;	
	in_resample->buffer = malloc(1024* 2* 4);
	if (!in_resample->buffer) {
		ret = -ENOMEM;
		goto err_malloc;
	}
	in_resample->channel_mask =2;
	in_resample->buf_provider.get_next_buffer = resample_get_next_buffer;
	in_resample->buf_provider.release_buffer = resample_release_buffer;
	ret = create_resampler(48000,16000,2,RESAMPLER_QUALITY_DEFAULT,&in_resample->buf_provider,&in_resample->resampler);
	if (ret != 0) {
		ret = -1;
		goto err_resampler;
	}

	//printf("====hampoo_resampler_init==out=\n");
	return 0;
err_resampler:
	free(in_resample->buffer);
err_malloc:
	free(in_resample);
	return ret;

}
static void hampoo_resampler_exit(void)
{
	if (in_resample->resampler) {
		release_resampler(in_resample->resampler);
		in_resample->resampler = NULL;
	}
	free(in_resample->buffer);
	free(in_resample);	
}

pthread_t tid_resample_buffer;
static void* resample_buffer_thread(void)
{
#if 1
	char buffer[RESAMPLE_BUFF_SIZE];
	int ret = 0;

	ALOGD("resample_buffer_thread start \n");
	while (g_record->runing){
		
		ALOGE("resample_buffer_thread start 1 \n");
		if(g_resample_event != NULL){
			native_event_wait(g_resample_event, 0x7fffffff); 
		}
		ALOGE("resample_buffer_thread start 2 \n");
		
		
		while(in_pcm_count>0){
			if(in_pcm_handle != NULL){

				if(in_pcm ==1)
				{
					ret = resample_read(buffer,RESAMPLE_BUFF_SIZE);
					//ALOGE("resample_buffer_thread read =%d  \n",ret );
				    share_write_data(buffer, ret);//666666
#if HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG
						if(file_audio_write != NULL){
					
							if(ret > 0)
								fwrite(buffer, ret, 1, file_audio_write);
						}
#endif

				}

			}else{
				usleep(5 * 1000);
			}
		}
		ALOGD("resample_buffer_thread start 3 \n");
	}
#endif
	ALOGD("resample_buffer_thread end \n");
	return NULL;
} 


//线程从打开的i2s设备中读取数据写入queue1
static void* RecordThread(void* param)
{
	RecordData *record = (RecordData *)param;
	int ret = 0;
	ALOGE("RecordThread sched_setaffinity return = %d \n", ret);
	char *buffer = NULL;
	int pcm_status = 0;
		struct pcm_config config;
	
	memcpy(&config, &pcm_config_in, sizeof(pcm_config_in));
	while (g_record->runing){
		
		ALOGE("RecordThread start 1 \n");
		if(g_record_event != NULL){
			native_event_wait(g_record_event, 0x7fffffff); 
		}
		ALOGE("RecordThread start 2 \n");
		
		buffer = (char *)malloc(in_buff_size);
		
		while(in_pcm_count>0){
			if(in_pcm_handle != NULL){
				//CAE_ALOGD("RecordThread api_cae_audio_write \n");
				pcm_status =pcm_read(in_pcm_handle, buffer, in_buff_size);
				if(pcm_status != 0)
				{
					in_pcm = 0;
					pcm_close(in_pcm_handle);
					in_pcm_handle = pcm_open(PCM_CARD_USB, PCM_DEVICE, PCM_IN, &config);
					if (!pcm_is_ready(in_pcm_handle)) {
					//	ALOGE("cannot open pcm_in driver: %s", pcm_get_error(in_pcm_handle));
					}	
				}
				else{
					in_pcm =1;
					queue_write(userData.queue, buffer,in_buff_size);
					share_write_data(buffer, in_buff_size);
					//ALOGE("share_write_data in_buff_size =%d \n",in_buff_size);
				#if 0
					queue_write(resampleData.queue, buffer,in_buff_size);
				#endif
				}

			}else{
				usleep(5 * 1000);
			}
		}
		free(buffer);
		buffer = NULL;
		ALOGD("RecordThread start 3 \n");
	}
	
	ALOGV("ljg_RecordThread end\n");
	return NULL;
}

static void start_record()
{
	struct pcm_config config;
	
	memcpy(&config, &pcm_config_in, sizeof(pcm_config_in));

	if( (in_pcm_count==1) && (in_pcm_handle==NULL) ){
		ALOGD("start_record first record ");
		in_pcm_handle = pcm_open(PCM_CARD_USB, PCM_DEVICE, PCM_IN, &config);
		if (!pcm_is_ready(in_pcm_handle)) {
			ALOGE("cannot open pcm_in driver: %s", pcm_get_error(in_pcm_handle));
			pcm_close(in_pcm_handle);
			in_pcm_handle = NULL;
			//adev->active_input = NULL;
			return;// -ENOMEM;
		}

		in_buff_size = 2048;//pcm_frames_to_bytes(in_pcm_handle, pcm_get_buffer_size(in_pcm_handle));	
		ALOGD("start_record in_buff_size = %d ",in_buff_size);
#if HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG
		if(file_audio_write == NULL){
			file_audio_write = fopen("/mnt/sdcard/audio_write.pcm","w");
}
#endif
	
#if HAMPOO_WAKE_UP_DEBUG
		if(file_audio_callback == NULL){
			file_audio_callback = fopen("/mnt/sdcard/audio_callback.pcm","w");
		}		
		if(file_in_read == NULL){
			file_in_read = fopen("/mnt/sdcard/in_read.pcm","w");
		}
#endif
	}
	
	share_write_init();//666666
	#if 0
	queue_clear(resampleData.queue);
	#endif
	native_event_set(g_record_event);//参数全部设置好再开启线程 
	#if 0
	native_event_set(g_resample_event);
	#endif
}

static void stop_record()
{
	ALOGE("stop_record in ");
	if( (in_pcm_count==0) && (in_pcm_handle != NULL) ){
		ALOGE("stop_record last record ");
		pcm_close(in_pcm_handle);
		in_pcm_handle = NULL;
#if HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG
	if(file_audio_write != NULL){
		fclose(file_audio_write);
		file_audio_write = NULL;
	}
#endif
		
#if HAMPOO_WAKE_UP_DEBUG
	if(file_in_read != NULL){
		fclose(file_in_read);
		file_in_read = NULL;
	}
	if(file_audio_callback != NULL){
		fclose(file_audio_callback);
		file_audio_callback = NULL;
	}	
#endif		
		
	}
}
#endif

#endif

static void start_record()
{
	//ALOGE("start_record in ");

#if HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG
	if(file_audio_write == NULL)
	{
		file_audio_write = fopen("/mnt/sdcard/audio_write.pcm","w");
	}
#endif
	
#if HAMPOO_WAKE_UP_DEBUG
	if(file_audio_callback == NULL)
	{
		file_audio_callback = fopen("/mnt/sdcard/audio_callback.pcm","w");
	}		
	if(file_in_read == NULL)
	{
		file_in_read = fopen("/mnt/sdcard/in_read.pcm","w");
	}
#endif
}

static void stop_record()
{
	//ALOGE("stop_record in ");

#if HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG
	if(file_audio_write != NULL){
		fclose(file_audio_write);
		file_audio_write = NULL;
	}
#endif
		
#if HAMPOO_WAKE_UP_DEBUG
	if(file_in_read != NULL){
		fclose(file_in_read);
		file_in_read = NULL;
	}
	if(file_audio_callback != NULL){
		fclose(file_audio_callback);
		file_audio_callback = NULL;
	}	
#endif		
	
}

static int start_input_stream(struct stream_in *in)
{
    struct audio_device *adev = in->dev;

    //in_dump(in, 0);
    route_pcm_open(getRouteFromDevice(in->device | AUDIO_DEVICE_BIT_IN));

#ifdef HAMPOO_WAKE_UP
	
	//ALOGD("start_input_stream:%d,%d,%d,%d",in->requested_rate,in->config->rate,in->config->format,in->config->channels);
	//in_ajust_rate = 64000;
//设置麦克阵列录音频率，打开麦克阵列对应的i2s音频设备，示例代码中为64K/pcm2
	//in->config->rate = 48000;
	//in->config->channels = 2;
	
	queue_clear(userData.queue);
	start_record();
	//ALOGE("start_input_stream \n");
#else
    in->pcm = pcm_open(PCM_CARD, PCM_DEVICE, PCM_IN, in->config);
#endif
    
    if (in->pcm && !pcm_is_ready(in->pcm)) {
        ALOGE("pcm_open() failed: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        return -ENOMEM;
    }

    /* if no supported sample rate is available, use the resampler */
    if (in->resampler)
        in->resampler->reset(in->resampler);

    in->frames_in = 0;
    adev->input_source = in->input_source;
    adev->in_device = in->device;
    adev->in_channel_mask = in->channel_mask;


    if (in->device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
        start_bt_sco(adev);

    /* initialize volume ramp */
    in->ramp_frames = (CAPTURE_START_RAMP_MS * in->requested_rate) / 1000;
    in->ramp_step = (uint16_t)(USHRT_MAX / in->ramp_frames);
    in->ramp_vol = 0;;


    return 0;
}

/**
 * @brief get_input_buffer_size 
 *
 * @param sample_rate
 * @param format
 * @param channel_count
 * @param is_low_latency
 *
 * @returns 
 */
static size_t get_input_buffer_size(unsigned int sample_rate,
                                    audio_format_t format,
                                    unsigned int channel_count,
                                    bool is_low_latency)
{
    const struct pcm_config *config = is_low_latency ?
            &pcm_config_in_low_latency : &pcm_config_in;
    size_t size;

    /*
     * take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    size = (config->period_size * sample_rate) / config->rate;
    size = ((size + 15) / 16) * 16;

    return size * channel_count * audio_bytes_per_sample(format);
}

/**
 * @brief get_next_buffer 
 *
 * @param buffer_provider
 * @param buffer
 *
 * @returns 
 */
static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct stream_in *in;
    size_t i,size;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    if (in->pcm == NULL) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    if (in->frames_in == 0) {
        size = pcm_frames_to_bytes(in->pcm,pcm_get_buffer_size(in->pcm));
        in->read_status = pcm_read(in->pcm,
                                   (void*)in->buffer,pcm_frames_to_bytes(in->pcm, in->config->period_size));
        if (in->read_status != 0) {
            ALOGE("get_next_buffer() pcm_read error %d", in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }

        //fwrite(in->buffer,pcm_frames_to_bytes(in->pcm,pcm_get_buffer_size(in->pcm)),1,in_debug);
        in->frames_in = in->config->period_size;

        /* Do stereo to mono conversion in place by discarding right channel */
        if (in->channel_mask == AUDIO_CHANNEL_IN_MONO)
        {
            //ALOGE("channel_mask = AUDIO_CHANNEL_IN_MONO");
            for (i = 0; i < in->frames_in; i++)
                in->buffer[i] = in->buffer[i * 2];
        }
    }

    //ALOGV("pcm_frames_to_bytes(in->pcm,pcm_get_buffer_size(in->pcm)):%d",size);
    buffer->frame_count = (buffer->frame_count > in->frames_in) ?
                                in->frames_in : buffer->frame_count;
    buffer->i16 = in->buffer +
            (in->config->period_size - in->frames_in) *
                audio_channel_count_from_in_mask(in->channel_mask);

    return in->read_status;

}

/**
 * @brief release_buffer 
 *
 * @param buffer_provider
 * @param buffer
 */
static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    in->frames_in -= buffer->frame_count;
}

/**
 * @brief read_frames 
 * read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified 
 *
 * @param in
 * @param buffer
 * @param frames
 *
 * @returns 
 */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;
    size_t frame_size = audio_stream_in_frame_size(&in->stream);

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                    (int16_t *)((char *)buffer +
                            frames_wr * frame_size),
                    &frames_rd);
        } else {
            struct resampler_buffer buf = {
                    { raw : NULL, },
                    frame_count : frames_rd,
            };
            get_next_buffer(&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                           frames_wr * frame_size,
                        buf.raw,
                        buf.frame_count * frame_size);
                frames_rd = buf.frame_count;
                //ALOGV("====frames_wr:%d,buf.frame_count:%d,frame_size:%d====",frames_wr,buf.frame_count,frame_size);
#ifdef ALSA_IN_DEBUG		        
                fwrite(buffer,frames_wr * frame_size,1,in_debug);
#endif
            }
            release_buffer(&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}

/**
 * @brief out_get_sample_rate 
 *
 * @param stream
 *
 * @returns 
 */
static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    return out->config.rate;
}

/**
 * @brief out_set_sample_rate 
 *
 * @param stream
 * @param rate
 *
 * @returns 
 */
static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return -ENOSYS;
}

/**
 * @brief out_get_buffer_size 
 *
 * @param stream
 *
 * @returns 
 */
static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->config.period_size *
            audio_stream_out_frame_size((const struct audio_stream_out *)stream);
}

/**
 * @brief out_get_channels 
 *
 * @param stream
 *
 * @returns 
 */
static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->channel_mask;
}

/**
 * @brief out_get_format 
 *
 * @param stream
 *
 * @returns 
 */
static audio_format_t out_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

/**
 * @brief out_set_format 
 *
 * @param stream
 * @param format
 *
 * @returns 
 */
static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

/**
 * @brief output_devices 
 * Return the set of output devices associated with active streams
 * other than out.  Assumes out is non-NULL and out->dev is locked.
 *
 * @param out
 *
 * @returns 
 */
static audio_devices_t output_devices(struct stream_out *out)
{
    struct audio_device *dev = out->dev;
    enum output_type type;
    audio_devices_t devices = AUDIO_DEVICE_NONE;

    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        struct stream_out *other = dev->outputs[type];
        if (other && (other != out) && !other->standby) {
            // TODO no longer accurate
            /* safe to access other stream without a mutex,
             * because we hold the dev lock,
             * which prevents the other stream from being closed
             */
            devices |= other->device;
        }
    }

    return devices;
}

/**
 * @brief do_out_standby 
 * must be called with hw device outputs list, all out streams, and hw device mutex locked 
 *
 * @param out
 */
static void do_out_standby(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int i;
    if (!out->standby) {
		 ALOGD("------!out->standby");
        for (i = 0; i < PCM_TOTAL; i++) {
            if (out->pcm[i]) {
                pcm_close(out->pcm[i]);
                out->pcm[i] = NULL;
            }
        }
        out->standby = true;
        out->nframes = 0;

        if (out == adev->outputs[OUTPUT_HDMI_MULTI]) {
            /* force standby on low latency output stream so that it can reuse HDMI driver if
             * necessary when restarted */
            force_non_hdmi_out_standby(adev);
        }

        if (out->device & AUDIO_DEVICE_OUT_ALL_SCO)
            stop_bt_sco(adev);

        /* re-calculate the set of active devices from other streams */
        adev->out_device = output_devices(out);

        route_pcm_close(PLAYBACK_OFF_ROUTE);
        ALOGD("------close device");

        /* Skip resetting the mixer if no output device is active */
        if (adev->out_device){
            route_pcm_open(getRouteFromDevice(adev->out_device));
            ALOGD("change device");
        }
    	if (direct_mode.hbr_Buf) {
    	    free (direct_mode.hbr_Buf);
    	    direct_mode.hbr_Buf = NULL;
    	}
		ALOGD("----do_out_standby--in-------");
		property_set("persist.sys.halspeek","0");
    }
	// ALOGD("----do_out_standby--out-------");
}

/**
 * @brief lock_all_outputs 
 * lock outputs list, all output streams, and device
 *
 * @param adev
 */
static void lock_all_outputs(struct audio_device *adev)
{
    enum output_type type;
    pthread_mutex_lock(&adev->lock_outputs);
    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        struct stream_out *out = adev->outputs[type];
        if (out)
            pthread_mutex_lock(&out->lock);
    }
    pthread_mutex_lock(&adev->lock);
}

/**
 * @brief unlock_all_outputs 
 * unlock device, all output streams (except specified stream), and outputs list
 *
 * @param adev
 * @param except
 */
static void unlock_all_outputs(struct audio_device *adev, struct stream_out *except)
{
    /* unlock order is irrelevant, but for cleanliness we unlock in reverse order */
    pthread_mutex_unlock(&adev->lock);
    enum output_type type = OUTPUT_TOTAL;
    do {
        struct stream_out *out = adev->outputs[--type];
        if (out && out != except)
            pthread_mutex_unlock(&out->lock);
    } while (type != (enum output_type) 0);
    pthread_mutex_unlock(&adev->lock_outputs);
}

/**
 * @brief out_standby 
 *
 * @param stream
 *
 * @returns 
 */
static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
	
    struct audio_device *adev = out->dev;
	
    ALOGD("Device   : out_standby ");

    lock_all_outputs(adev);

    do_out_standby(out);

    unlock_all_outputs(adev, NULL);

    return 0;
}

/**
 * @brief out_dump 
 *
 * @param stream
 * @param fd
 *
 * @returns 
 */
 int out_dump(const struct audio_stream *stream, int fd)
{
    struct stream_out *out = (struct stream_out *)stream;

    ALOGD("Device     : 0x%x", out->device);
    ALOGD("SampleRate : %d", out->config.rate);
    ALOGD("Channels   : %d", out->config.channels);
    ALOGD("Formate    : %d", out->config.format);
    ALOGD("PreiodSize : %d", out->config.period_size);

    return 0;
}
/**
 * @brief out_set_parameters 
 *
 * @param stream
 * @param kvpairs
 *
 * @returns 
 */
static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    unsigned int val;
	//ALOGD("--- out_set_parameters --1---");

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    lock_all_outputs(adev);
    if (ret >= 0) {
		//ALOGD("--- out_set_parameters ---2--");
        val = atoi(value);
        if ((out->device != val) && (val != 0)) {
            /* Force standby if moving to/from SPDIF or if the output
             * device changes when in SPDIF mode */
            if (((val & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ^
                 (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)) ||
                (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)) {
                ALOGD("--- out_set_parameters ---3--");
                do_out_standby(out);
            }

            /* force output standby to start or stop SCO pcm stream if needed */
            if ((val & AUDIO_DEVICE_OUT_ALL_SCO) ^
                    (out->device & AUDIO_DEVICE_OUT_ALL_SCO)) {
                    ALOGD("--- out_set_parameters ---4--");
                do_out_standby(out);
            }

            if (!out->standby && (out == adev->outputs[OUTPUT_HDMI_MULTI] ||
                    !adev->outputs[OUTPUT_HDMI_MULTI] ||
                    adev->outputs[OUTPUT_HDMI_MULTI]->standby)) {
                adev->out_device = output_devices(out) | val;
#ifndef RK3228
                do_out_standby(out);
				//ALOGD("--- out_set_parameters ---6--");

#endif
			//ALOGD("--- out_set_parameters ---5--");

            }
            out->device = val;
        }
    }
    unlock_all_outputs(adev, NULL);

    str_parms_destroy(parms);
    return ret;
}

/**
 * @brief out_get_parameters 
 *
 * @param stream
 * @param keys
 *
 * @returns 
 */
static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    size_t i, j;
    int ret;
    bool first = true;

    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';
        i = 0;
        /* the last entry in supported_channel_masks[] is always 0 */
        while (out->supported_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(out_channels_name_to_enum_table); j++) {
                if (out_channels_name_to_enum_table[j].value == out->supported_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }
                    strcat(value, out_channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        str = str_parms_to_str(reply);
    } else {
        str = strdup(keys);
    }

    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

/**
 * @brief out_get_latency 
 *
 * @param stream
 *
 * @returns 
 */
static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return (out->config.period_size * out->config.period_count * 1000) /
            out->config.rate;
}

/**
 * @brief out_set_volume 
 *
 * @param stream
 * @param left
 * @param right
 *
 * @returns 
 */
static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;

    /* The mutex lock is not needed, because the client
     * is not allowed to close the stream concurrently with this API
     *  pthread_mutex_lock(&adev->lock_outputs);
     */
    bool is_HDMI = out == adev->outputs[OUTPUT_HDMI_MULTI];
    /*  pthread_mutex_unlock(&adev->lock_outputs); */
    if (is_HDMI) {
        /* only take left channel into account: the API is for stereo anyway */
        out->muted = (left == 0.0f);
        return 0;
    }
    return -ENOSYS;
}
/**
 * @brief dump_out_data
 *
 * @param buffer bytes
 */
 static void dump_out_data(const void* buffer,size_t bytes, int *size)
 {
    ALOGD("dump pcm file.");
    static FILE* fd;
    static int offset = 0;
    if(fd == NULL) {
        fd=fopen("/data/debug.pcm","wb+");
            if(fd == NULL) {
            ALOGD("DEBUG open /data/debug.pcm error =%d ,errno = %d",fd,errno);
            offset = 0;
        }
    }
    fwrite(buffer,bytes,1,fd);
    offset += bytes;
    fflush(fd);
    if(offset >= (*size)*1024*1024) {
        *size = 0;
        fclose(fd);
        offset = 0;
        system("setprop media.audio.record 0");
        ALOGD("TEST playback pcmfile end");
    }
 }

/**
 * @brief reset_bitstream_buf 
 *
 * @param out
 */
static void reset_bitstream_buf(struct stream_out *out)
{
    if (direct_mode.output_mode == HW_PARAMS_FLAG_NLPCM) {
        do_out_standby(out);
        if (direct_mode.hbr_Buf) {
            free (direct_mode.hbr_Buf);
            direct_mode.hbr_Buf = NULL;
        }
    }
}
/**
 * @brief set_bitstream_buf 
 *
 * @param in out length
 */
static void set_bitstream_buf(void * in, void* out, size_t length)
{
    int temp, p, j = 0;
    char *ptr = (char *)in;
    char *ptr_end = (char *)in+length;
    char *newptr = (char *)out;

    while (ptr < ptr_end) {
        newptr[0] = (ptr[0]&0x1f)<<3;
        newptr[1] = ((ptr[0]&0xe0)>>5)|((ptr[1]&0x1f)<<3);
        newptr[2] = (ptr[1]&0xe0)>>5;
        newptr[2] |= channel_status[scount];
        temp = (newptr[2]<<24) | (newptr[1]<<16) | (newptr[0]<<8);
        j=0;
        p=0;
        while (j<31) {
            p ^= temp&0x1;
            p &= 0x1;
            temp >>= 1;
            j++;
        }
        newptr[2] |= (p&0x01)<<6;
        newptr[3] = 0x00;
        scount++;
        scount %= 384;
        ptr +=2;
        newptr +=4;
    }
}

/**
 * @brief out_write 
 *
 * @param stream
 * @param buffer
 * @param bytes
 *
 * @returns 
 */
static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret = 0;
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    size_t newbytes = bytes * 2;
    int i;
    /* FIXME This comment is no longer correct
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the output stream mutex - e.g.
     * executing out_set_parameters() while holding the hw device
     * mutex
     */
	

	//ALOGE("out_write  bt do not open ,we do not open sound card to play");
	char value[PROPERTY_VALUE_MAX];
    property_get("media.audio.debug",value, NULL);	
    if (strstr (value, "1") || strstr (value, "true")) {
        ALOGD("audio playback write bytes = %d, direct_mode.output_mode = %d", bytes, direct_mode.output_mode);
    }

    property_get("media.audio.reset", value, NULL);
    if (atoi(value) > 0) {
        reset_bitstream_buf(out);
        property_set("media.audio.reset", "0");
    }
	property_get("persist.sys.ktvspeek",value,NULL);
    if (strstr (value, "1") || strstr (value, "true")) {
        //ALOGD("KTV will be holding the sound card or holding now-------");
        if (out->pcm[PCM_CARD]) {
              pcm_close(out->pcm[PCM_CARD]);
              out->pcm[PCM_CARD] = NULL;
        }
		do_out_standby(out);
		goto final_exit;
    }
	else
	{
/*	    pthread_mutex_lock(&out->lock);
	    if (out->standby || out->pcm[0] == NULL) {
			ALOGD("AudioData write out->standby ");
	        pthread_mutex_unlock(&out->lock);
	        lock_all_outputs(adev);
	        if (!out->standby) {
	            unlock_all_outputs(adev, out);
	            goto false_alarm;
	        }
			
	        ret = start_output_stream(out);
	        if (ret < 0) {
				ALOGD("AudioData write out->standby ");
	            unlock_all_outputs(adev, NULL);
	            goto final_exit;
	        }
	        out->standby = false;
	        unlock_all_outputs(adev, out);
	    }
*/
			pthread_mutex_lock(&out->lock);
			if (out->standby) {
				ALOGD("AudioData write out->standby ");
				pthread_mutex_unlock(&out->lock);
				lock_all_outputs(adev);
				if (!out->standby) {
					unlock_all_outputs(adev, out);
					goto false_alarm;
				}
				
				ret = start_output_stream(out);
				if (ret < 0) {
					ALOGD("AudioData write out->standby ");
					unlock_all_outputs(adev, NULL);
					goto final_exit;
				}
				out->standby = false;
				unlock_all_outputs(adev, out);
			}


	}


false_alarm:

    if (out->disabled) {
        ret = -EPIPE;
        goto exit;
    }

    if ((direct_mode.output_mode == HW_PARAMS_FLAG_NLPCM)) {
        if (!direct_mode.hbr_Buf) {
            ALOGD("new hbr buffer!");
            direct_mode.hbr_Buf = (char *)malloc(newbytes);
        }
        memset(direct_mode.hbr_Buf, 0x00, newbytes);
        set_bitstream_buf((void *)buffer, (void *)direct_mode.hbr_Buf, bytes);
    }

    if (out->muted)
        memset((void *)buffer, 0, bytes);

    property_get("media.audio.record", value, NULL);
    prop_pcm = atoi(value);
    if (prop_pcm > 0) {
        dump_out_data(buffer, bytes, &prop_pcm);
    }

#if 0
    usleep(bytes * 1000000 / audio_stream_out_frame_size(stream) /
           out_get_sample_rate(&stream->common));
    ALOGD("Donnot output sound !!");
    out->written += bytes / (out->config.channels * sizeof(short));
    pthread_mutex_unlock(&out->lock);
    return bytes;
#endif
    /* Write to all active PCMs */
    if ((direct_mode.output_mode == HW_PARAMS_FLAG_NLPCM) && (direct_mode.hbr_Buf != NULL)) {
        if (out->pcm[PCM_CARD_HDMI] != NULL) {
            ret = pcm_write(out->pcm[PCM_CARD_HDMI], (void *)direct_mode.hbr_Buf, newbytes);
            if (ret != 0) {
               goto exit;
            }
        } else {
            ALOGD("HDMI sound card not open , return !");
            usleep(bytes * 1000000 / audio_stream_out_frame_size(stream) /
                   out_get_sample_rate(&stream->common));
        }
    } else {
        for (i = 0; i < PCM_TOTAL; i++)
            if (out->pcm[i]) {
                ret = pcm_write(out->pcm[i], (void *)buffer, bytes);
                if (ret != 0)
                    break;
            }
    }
    if (ret == 0) {
        out->written += bytes / (out->config.channels * sizeof(short));
        out->nframes = out->written;
    }
exit:
    pthread_mutex_unlock(&out->lock);
final_exit:

    if (ret != 0) {
        ALOGV("AudioData write  error , keep slience! ret = %d", ret);
        usleep(bytes * 1000000 / audio_stream_out_frame_size(stream) /
               out_get_sample_rate(&stream->common));
    }
    return bytes;
}

/**
 * @brief out_get_render_position 
 *
 * @param stream
 * @param dsp_frames
 *
 * @returns 
 */
static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct stream_out *out = (struct stream_out *)stream;

    *dsp_frames = out->nframes;
    return 0;
}

/**
 * @brief out_add_audio_effect 
 *
 * @param stream
 * @param effect
 *
 * @returns 
 */
static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

/**
 * @brief out_remove_audio_effect 
 *
 * @param stream
 * @param effect
 *
 * @returns 
 */
static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

/**
 * @brief out_get_next_write_timestamp 
 *
 * @param stream
 * @param timestamp
 *
 * @returns 
 */
static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    return -EINVAL;
}

/**
 * @brief out_get_presentation_position 
 *
 * @param stream
 * @param frames
 * @param timestamp
 *
 * @returns 
 */
static int out_get_presentation_position(const struct audio_stream_out *stream,
                                   uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream;
    int ret = -1;

    pthread_mutex_lock(&out->lock);

    int i;
    // There is a question how to implement this correctly when there is more than one PCM stream.
    // We are just interested in the frames pending for playback in the kernel buffer here,
    // not the total played since start.  The current behavior should be safe because the
    // cases where both cards are active are marginal.
    for (i = 0; i < PCM_TOTAL; i++)
        if (out->pcm[i]) {
            size_t avail;
            //ALOGD("===============%s,%d==============",__FUNCTION__,__LINE__);
            if (pcm_get_htimestamp(out->pcm[i], &avail, timestamp) == 0) {
                size_t kernel_buffer_size = out->config.period_size * out->config.period_count;
                //ALOGD("===============%s,%d==============",__FUNCTION__,__LINE__);
                // FIXME This calculation is incorrect if there is buffering after app processor
                int64_t signed_frames = out->written - kernel_buffer_size + avail;
                //signed_frames -= 17;
                //ALOGV("============singed_frames:%lld=======",signed_frames);
                //ALOGV("============timestamp:%lld==========",timestamp);
                // It would be unusual for this value to be negative, but check just in case ...
                if (signed_frames >= 0) {
                    *frames = signed_frames;
                    ret = 0;
                }
                break;
            }
        }
    pthread_mutex_unlock(&out->lock);

    return ret;
}

/**
 * @brief in_get_sample_rate 
 * audio_stream_in implementation 
 *
 * @param stream
 *
 * @returns 
 */
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    //ALOGV("%s:get requested_rate : %d ",__FUNCTION__,in->requested_rate);
	
#ifdef HAMPOO_WAKE_UP
//CAE 处理后音频数据为16K
    ALOGV("ljg_rate2:%d",in->requested_rate);
    return 48000;//32000;
#else	
    return in->requested_rate;
#endif
}

/**
 * @brief in_set_sample_rate 
 *
 * @param stream
 * @param rate
 *
 * @returns 
 */
static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

/**
 * @brief in_get_channels 
 *
 * @param stream
 *
 * @returns 
 */
static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    //ALOGV("%s:get channel_mask : %d ",__FUNCTION__,in->channel_mask);
    return in->channel_mask;
}


/**
 * @brief in_get_buffer_size 
 *
 * @param stream
 *
 * @returns 
 */
static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

#ifdef HAMPOO_WAKE_UP	
		return 1024;//4096;
#else
    return get_input_buffer_size(in->requested_rate,
                                 AUDIO_FORMAT_PCM_16_BIT,
                                 audio_channel_count_from_in_mask(in_get_channels(stream)),
                                 (in->flags & AUDIO_INPUT_FLAG_FAST) != 0);
#endif	
}

/**
 * @brief in_get_format 
 *
 * @param stream
 *
 * @returns 
 */
static audio_format_t in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

/**
 * @brief in_set_format 
 *
 * @param stream
 * @param format
 *
 * @returns 
 */
static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

/**
 * @brief do_in_standby 
 * must be called with in stream and hw device mutex locked 
 *
 * @param in
 */
static void do_in_standby(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
	//ALOGD("do_in_standby  in_pcm_count = %d ", in_pcm_count);

	stop_record();
	queue_clear(userData.queue);
#if 0
	queue_clear(resampleData.queue);
#endif 
    if (!in->standby) {
      //  pcm_close(in->pcm);
      //  in->pcm = NULL;
      //	ALOGD("do_in_standby in->standby = %d ", in->standby);
      stop_record();
	  queue_clear(userData.queue);
#if 0
	  queue_clear(resampleData.queue);
#endif
        if (in->device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
            stop_bt_sco(adev);

        in->dev->input_source = AUDIO_SOURCE_DEFAULT;
        in->dev->in_device = AUDIO_DEVICE_NONE;
        in->dev->in_channel_mask = 0;
        in->standby = true;
        route_pcm_close(CAPTURE_OFF_ROUTE);
    }

}

/**
 * @brief in_standby 
 *
 * @param stream
 *
 * @returns 
 */
static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&in->dev->lock);

    do_in_standby(in);

    pthread_mutex_unlock(&in->dev->lock);
    pthread_mutex_unlock(&in->lock);

    return 0;
}

/**
 * @brief in_dump 
 *
 * @param stream
 * @param fd
 *
 * @returns 
 */
int in_dump(const struct audio_stream *stream, int fd)
{
    struct stream_out *in = (struct stream_out *)stream;

    ALOGD("Device     : 0x%x", in->device);
    ALOGD("SampleRate : %d", in->config.rate);
    ALOGD("Channels   : %d", in->config.channels);
    ALOGD("Formate    : %d", in->config.format);
    ALOGD("PreiodSize : %d", in->config.period_size);

    return 0;
}

/**
 * @brief in_set_parameters 
 *
 * @param stream
 * @param kvpairs
 *
 * @returns 
 */
static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    unsigned int val;
    bool apply_now = false;

    parms = str_parms_create_str(kvpairs);

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&adev->lock);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if ((in->input_source != val) && (val != 0)) {
            in->input_source = val;
            apply_now = !in->standby;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    if (ret >= 0) {
        /* strip AUDIO_DEVICE_BIT_IN to allow bitwise comparisons */
        val = atoi(value) & ~AUDIO_DEVICE_BIT_IN;
        /* no audio device uses val == 0 */
        if ((in->device != val) && (val != 0)) {
            /* force output standby to start or stop SCO pcm stream if needed */
            if ((val & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) ^
                    (in->device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)) {
                do_in_standby(in);
            }
            in->device = val;
            apply_now = !in->standby;
        }
    }

    if (apply_now) {
        adev->input_source = in->input_source;
        adev->in_device = in->device;
        route_pcm_open(getRouteFromDevice(in->device | AUDIO_DEVICE_BIT_IN));
    }

    pthread_mutex_unlock(&adev->lock);
    pthread_mutex_unlock(&in->lock);

    str_parms_destroy(parms);
    return ret;
}

/**
 * @brief in_get_parameters 
 *
 * @param stream
 * @param keys
 *
 * @returns 
 */
static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup("");
}

/**
 * @brief in_set_gain 
 *
 * @param stream
 * @param gain
 *
 * @returns 
 */
static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

/**
 * @brief in_apply_ramp 
 *
 * @param in
 * @param buffer
 * @param frames
 */
static void in_apply_ramp(struct stream_in *in, int16_t *buffer, size_t frames)
{
    size_t i;
    uint16_t vol = in->ramp_vol;
    uint16_t step = in->ramp_step;

    frames = (frames < in->ramp_frames) ? frames : in->ramp_frames;

    if (in->channel_mask == AUDIO_CHANNEL_IN_MONO)
        for (i = 0; i < frames; i++)
        {
            buffer[i] = (int16_t)((buffer[i] * vol) >> 16);
            vol += step;
        }
    else
        for (i = 0; i < frames; i++)
        {
            buffer[2*i] = (int16_t)((buffer[2*i] * vol) >> 16);
            buffer[2*i + 1] = (int16_t)((buffer[2*i + 1] * vol) >> 16);
            vol += step;
        }


    in->ramp_vol = vol;
    in->ramp_frames -= frames;
}

/**
 * @brief in_read 
 *
 * @param stream
 * @param buffer
 * @param bytes
 *
 * @returns 
 */
static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    int ret = 0;
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    size_t frames_rq = bytes / audio_stream_in_frame_size(stream);

    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the input stream mutex - e.g.
     * executing in_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&in->lock);
    if (in->standby) {
		ALOGE("start_input_stream ");
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        pthread_mutex_unlock(&adev->lock);
        if (ret < 0)
            goto exit;
        in->standby = false;
    }
//	ALOGE("in_read  bytes = %d",bytes);
	if( in_pcm == 0)
	{
		//ALOGE("ERR0R in_pcm  = %d",in_pcm);
		 do_in_standby(in);
		 goto exit;
	}
#ifdef HAMPOO_WAKE_UP
	char *data_buff = NULL;
	int i = 3,Len;
	do
	{
		//ALOGE("queue_read bytes = %d",bytes);
		Len=queue_read(userData.queue, &data_buff,512);
		if(Len == 512){
			break;
		}
		//ALOGE("in_read error,Len = %d,bytes = %d",Len,bytes);
		usleep(50 * 1000);
	}while(i>0);
	
	//CAE_ALOGD("ljg_read:%d,%d",Len,bytes);
	
	//memcpy((void *)buffer, (void *)data_buff, bytes);
	//free(data_buff);
	//data_buff = NULL;

	if(Len==bytes){//mono
		memcpy((void *)buffer, (void *)data_buff, bytes);
		free(data_buff);
	}else if(Len*2==bytes){//stereo
		for(i=0;i<(bytes/4);i++){
			*((char*)buffer+(i*4)+0) = *(data_buff+(i*2));
			*((char*)buffer+(i*4)+1) = *(data_buff+(i*2)+1);
			*((char*)buffer+(i*4)+2) = *(data_buff+(i*2));
			*((char*)buffer+(i*4)+3) = *(data_buff+(i*2)+1);
		}
		free(data_buff);
	}else{
		ALOGE("2 in_read error,Len = %d,bytes = %d",Len,bytes);
		memset(buffer, 0, bytes);
	}
	
#if HAMPOO_WAKE_UP_DEBUG
		if(file_in_read != NULL){
			fwrite(buffer, bytes, 1, file_in_read);
		}
#endif

#else	
    ret = read_frames(in, buffer, frames_rq);
    if (ret > 0)
        ret = 0;
#endif
#ifdef SPEEX_DENOISE_ENABLE
    if(!adev->mic_mute && ret== 0)
    {
        int index = 0;
        int startPos = 0;
        spx_int16_t* data = (spx_int16_t*) buffer;

        int channel_count = audio_channel_count_from_out_mask(in->channel_mask);
        int curFrameSize = bytes/(channel_count*sizeof(int16_t));
        long ch;
        ALOGV("channel_count:%d",channel_count);
        if(curFrameSize != 2*in->mSpeexFrameSize)
            ALOGD("the current request have some error mSpeexFrameSize %d bytes %d ",in->mSpeexFrameSize,bytes);

        while(curFrameSize >= startPos+in->mSpeexFrameSize)
        {

            for(index = startPos; index< startPos +in->mSpeexFrameSize ;index++ )
                in->mSpeexPcmIn[index-startPos] = data[index*channel_count]/2 + data[index*channel_count+1]/2;

            speex_preprocess_run(in->mSpeexState,in->mSpeexPcmIn);
#ifndef TARGET_RK2928
            for(ch = 0 ; ch < channel_count;ch++)
                for(index = startPos; index< startPos + in->mSpeexFrameSize ;index++ )
                {
                    data[index*channel_count+ch] = in->mSpeexPcmIn[index-startPos];
                }
#else
            for(index = startPos; index< startPos + in->mSpeexFrameSize ;index++ )
            {
                int tmp = (int)in->mSpeexPcmIn[index-startPos]+ in->mSpeexPcmIn[index-startPos]/2;
                data[index*channel_count+0] = tmp > 32767 ? 32767 : (tmp < -32768 ? -32768 : tmp);
            }
            for(int ch = 1 ; ch < channel_count;ch++)
                for(index = startPos; index< startPos + in->mSpeexFrameSize ;index++ )
                {
                    data[index*channel_count+ch] = data[index*channel_count+0];
                }
#endif
             startPos += in->mSpeexFrameSize;
         }
    }
#endif

exit:
    if (ret < 0)
        usleep(bytes * 1000000 / audio_stream_in_frame_size(stream) /
               in_get_sample_rate(&stream->common));

    pthread_mutex_unlock(&in->lock);
    return bytes;
}

/**
 * @brief in_get_input_frames_lost 
 *
 * @param stream
 *
 * @returns 
 */
static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

/**
 * @brief in_add_audio_effect 
 *
 * @param stream
 * @param effect
 *
 * @returns 
 */
static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    struct stream_in *in = (struct stream_in *)stream;
    effect_descriptor_t descr;
    if ((*effect)->get_descriptor(effect, &descr) == 0) {

        pthread_mutex_lock(&in->lock);
        pthread_mutex_lock(&in->dev->lock);


        pthread_mutex_unlock(&in->dev->lock);
        pthread_mutex_unlock(&in->lock);
    }

    return 0;
}

/**
 * @brief in_remove_audio_effect 
 *
 * @param stream
 * @param effect
 *
 * @returns 
 */
static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    struct stream_in *in = (struct stream_in *)stream;
    effect_descriptor_t descr;
    if ((*effect)->get_descriptor(effect, &descr) == 0) {

        pthread_mutex_lock(&in->lock);
        pthread_mutex_lock(&in->dev->lock);


        pthread_mutex_unlock(&in->dev->lock);
        pthread_mutex_unlock(&in->lock);
    }

    return 0;
}

/**
 * @brief adev_open_output_stream 
 *
 * @param dev
 * @param handle
 * @param devices
 * @param flags
 * @param config
 * @param stream_out
 * @param __unused
 *
 * @returns 
 */
static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int ret;
    enum output_type type;

    ALOGE("audio hal adev_open_output_stream devices = 0x%x, flags = %d, samplerate = %d",
		    devices, flags, config->sample_rate);
    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (!out)
        return -ENOMEM;
	
    out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    if (devices == AUDIO_DEVICE_NONE)
        devices = AUDIO_DEVICE_OUT_SPEAKER;
    out->device = devices;

    char value[PROPERTY_VALUE_MAX] = "";
    int device_mode = 0;
    if (flags & AUDIO_OUTPUT_FLAG_DIRECT) {
        property_get(MEDIA_AUDIO_DEVICE, value, "");
        if (atoi(value) == 8) {
            device_mode = SPDIF_PASSTHROUGH_MODE;
        } else if (atoi(value) == 6) {
            device_mode = HDMI_BITSTREAM_MODE;
        } else {
            device_mode = DEFAULT_MODE;
        }
        if ((devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) && (device_mode == HDMI_BITSTREAM_MODE)){
            property_get(MEDIA_CFG_AUDIO_BYPASS, value, "-1");
            if(memcmp(value, "true", 4) == 0){
                out->channel_mask = config->channel_mask;
                if (isCurIEC958SamplerateSupport(config->sample_rate)) {
                    out->config = pcm_config_direct;
                    out->config.rate = config->sample_rate;
                    out->output_direct = true;
                    int channel = audio_channel_count_from_out_mask(config->channel_mask);
                    if (channel == 8) {
                        out->config = pcm_config_direct;
			out->config.rate = config->sample_rate;
                    } else if (config->sample_rate >= 176400) {
                        out->config.period_size = 1024 * 4;
                    } else {
                        out->config.period_size = 2048;
                    }
                    type = OUTPUT_HDMI_MULTI;
                } else {
                    out->config = pcm_config;
                    out->config.rate = 44100;
                    ALOGE("hdmi bitstream samplerate %d unsupport", config->sample_rate);
                }
                out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
                if (out->config.channels < 2)
                    out->config.channels = 2;
                out->pcm_device = PCM_DEVICE;
            } else {
                //property_get(MEDIA_CFG_AUDIO_MUL, value, "-1");
                if (config->sample_rate == 0)
                config->sample_rate = HDMI_MULTI_DEFAULT_SAMPLING_RATE;
                if (config->channel_mask == 0)
                    config->channel_mask = AUDIO_CHANNEL_OUT_5POINT1;
                out->channel_mask = config->channel_mask;
                out->config = pcm_config_hdmi_multi;
                out->config.rate = config->sample_rate;
                out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
                out->pcm_device = PCM_DEVICE;
                out->output_direct = false;
                type = OUTPUT_HDMI_MULTI;
            }
        } else if ((devices & AUDIO_DEVICE_OUT_SPDIF) && (device_mode == SPDIF_PASSTHROUGH_MODE)) {
            out->channel_mask = config->channel_mask;
            out->config = pcm_config_direct;
            if ((config->sample_rate == 48000) ||
                (config->sample_rate == 32000) ||
                (config->sample_rate == 44100)) {
                out->config.rate = config->sample_rate;
                out->config.format = PCM_FORMAT_S16_LE;
                out->config.period_size = 2048;
            } else {
                out->config.rate = 44100;
                ALOGE("spdif passthrough samplerate %d is unsupport",config->sample_rate);
            }
            out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
            devices = AUDIO_DEVICE_OUT_SPDIF;
            out->pcm_device = PCM_DEVICE;
            out->output_direct = true;
            type = OUTPUT_HDMI_MULTI;
        } else {
            out->config = pcm_config;
            out->pcm_device = PCM_DEVICE;
            type = OUTPUT_LOW_LATENCY;
	}
    } else if (flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER) {
        out->config = pcm_config_deep;
        out->pcm_device = PCM_DEVICE_DEEP;
        type = OUTPUT_DEEP_BUF;
    } else {
        out->config = pcm_config;
        out->pcm_device = PCM_DEVICE;
        type = OUTPUT_LOW_LATENCY;
    }

    ALOGD("out->config.rate = %d, out->config.channels = %d", out->config.rate, out->config.channels);
    direct_mode.output_mode = HW_PARAMS_FLAG_LPCM;
    if ((type == OUTPUT_HDMI_MULTI) && (devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) && (device_mode == HDMI_BITSTREAM_MODE)) {
        direct_mode.output_mode = HW_PARAMS_FLAG_NLPCM;
        out->config.format = PCM_FORMAT_S24_LE;
        setChanSta(out->config.rate, out->config.channels);
    } else {
        direct_mode.output_mode = HW_PARAMS_FLAG_LPCM;
        out->config.format = PCM_FORMAT_S16_LE;
    }

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->dev = adev;
    out->dev->pre_output_device_id = OUT_DEVICE_SPEAKER;
    out->dev->pre_input_source_id = IN_SOURCE_MIC;

    config->format = out_get_format(&out->stream.common);
    config->channel_mask = out_get_channels(&out->stream.common);
    config->sample_rate = out_get_sample_rate(&out->stream.common);

    out->standby = true;
    out->nframes = 0;
    /* out->muted = false; by calloc() */
    /* out->written = 0; by calloc() */

    pthread_mutex_lock(&adev->lock_outputs);
    if (adev->outputs[type]) {
        pthread_mutex_unlock(&adev->lock_outputs);
        ret = -EBUSY;
        goto err_open;
    }
    adev->outputs[type] = out;
    pthread_mutex_unlock(&adev->lock_outputs);

    *stream_out = &out->stream;

    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return ret;
}

/**
 * @brief adev_close_output_stream 
 *
 * @param dev
 * @param stream
 */
static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct audio_device *adev;
    enum output_type type;

    ALOGD("adev_close_output_stream!");
    out_standby(&stream->common);
    adev = (struct audio_device *)dev;
    pthread_mutex_lock(&adev->lock_outputs);
    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        if (adev->outputs[type] == (struct stream_out *) stream) {
            adev->outputs[type] = NULL;
            break;
        }
    }
    direct_mode.output_mode = HW_PARAMS_FLAG_LPCM;
    pthread_mutex_unlock(&adev->lock_outputs);
    free(stream);
}

/**
 * @brief adev_set_parameters 
 *
 * @param dev
 * @param kvpairs
 *
 * @returns 
 */
static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
	struct str_parms *parms;
    char *str;
    char value[32];
    int ret;
	int temp_val;
	
	ALOGV("adev_set_parameters, %s", kvpairs);
	parms 	= str_parms_create_str(kvpairs);
	ALOGE("aaaaaaaaaaaaaaa");
#ifdef HAMPOO_WAKE_UP
#if 0
	ALOGE("bbbbb");

    ret = str_parms_get_str(parms, "audio_record_status", value, sizeof(value));
    if (ret >= 0) {
		temp_val = atoi(value);
		ALOGE("cccccccc");
		if( (temp_val == 1) && (audio_record_status == 0) ) {
			ALOGE("ddddddddddd");
			in_pcm_count++;
			ALOGE("eeeeeeeee");
			share_write_clear();////666666
			ALOGE("fffffffffff");
			start_record();
		}else if( (temp_val == 0) && (audio_record_status == 1) ){
			in_pcm_count--;
			stop_record();
		}
		audio_record_status = temp_val;
		ALOGD("adev_set_parameters in_pcm_count = %d ", in_pcm_count);
    }
#endif
#endif

    str_parms_destroy(parms);
		
    return 0;
}

/**
 * @brief adev_get_parameters 
 *
 * @param dev
 * @param keys
 *
 * @returns 
 */
static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms = str_parms_create_str(keys);
    char value[32];
    int ret = str_parms_get_str(parms, "ec_supported", value, sizeof(value));
    char *str;

    str_parms_destroy(parms);
    if (ret >= 0) {
        parms = str_parms_create_str("ec_supported=yes");
        str = str_parms_to_str(parms);
        str_parms_destroy(parms);
        return str;
    }
#ifdef HAMPOO_WAKE_UP
#if 0
	if(!strcmp(keys, "audio_record_status")){
		char str[50];
		sprintf(str,"%d",audio_record_status);
		return strdup(str); 
	}
#endif
#endif
    return strdup("");
}

/**
 * @brief adev_init_check 
 *
 * @param dev
 *
 * @returns 
 */
static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

/**
 * @brief adev_set_voice_volume 
 *
 * @param dev
 * @param volume
 *
 * @returns 
 */
static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

/**
 * @brief adev_set_master_volume 
 *
 * @param dev
 * @param volume
 *
 * @returns 
 */
static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

/**
 * @brief adev_set_mode 
 *
 * @param dev
 * @param mode
 *
 * @returns 
 */
static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    return 0;
}

/**
 * @brief adev_set_mic_mute 
 *
 * @param dev
 * @param state
 *
 * @returns 
 */
static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    adev->mic_mute = state;

    return 0;
}

/**
 * @brief adev_get_mic_mute 
 *
 * @param dev
 * @param state
 *
 * @returns 
 */
static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;

    return 0;
}

/**
 * @brief adev_get_input_buffer_size 
 *
 * @param dev
 * @param config
 *
 * @returns 
 */
static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{

    return get_input_buffer_size(config->sample_rate, config->format,
                                 audio_channel_count_from_in_mask(config->channel_mask),
                                 false /* is_low_latency: since we don't know, be conservative */);
}

/**
 * @brief adev_open_input_stream 
 *
 * @param dev
 * @param handle
 * @param devices
 * @param config
 * @param stream_in
 * @param flags
 * @param __unused
 * @param __unused
 *
 * @returns 
 */
static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address __unused,
                                  audio_source_t source __unused)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    int ret;



    *stream_in = NULL;
#ifdef ALSA_IN_DEBUG
    in_debug = fopen("/data/debug.pcm","wb");//please touch /data/debug.pcm first
#endif
    /* Respond with a request for mono if a different format is given. */
    //ALOGV("%s:config->channel_mask %d",__FUNCTION__,config->channel_mask);
    if (/*config->channel_mask != AUDIO_CHANNEL_IN_MONO &&
            config->channel_mask != AUDIO_CHANNEL_IN_FRONT_BACK*/ 
            config->channel_mask != AUDIO_CHANNEL_IN_STEREO) {
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
        ALOGE("%s:channel is not support",__FUNCTION__);
        return -EINVAL;
    }

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (!in)
        return -ENOMEM;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    in->dev = adev;
    in->standby = true;
    in->requested_rate = config->sample_rate;
    in->input_source = AUDIO_SOURCE_DEFAULT;
    /* strip AUDIO_DEVICE_BIT_IN to allow bitwise comparisons */
    in->device = devices & ~AUDIO_DEVICE_BIT_IN;
    in->io_handle = handle;
    in->channel_mask = config->channel_mask;
    in->flags = flags;
    struct pcm_config *pcm_config = flags & AUDIO_INPUT_FLAG_FAST ?
            &pcm_config_in_low_latency : &pcm_config_in;
    in->config = pcm_config;

    in->buffer = malloc(pcm_config->period_size * pcm_config->channels
                                               * audio_stream_in_frame_size(&in->stream));
#ifdef SPEEX_DENOISE_ENABLE
    in->mSpeexState = NULL;
    in->mSpeexFrameSize = 0;
    in->mSpeexPcmIn = NULL;
#endif

    if (!in->buffer) {
        ret = -ENOMEM;
        goto err_malloc;
    }

    if (in->requested_rate != pcm_config->rate) {
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;

        ALOGD("pcm_config->rate:%d,in->requested_rate:%d,in->channel_mask:%d",
             pcm_config->rate,in->requested_rate,audio_channel_count_from_in_mask(in->channel_mask));
        ret = create_resampler(pcm_config->rate,
                               in->requested_rate,
                               audio_channel_count_from_in_mask(in->channel_mask),
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
        if (ret != 0) {
            ret = -EINVAL;
            goto err_resampler;
        }
    }

#ifdef SPEEX_DENOISE_ENABLE
    uint32_t size;
    int denoise = 1;
    int noiseSuppress = -24;
    int channel_count = audio_channel_count_from_out_mask(config->channel_mask);

    size = pcm_config->period_size*in->requested_rate/44100;
    size = ((size + 15) / 16) * 16;
    size =  size * channel_count * sizeof(int16_t);

    in->mSpeexFrameSize =size/((channel_count*sizeof(int16_t))*2);
    ALOGD("in->mSpeexFrameSize:%d",in->mSpeexFrameSize);
    in->mSpeexPcmIn = malloc(sizeof(int16_t)*in->mSpeexFrameSize);
    if(!in->mSpeexPcmIn){
        ALOGE("speexPcmIn malloc failed");
        goto err_speex_malloc;
    }
    in->mSpeexState = speex_preprocess_state_init(in->mSpeexFrameSize, in->requested_rate);
    if(in->mSpeexState == NULL)
    {
        ALOGE("speex error");
        goto err_speex_malloc;
    }

    speex_preprocess_ctl(in->mSpeexState, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
    speex_preprocess_ctl(in->mSpeexState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppress);

#endif

    *stream_in = &in->stream;
	
	if(in_pcm_count < HAMPOO_WAKE_UP_RECORD_NUM){
		in_pcm_count++;
	}
	ALOGD("adev_open_input_stream in_pcm_count = %d ", in_pcm_count);
    return 0;

err_speex_malloc:
#ifdef SPEEX_DENOISE_ENABLE
    free(in->mSpeexPcmIn);
#endif
err_resampler:
    free(in->buffer);
err_malloc:
    free(in);
    return ret;
}

/**
 * @brief adev_close_input_stream 
 *
 * @param dev
 * @param stream
 */
static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
#ifdef HAMPOO_WAKE_UP
//APP 退出后资源释放
	if(in_pcm_count>0){
		in_pcm_count--;
	}
	usleep(5 * 1000);
	ALOGE("----adev_close_input_stream in_pcm_count = %d ", in_pcm_count);

#endif
    in_standby(&stream->common);
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
#ifdef ALSA_IN_DEBUG
    fclose(in_debug);
#endif

#ifdef SPEEX_DENOISE_ENABLE
    if (in->mSpeexState) {
        speex_preprocess_state_destroy(in->mSpeexState);
    }
    if(in->mSpeexPcmIn) {
        free(in->mSpeexPcmIn);
    }
#endif
    free(in->buffer);
    free(stream);
}

/**
 * @brief adev_dump 
 *
 * @param device
 * @param fd
 *
 * @returns 
 */
static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

/**
 * @brief adev_close 
 *
 * @param device
 *
 * @returns 
 */
static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;

    //audio_route_free(adev->ar);


    route_uninit();

    if (adev->hdmi_drv_fd >= 0)
        close(adev->hdmi_drv_fd);

    if (hdmi_uevent_t != NULL) {
        pthread_join(hdmi_uevent_t, NULL);
    }
    free(device);
#ifdef HAMPOO_WAKE_UP
#if 0

//APP 退出后资源释放
	native_event_set(g_record_event);//先激活线程再销毁
	native_event_destroy(g_record_event);
	g_record_event = NULL;

	native_event_set(g_resample_event);//先激活线程再销毁
	native_event_destroy(g_resample_event);
	g_resample_event = NULL;
#endif
	

#if 0
    g_record->runing = 0;
    pthread_join(g_record->tid_pcm_read, NULL);
    free(g_record);
	g_record = NULL;
	usleep(10*1000);
	pthread_join(tid_resample_buffer, NULL);
	usleep(10*1000);
	queue_destroy(resampleData.queue);
	resampleData.queue = NULL;
	free(resampleData.queue_buff);
	resampleData.queue_buff = NULL;
	hampoo_resampler_exit();
#endif
	in_pcm_count = 0;
	ALOGV("tid_resample_buffer pthread_join \n");
	queue_destroy(userData.queue);
	userData.queue = NULL;
	free(userData.queue_buff);
	userData.queue_buff = NULL;

	ALOGV("adev_close queue_destroy \n");
#endif
    return 0;
}

/**
 * @brief adev_open 
 *
 * @param module
 * @param name
 * @param device
 *
 * @returns 
 */
pthread_t tid_share_buffer;
static void* share_buffer_thread(void)
{
#if 0
	ALOGD("share_buffer_thread start \n");
	share_buffer_init();//666666
	ALOGD("share_buffer_thread end \n");
#endif
	ALOGD("share_buffer_thread start \n");

	int server_sockfd, client_sockfd;
	int server_len, client_len;
	struct sockaddr_un server_address;		/*声明一个UNIX域套接字结构*/
	struct sockaddr_un client_address;
	int i, bytes;
	char ch_recv[2048];
	int ret;


	while(1)
	{
		/*创建 socket, 通信协议为PF_LOCAL, SCK_STREAM 数据方式*/
		server_sockfd = socket (PF_LOCAL, SOCK_STREAM, 0);
		if(server_sockfd <0 )
		{
			ALOGD("cannot create listening socket\n" );
			continue;				
		}
		else
		{
		
			while(1)
			{
				sleep(2);
				 /*配置服务器信息(通信协议)*/
				server_address.sun_family = AF_LOCAL;

				 /*配置服务器信息(socket 对象)*/
				unlink(LOCAL_SOCKET_NAME);	 /*删除原有server_socket对象*/
				//server_address.sun_path[0] = '\0';
				strcpy (server_address.sun_path, LOCAL_SOCKET_NAME);

				 /*配置服务器信息(服务器地址长度)*/
				  server_len = sizeof(server_address);
				 /*绑定 socket 对象*/
				ret = bind (server_sockfd, (struct sockaddr *)&server_address, server_len);
				if(ret == -1)
				{
					ALOGD(" cannot bind server socket\n" );
					close(server_sockfd);
					unlink (LOCAL_SOCKET_NAME); 
					break;
				}
				 
				 /*监听网络,队列数为10*/
				ret = listen (server_sockfd, 10);
				if(ret == -1)
				{
				   ALOGD("cannot listen the client connect request");  
				   close(server_sockfd);   
				   unlink(LOCAL_SOCKET_NAME);	
				   break;							
				}
				chmod(LOCAL_SOCKET_NAME,00777);//设置通信文件权限 
				client_len = sizeof (client_address);
				ALOGD("Server is waiting for client connect...\n");  
				client_sockfd = accept (server_sockfd, (struct sockaddr *)&client_address, (socklen_t*)&client_len);
				if (client_sockfd == -1) {
				ALOGD("accept failed");  
				close(server_sockfd);	
				unlink(LOCAL_SOCKET_NAME);	
				break;
				}
				memset(ch_recv,0,2048);
				while(1)
				{
					 ret =read(client_sockfd,ch_recv,512);
					 if(ret == -1 || ret == 0)
					 {
						close(client_sockfd);
						close(server_sockfd);	
						unlink(LOCAL_SOCKET_NAME);
						in_pcm =0;
						ALOGD("read = %d\n",ret); 
					
						break;		
					 }
					 else
					 {
					 	in_pcm =1;			
						queue_write(userData.queue, ch_recv,ret);
					 }
					 //if(ret != 512)
					// {
					//	ALOGD(" server read = %d\n",ret); 
					// }						
				} 
			}
		}
	}
	return 0;

} 

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct audio_device *adev;
    int ret;

    ALOGD(AUDIO_HAL_VERSION);
    
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct audio_device));
    if (!adev)
        return -ENOMEM;

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->hw_device.common.module = (struct hw_module_t *) module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;

    //adev->ar = audio_route_init(MIXER_CARD, NULL);
    route_init();

    adev->input_source = AUDIO_SOURCE_DEFAULT;
    /* adev->cur_route_id initial value is 0 and such that first device
     * selection is always applied by select_devices() */

    adev->hdmi_drv_fd = -1;

    *device = &adev->hw_device.common;

    char value[PROPERTY_VALUE_MAX];
    if (property_get("audio_hal.period_size", value, NULL) > 0) {
        pcm_config.period_size = atoi(value);
        pcm_config_in.period_size = pcm_config.period_size;
    }
    if (property_get("audio_hal.in_period_size", value, NULL) > 0)
        pcm_config_in.period_size = atoi(value);

    read_snd_card_info();
#ifdef BOX_HAL
    initchnsta();
    if (!pthread_create(&hdmi_uevent_t, NULL, audio_hdmi_thread, NULL)) {
        ALOGD("pthread_create error");
    }
#endif
#ifdef HAMPOO_WAKE_UP
	ALOGD("init userData queue  \n");

	int size = 2144;
	userData.queue_buff = (char *) malloc(sizeof(audio_queue_t) + size * QUEUE_BUFF_MULTIPLE + 1);
	if (NULL == userData.queue_buff){
		return -1;
	}
//新建队列queue2
	userData.queue = queue_init(userData.queue_buff, size * QUEUE_BUFF_MULTIPLE + 1);
	if (NULL == userData.queue){
		return -1;
	}
	pthread_create(&tid_share_buffer, NULL, share_buffer_thread, NULL);
	property_set("persist.sys.halspeek","0");
	property_set("persist.sys.ktvspeek","0");
#if 0
		resampleData.queue_buff = (char *) malloc(sizeof(audio_queue_t) + size * QUEUE_BUFF_MULTIPLE + 1);
		if (NULL == resampleData.queue_buff){
			return -1;
		}
	//新建队列queue2
		resampleData.queue = queue_init(resampleData.queue_buff, size * QUEUE_BUFF_MULTIPLE + 1);
		if (NULL == resampleData.queue){
			return -1;
		}

    g_record = (RecordData *)malloc(sizeof(RecordData));
	if (NULL == g_record){
		return -1;
	}
	memset(g_record, 0, sizeof(RecordData));


	 hampoo_resampler_init();

	
	pthread_attr_t thread_attr;
	struct sched_param thread_param;
	pthread_attr_init(&thread_attr);
	pthread_attr_setschedpolicy(&thread_attr, SCHED_RR);
	thread_param.sched_priority = sched_get_priority_max(SCHED_RR);
	pthread_attr_setschedparam(&thread_attr, &thread_param);
	g_record->runing = 1;
	g_record_event = native_event_create("record_event", NULL);
	pthread_create(&g_record->tid_pcm_read, &thread_attr, RecordThread, (void*)g_record);
	g_resample_event = native_event_create("resample_event", NULL);
	pthread_create(&tid_resample_buffer, NULL, resample_buffer_thread, NULL);
#endif

#endif
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Manta audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
