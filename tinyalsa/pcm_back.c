/* pcm.c
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <limits.h>
#include <cutils/log.h>
#include <linux/ioctl.h>
#define __force
#define __bitwise
#define __user
#include <sound/asound.h>
#include <android/log.h>
#include <tinyalsa/asoundlib.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#include "cutils/properties.h"

#define PARAM_MAX SNDRV_PCM_HW_PARAM_LAST_INTERVAL
#define SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP (1<<2)

/* Logs information into a string; follows snprintf() in that
 * offset may be greater than size, and though no characters are copied
 * into string, characters are still counted into offset. */
#define STRLOG(string, offset, size, ...) \
    do { int temp, clipoffset = offset > size ? size : offset; \
         temp = snprintf(string + clipoffset, size - clipoffset, __VA_ARGS__); \
         if (temp > 0) offset += temp; } while (0)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

//#define LOG_TAG "TINY-ALSA"

//#define ALOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
//#define ALOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args)
/* refer to SNDRV_PCM_ACCESS_##index in sound/asound.h. */
static const char * const access_lookup[] = {
        "MMAP_INTERLEAVED",
        "MMAP_NONINTERLEAVED",
        "MMAP_COMPLEX",
        "RW_INTERLEAVED",
        "RW_NONINTERLEAVED",
};

/* refer to SNDRV_PCM_FORMAT_##index in sound/asound.h. */
static const char * const format_lookup[] = {
        /*[0] =*/ "S8",
        "U8",
        "S16_LE",
        "S16_BE",
        "U16_LE",
        "U16_BE",
        "S24_LE",
        "S24_BE",
        "U24_LE",
        "U24_BE",
        "S32_LE",
        "S32_BE",
        "U32_LE",
        "U32_BE",
        "FLOAT_LE",
        "FLOAT_BE",
        "FLOAT64_LE",
        "FLOAT64_BE",
        "IEC958_SUBFRAME_LE",
        "IEC958_SUBFRAME_BE",
        "MU_LAW",
        "A_LAW",
        "IMA_ADPCM",
        "MPEG",
        /*[24] =*/ "GSM",
        /* gap */
        [31] = "SPECIAL",
        "S24_3LE",
        "S24_3BE",
        "U24_3LE",
        "U24_3BE",
        "S20_3LE",
        "S20_3BE",
        "U20_3LE",
        "U20_3BE",
        "S18_3LE",
        "S18_3BE",
        "U18_3LE",
        /*[43] =*/ "U18_3BE",
#if 0
        /* recent additions, may not be present on local asound.h */
        "G723_24",
        "G723_24_1B",
        "G723_40",
        "G723_40_1B",
        "DSD_U8",
        "DSD_U16_LE",
#endif
};

/* refer to SNDRV_PCM_SUBFORMAT_##index in sound/asound.h. */
static const char * const subformat_lookup[] = {
        "STD",
};
#define MUTI_CAP 1
#ifdef MUTI_CAP
#include <sys/syscall.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>

#define WAIT_CAP_BUFFER_TIMER_US 1000 * 30

#define SERV_PORT 8888
#define SERVER_IP "127.0.0.1"
static int socketid_s = -1;
static int socketid_c = -1;
static struct sockaddr_in addr_s;
static struct sockaddr_in addr_c;

static pid_t pid_s = -1;
static pid_t pid_c = -1;

static void * databuffer = NULL;
#define RECORD_NUM 8
static pid_t tid_socket_c[RECORD_NUM][2];
static int first_enter=1;
static int current_audio_num=0;

#endif

static inline int param_is_mask(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline int param_is_interval(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL);
}

static inline struct snd_interval *param_to_interval(struct snd_pcm_hw_params *p, int n)
{
    return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
    return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned int bit)
{
    if (bit >= SNDRV_MASK_MAX)
        return;
    if (param_is_mask(n)) {
        struct snd_mask *m = param_to_mask(p, n);
        m->bits[0] = 0;
        m->bits[1] = 0;
        m->bits[bit >> 5] |= (1 << (bit & 31));
    }
}

static void param_set_min(struct snd_pcm_hw_params *p, int n, unsigned int val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
    }
}

static unsigned int param_get_min(struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        return i->min;
    }
    return 0;
}

static void param_set_max(struct snd_pcm_hw_params *p, int n, unsigned int val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->max = val;
    }
}

static unsigned int param_get_max(struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        return i->max;
    }
    return 0;
}

static void param_set_int(struct snd_pcm_hw_params *p, int n, unsigned int val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
        i->max = val;
        i->integer = 1;
    }
}

static unsigned int param_get_int(struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        if (i->integer)
            return i->max;
    }
    return 0;
}

static void param_set_flag(struct snd_pcm_hw_params *p, unsigned int flag)
{
    if (p != NULL) {
        p->flags = flag;
    }
}

static int param_get_flag(struct snd_pcm_hw_params *p)
{
    if (p != NULL) {
        return p->flags;
    }
    return 0;
}


static void param_init(struct snd_pcm_hw_params *p)
{
    int n;

    memset(p, 0, sizeof(*p));
    for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
         n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++) {
            struct snd_mask *m = param_to_mask(p, n);
            m->bits[0] = ~0;
            m->bits[1] = ~0;
    }
    for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
            struct snd_interval *i = param_to_interval(p, n);
            i->min = 0;
            i->max = ~0;
    }
    p->rmask = ~0U;
    p->cmask = 0;
    p->info = ~0U;
}

#define PCM_ERROR_MAX 128

struct pcm {
    int fd;
    unsigned int flags;
    int running:1;
    int prepared:1;
    int underruns;
    unsigned int buffer_size;
    unsigned int boundary;
    char error[PCM_ERROR_MAX];
    struct pcm_config config;
    struct snd_pcm_mmap_status *mmap_status;
    struct snd_pcm_mmap_control *mmap_control;
    struct snd_pcm_sync_ptr *sync_ptr;
    void *mmap_buffer;
    unsigned int noirq_frames_per_msec;
    int wait_for_avail_min;
};

unsigned int pcm_get_buffer_size(struct pcm *pcm)
{
    return pcm->buffer_size;
}

const char* pcm_get_error(struct pcm *pcm)
{
    return pcm->error;
}

static int oops(struct pcm *pcm, int e, const char *fmt, ...)
{
    va_list ap;
    int sz;

    va_start(ap, fmt);
    vsnprintf(pcm->error, PCM_ERROR_MAX, fmt, ap);
    va_end(ap);
    sz = strlen(pcm->error);

    if (errno)
        snprintf(pcm->error + sz, PCM_ERROR_MAX - sz,
                 ": %s", strerror(e));
    return -1;
}

static unsigned int pcm_format_to_alsa(enum pcm_format format)
{
    switch (format) {
    case PCM_FORMAT_S32_LE:
        return SNDRV_PCM_FORMAT_S32_LE;
    case PCM_FORMAT_S8:
        return SNDRV_PCM_FORMAT_S8;
    case PCM_FORMAT_S24_3LE:
        return SNDRV_PCM_FORMAT_S24_3LE;
    case PCM_FORMAT_S24_LE:
        return SNDRV_PCM_FORMAT_S24_LE;
    default:
    case PCM_FORMAT_S16_LE:
        return SNDRV_PCM_FORMAT_S16_LE;
    };
}

unsigned int pcm_format_to_bits(enum pcm_format format)
{
    switch (format) {
    case PCM_FORMAT_S32_LE:
    case PCM_FORMAT_S24_LE:
        return 32;
    case PCM_FORMAT_S24_3LE:
        return 24;
    default:
    case PCM_FORMAT_S16_LE:
        return 16;
    };
}

unsigned int pcm_bytes_to_frames(struct pcm *pcm, unsigned int bytes)
{
    return bytes / (pcm->config.channels *
        (pcm_format_to_bits(pcm->config.format) >> 3));
}

unsigned int pcm_frames_to_bytes(struct pcm *pcm, unsigned int frames)
{
    return frames * pcm->config.channels *
        (pcm_format_to_bits(pcm->config.format) >> 3);
}

static int pcm_sync_ptr(struct pcm *pcm, int flags) {
    if (pcm->sync_ptr) {
        pcm->sync_ptr->flags = flags;
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SYNC_PTR, pcm->sync_ptr) < 0)
            return -1;
    }
    return 0;
}

static int pcm_hw_mmap_status(struct pcm *pcm) {

    if (pcm->sync_ptr)
        return 0;

    int page_size = sysconf(_SC_PAGE_SIZE);
    pcm->mmap_status = mmap(NULL, page_size, PROT_READ, MAP_FILE | MAP_SHARED,
                            pcm->fd, SNDRV_PCM_MMAP_OFFSET_STATUS);
    if (pcm->mmap_status == MAP_FAILED)
        pcm->mmap_status = NULL;
    if (!pcm->mmap_status)
        goto mmap_error;

    pcm->mmap_control = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                             MAP_FILE | MAP_SHARED, pcm->fd, SNDRV_PCM_MMAP_OFFSET_CONTROL);
    if (pcm->mmap_control == MAP_FAILED)
        pcm->mmap_control = NULL;
    if (!pcm->mmap_control) {
        munmap(pcm->mmap_status, page_size);
        pcm->mmap_status = NULL;
        goto mmap_error;
    }
    if (pcm->flags & PCM_MMAP)
        pcm->mmap_control->avail_min = pcm->config.avail_min;
    else
        pcm->mmap_control->avail_min = 1;

    return 0;

mmap_error:

    pcm->sync_ptr = calloc(1, sizeof(*pcm->sync_ptr));
    if (!pcm->sync_ptr)
        return -ENOMEM;
    pcm->mmap_status = &pcm->sync_ptr->s.status;
    pcm->mmap_control = &pcm->sync_ptr->c.control;
    if (pcm->flags & PCM_MMAP)
        pcm->mmap_control->avail_min = pcm->config.avail_min;
    else
        pcm->mmap_control->avail_min = 1;

    pcm_sync_ptr(pcm, 0);

    return 0;
}

static void pcm_hw_munmap_status(struct pcm *pcm) {
    if (pcm->sync_ptr) {
        free(pcm->sync_ptr);
        pcm->sync_ptr = NULL;
    } else {
        int page_size = sysconf(_SC_PAGE_SIZE);
        if (pcm->mmap_status)
            munmap(pcm->mmap_status, page_size);
        if (pcm->mmap_control)
            munmap(pcm->mmap_control, page_size);
    }
    pcm->mmap_status = NULL;
    pcm->mmap_control = NULL;
}

static int pcm_areas_copy(struct pcm *pcm, unsigned int pcm_offset,
                          char *buf, unsigned int src_offset,
                          unsigned int frames)
{
    int size_bytes = pcm_frames_to_bytes(pcm, frames);
    int pcm_offset_bytes = pcm_frames_to_bytes(pcm, pcm_offset);
    int src_offset_bytes = pcm_frames_to_bytes(pcm, src_offset);

    /* interleaved only atm */
    if (pcm->flags & PCM_IN)
        memcpy(buf + src_offset_bytes,
               (char*)pcm->mmap_buffer + pcm_offset_bytes,
               size_bytes);
    else
        memcpy((char*)pcm->mmap_buffer + pcm_offset_bytes,
               buf + src_offset_bytes,
               size_bytes);
    return 0;
}

static int pcm_mmap_transfer_areas(struct pcm *pcm, char *buf,
                                unsigned int offset, unsigned int size)
{
    void *pcm_areas;
    int commit;
    unsigned int pcm_offset, frames, count = 0;

    while (size > 0) {
        frames = size;
        pcm_mmap_begin(pcm, &pcm_areas, &pcm_offset, &frames);
        pcm_areas_copy(pcm, pcm_offset, buf, offset, frames);
        commit = pcm_mmap_commit(pcm, pcm_offset, frames);
        if (commit < 0) {
            oops(pcm, commit, "failed to commit %d frames\n", frames);
            return commit;
        }

        offset += commit;
        count += commit;
        size -= commit;
    }
    return count;
}

int pcm_get_htimestamp(struct pcm *pcm, unsigned int *avail,
                       struct timespec *tstamp)
{
    int frames;
    int rc;
    snd_pcm_uframes_t hw_ptr;

    if (!pcm_is_ready(pcm))
        return -1;

    rc = pcm_sync_ptr(pcm, SNDRV_PCM_SYNC_PTR_APPL|SNDRV_PCM_SYNC_PTR_HWSYNC);
    if (rc < 0)
        return -1;

    if ((pcm->mmap_status->state != PCM_STATE_RUNNING) &&
            (pcm->mmap_status->state != PCM_STATE_DRAINING))
        return -1;

    *tstamp = pcm->mmap_status->tstamp;
    if (tstamp->tv_sec == 0 && tstamp->tv_nsec == 0)
        return -1;

    hw_ptr = pcm->mmap_status->hw_ptr;
    if (pcm->flags & PCM_IN)
        frames = hw_ptr - pcm->mmap_control->appl_ptr;
    else
        frames = hw_ptr + pcm->buffer_size - pcm->mmap_control->appl_ptr;

    if (frames < 0)
        frames += pcm->boundary;
    else if (frames > (int)pcm->boundary)
        frames -= pcm->boundary;

    *avail = (unsigned int)frames;

    return 0;
}

int pcm_write(struct pcm *pcm, const void *data, unsigned int count)
{
    struct snd_xferi x;

    if (pcm->flags & PCM_IN)
        return -EINVAL;

    x.buf = (void*)data;
    x.frames = count / (pcm->config.channels *
                        pcm_format_to_bits(pcm->config.format) / 8);

    for (;;) {
        if (!pcm->running) {
            int prepare_error = pcm_prepare(pcm);
            if (prepare_error)
                return prepare_error;
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x))
                return oops(pcm, errno, "cannot write initial data");
            pcm->running = 1;
            return 0;
        }
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x)) {
            pcm->prepared = 0;
            pcm->running = 0;
            if (errno == EPIPE) {
                /* we failed to make our window -- try to restart if we are
                 * allowed to do so.  Otherwise, simply allow the EPIPE error to
                 * propagate up to the app level */
                pcm->underruns++;
                if (pcm->flags & PCM_NORESTART)
                    return -EPIPE;
                continue;
            }
            return oops(pcm, errno, "cannot write stream data");
        }
        return 0;
    }
}

/********************************
 *  author:charles chen
 *  data:2012.09.27
 *  parameter 
 *  data: the input data buf point
 *  len:   the input data len need consider the pcm_format
 *  ret: 0:Left and right channel is valid
 *  1:Left      channel is valid
 *  2:Right    channel is valid
 *
 *  defalt the input signal is like LRLRLR,default pcm_format is 16bit
 **********************************/
#define SAMPLECOUNT 441*5*2*2
int channalFlags = -1;//mean the channel is not checked now

int startCheckCount = 0;

int channel_check(void * data, unsigned len)
{
    short * pcmLeftChannel = (short *)data;
    short * pcmRightChannel = pcmLeftChannel+1;
    unsigned index = 0;
    int leftValid = 0x0;
    int rightValid = 0x0;
    short checkValue = 0;

    checkValue = *pcmLeftChannel;

    for(index = 0; index < len; index += 2)
    {

        if((pcmLeftChannel[index] >= checkValue+50)||(pcmLeftChannel[index] <= checkValue-50))
        {
            leftValid++;// = 0x01;
        }   
    }

    if(leftValid >20)
        leftValid = 0x01;
    else
        leftValid = 0;
    checkValue = *pcmRightChannel;

    for(index = 0; index < len; index += 2)
    {

        if((pcmRightChannel[index] >= checkValue+50)||(pcmRightChannel[index] <= checkValue-50))
        {
            rightValid++;//= 0x02;
        }   
    }

    if(rightValid >20)
        rightValid = 0x02;
    else
        rightValid = 0;
    return leftValid|rightValid;
}

void channel_fixed(void * data, unsigned len, int chFlag)
{
    if(chFlag <= 0 || chFlag > 2 )
        return;

    short * pcmValid = (short *)data;
    short * pcmInvalid = pcmValid;

    if(chFlag == 1)
        pcmInvalid += 1;
    else if (chFlag == 2)
        pcmValid += 1;

    unsigned index ;

    for(index = 0; index < len; index += 2)
    {
        pcmInvalid[index] = pcmValid[index];
    }
    return;
}



int pcm_read(struct pcm *pcm, void *data, unsigned int count)
{
    struct snd_xferi x;
	pid_t tid_c;
	int i;
    if (!(pcm->flags & PCM_IN))
        return -EINVAL;
	
#ifdef MUTI_CAP
	////ALOGD("-lfy-vicent-vicent-----------------------socketid_c = %d\n",socketid_c);
	////ALOGD("-lfy-vicent-vicent-----------------------pid_c = %d\n",pid_c);
	////ALOGD("-lfy-vicent-vicent-----------------------socketid_s = %d\n",socketid_s);
	////ALOGD("-lfy-vicent-vicent-----------------------pid_s = %d tid_c:%d\n",pid_s,syscall(SYS_gettid));
	tid_c = syscall(SYS_gettid);
	//ALOGD("-lfy-vicent-stone:current pcm_read tid:%d",tid_c);
	//ALOGD("-lfy-vicent-stone:current pcm_read count:%d",count);
	for(i=0;i<current_audio_num;i++){
		if(tid_c == tid_socket_c[i][0] && tid_socket_c[i][1]!=-1){
			int recbyte = 0;
			int addr_len = sizeof(struct sockaddr_in);
			int read_size = pcm->config.period_size * pcm->config.period_count * 4;
			//ALOGD("-lfy-vicent-current_audio_num  :%d",current_audio_num);
			//memset(databuffer, 0, read_size);
			recbyte = recvfrom(tid_socket_c[i][1], databuffer, read_size,  0 , (struct sockaddr *)&addr_c ,&addr_len);
			//ALOGD("-lfy-vicent-vicent pcm_read socketid_c:%d tid:%d----------------recbyte = %d\n",tid_socket_c[i][1],tid_socket_c[i][0],recbyte);
			//ALOGD("-lfy-vicent-vicent pcm_read socketid_c----------------count = %d\n",count);
			if(recbyte > 0){
				memcpy(data,databuffer,count);
				//memset(databuffer, 0, count);
				}
			else
				memcpy(data,databuffer,count);
			return 0;
			}
		}

#endif
    x.buf = data;
    x.frames = count / (pcm->config.channels *
                        pcm_format_to_bits(pcm->config.format) / 8);

    for (;;) {
        if (!pcm->running) {
            if (pcm_start(pcm) < 0) {
				//ALOGD("-lfy-vicent-stone:pcm start errno:%d",errno);
                fprintf(stderr, "start error");
                return -errno;
            }
        }
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x)) {
			//ALOGD("-lfy-vicent-stone:get frames for kernel failed:%d",errno);
            pcm->prepared = 0;
            pcm->running = 0;
            if (errno == EPIPE) {
                    /* we failed to make our window -- try to restart */
                pcm->underruns++;
                continue;
            }
            return oops(pcm, errno, "cannot read stream data");
        }
        if(!(pcm->config.channels == 1))
        {
            if(channalFlags == -1 ) 
            {
                if(startCheckCount < SAMPLECOUNT)
                {
                    startCheckCount += count;
                }
                else
                {
                    channalFlags = channel_check(data,count/2);
                }
            }//if(channalFlags == -1)

            channel_fixed(data,count/2, channalFlags);
        }
#ifdef MUTI_CAP		
		if(socketid_s != -1 && pid_s == getpid()){
			int senbyte = 0;
			senbyte = sendto(socketid_s, data, count, 0, (struct sockaddr *)&addr_s,sizeof(struct sockaddr_in));
			ALOGD("-lfy-vicent-vicent pcm_read socketid_s-----------------senbyte = %d\n",senbyte);
			ALOGD("-lfy-vicent-vicent pcm_read socketid_c----------------count = %d\n",count);
		}
#endif	
	//ALOGD("-lfy-vicent-vicent pcm_read end----------\n");
        return 0;
    }
}

static struct pcm bad_pcm = {
    .fd = -1,
};

struct pcm_params *pcm_params_get(unsigned int card, unsigned int device,
                                  unsigned int flags)
{
    struct snd_pcm_hw_params *params;
    char fn[256];
    int fd;

    snprintf(fn, sizeof(fn), "/dev/snd/pcmC%uD%u%c", card, device,
             flags & PCM_IN ? 'c' : 'p');

    fd = open(fn, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "cannot open device '%s'\n", fn);
        goto err_open;
    }

    params = calloc(1, sizeof(struct snd_pcm_hw_params));
    if (!params)
        goto err_calloc;

    param_init(params);
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, params)) {
        fprintf(stderr, "SNDRV_PCM_IOCTL_HW_REFINE error (%d)\n", errno);
        goto err_hw_refine;
    }

    close(fd);

    return (struct pcm_params *)params;

err_hw_refine:
    free(params);
err_calloc:
    close(fd);
err_open:
    return NULL;
}

void pcm_params_free(struct pcm_params *pcm_params)
{
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;

    if (params)
        free(params);
}

static int pcm_param_to_alsa(enum pcm_param param)
{
    switch (param) {
    case PCM_PARAM_ACCESS:
        return SNDRV_PCM_HW_PARAM_ACCESS;
    case PCM_PARAM_FORMAT:
        return SNDRV_PCM_HW_PARAM_FORMAT;
    case PCM_PARAM_SUBFORMAT:
        return SNDRV_PCM_HW_PARAM_SUBFORMAT;
    case PCM_PARAM_SAMPLE_BITS:
        return SNDRV_PCM_HW_PARAM_SAMPLE_BITS;
        break;
    case PCM_PARAM_FRAME_BITS:
        return SNDRV_PCM_HW_PARAM_FRAME_BITS;
        break;
    case PCM_PARAM_CHANNELS:
        return SNDRV_PCM_HW_PARAM_CHANNELS;
        break;
    case PCM_PARAM_RATE:
        return SNDRV_PCM_HW_PARAM_RATE;
        break;
    case PCM_PARAM_PERIOD_TIME:
        return SNDRV_PCM_HW_PARAM_PERIOD_TIME;
        break;
    case PCM_PARAM_PERIOD_SIZE:
        return SNDRV_PCM_HW_PARAM_PERIOD_SIZE;
        break;
    case PCM_PARAM_PERIOD_BYTES:
        return SNDRV_PCM_HW_PARAM_PERIOD_BYTES;
        break;
    case PCM_PARAM_PERIODS:
        return SNDRV_PCM_HW_PARAM_PERIODS;
        break;
    case PCM_PARAM_BUFFER_TIME:
        return SNDRV_PCM_HW_PARAM_BUFFER_TIME;
        break;
    case PCM_PARAM_BUFFER_SIZE:
        return SNDRV_PCM_HW_PARAM_BUFFER_SIZE;
        break;
    case PCM_PARAM_BUFFER_BYTES:
        return SNDRV_PCM_HW_PARAM_BUFFER_BYTES;
        break;
    case PCM_PARAM_TICK_TIME:
        return SNDRV_PCM_HW_PARAM_TICK_TIME;
        break;

    default:
        return -1;
    }
}

struct pcm_mask *pcm_params_get_mask(struct pcm_params *pcm_params,
                                     enum pcm_param param)
{
    int p;
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
    if (params == NULL) {
        return NULL;
    }

    p = pcm_param_to_alsa(param);
    if (p < 0 || !param_is_mask(p)) {
        return NULL;
    }

    return (struct pcm_mask *)param_to_mask(params, p);
}

unsigned int pcm_params_get_min(struct pcm_params *pcm_params,
                                enum pcm_param param)
{
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
    int p;

    if (!params)
        return 0;

    p = pcm_param_to_alsa(param);
    if (p < 0)
        return 0;

    return param_get_min(params, p);
}

void pcm_params_set_min(struct pcm_params *pcm_params,
                                enum pcm_param param, unsigned int val)
{
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
    int p;

    if (!params)
        return;

    p = pcm_param_to_alsa(param);
    if (p < 0)
        return;

    param_set_min(params, p, val);
}

unsigned int pcm_params_get_max(struct pcm_params *pcm_params,
                                enum pcm_param param)
{
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
    int p;

    if (!params)
        return 0;

    p = pcm_param_to_alsa(param);
    if (p < 0)
        return 0;

    return param_get_max(params, p);
}

void pcm_params_set_max(struct pcm_params *pcm_params,
                                enum pcm_param param, unsigned int val)
{
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
    int p;

    if (!params)
        return;

    p = pcm_param_to_alsa(param);
    if (p < 0)
        return;

    param_set_max(params, p, val);
}

static int pcm_mask_test(struct pcm_mask *m, unsigned int index)
{
    const unsigned int bitshift = 5; /* for 32 bit integer */
    const unsigned int bitmask = (1 << bitshift) - 1;
    unsigned int element;

    element = index >> bitshift;
    if (element >= ARRAY_SIZE(m->bits))
        return 0; /* for safety, but should never occur */
    return (m->bits[element] >> (index & bitmask)) & 1;
}

static int pcm_mask_to_string(struct pcm_mask *m, char *string, unsigned int size,
                              char *mask_name,
                              const char * const *bit_array_name, size_t bit_array_size)
{
    unsigned int i;
    unsigned int offset = 0;

    if (m == NULL)
        return 0;
    if (bit_array_size < 32) {
        STRLOG(string, offset, size, "%12s:\t%#08x\n", mask_name, m->bits[0]);
    } else { /* spans two or more bitfields, print with an array index */
        for (i = 0; i < (bit_array_size + 31) >> 5; ++i) {
            STRLOG(string, offset, size, "%9s[%d]:\t%#08x\n",
                   mask_name, i, m->bits[i]);
        }
    }
    for (i = 0; i < bit_array_size; ++i) {
        if (pcm_mask_test(m, i)) {
            STRLOG(string, offset, size, "%12s \t%s\n", "", bit_array_name[i]);
        }
    }
    return offset;
}

int pcm_params_to_string(struct pcm_params *params, char *string, unsigned int size)
{
    struct pcm_mask *m;
    unsigned int min, max;
    unsigned int clipoffset, offset;

    m = pcm_params_get_mask(params, PCM_PARAM_ACCESS);
    offset = pcm_mask_to_string(m, string, size,
                                 "Access", access_lookup, ARRAY_SIZE(access_lookup));
    m = pcm_params_get_mask(params, PCM_PARAM_FORMAT);
    clipoffset = offset > size ? size : offset;
    offset += pcm_mask_to_string(m, string + clipoffset, size - clipoffset,
                                 "Format", format_lookup, ARRAY_SIZE(format_lookup));
    m = pcm_params_get_mask(params, PCM_PARAM_SUBFORMAT);
    clipoffset = offset > size ? size : offset;
    offset += pcm_mask_to_string(m, string + clipoffset, size - clipoffset,
                                 "Subformat", subformat_lookup, ARRAY_SIZE(subformat_lookup));
    min = pcm_params_get_min(params, PCM_PARAM_RATE);
    max = pcm_params_get_max(params, PCM_PARAM_RATE);
    STRLOG(string, offset, size, "        Rate:\tmin=%uHz\tmax=%uHz\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_CHANNELS);
    max = pcm_params_get_max(params, PCM_PARAM_CHANNELS);
    STRLOG(string, offset, size, "    Channels:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_SAMPLE_BITS);
    max = pcm_params_get_max(params, PCM_PARAM_SAMPLE_BITS);
    STRLOG(string, offset, size, " Sample bits:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_PERIOD_SIZE);
    max = pcm_params_get_max(params, PCM_PARAM_PERIOD_SIZE);
    STRLOG(string, offset, size, " Period size:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_PERIODS);
    max = pcm_params_get_max(params, PCM_PARAM_PERIODS);
    STRLOG(string, offset, size, "Period count:\tmin=%u\t\tmax=%u\n", min, max);
    return offset;
}

int pcm_params_format_test(struct pcm_params *params, enum pcm_format format)
{
    unsigned int alsa_format = pcm_format_to_alsa(format);

    if (alsa_format == SNDRV_PCM_FORMAT_S16_LE && format != PCM_FORMAT_S16_LE)
        return 0; /* caution: format not recognized is equivalent to S16_LE */
    return pcm_mask_test(pcm_params_get_mask(params, PCM_PARAM_FORMAT), alsa_format);
}

int pcm_close(struct pcm *pcm)
{
	pid_t tid_c;
	int move_tid_socket_c=0,i;
#ifdef MUTI_CAP	
	if(pcm->flags == PCM_IN){
		tid_c = syscall(SYS_gettid);
		ALOGD("-lfy-vicent-stone:pcm close tid:%d",tid_c);
		for(i=0;i<current_audio_num;i++){
			if(tid_c==tid_socket_c[i][0] && tid_socket_c[i][1]!=-1){
				close(tid_socket_c[i][1]);
				move_tid_socket_c=1;
				}
			if(move_tid_socket_c){
				tid_socket_c[i][0]=tid_socket_c[i+1][0];
				tid_socket_c[i][1]=tid_socket_c[i+1][1];
				}
			}
		if(move_tid_socket_c){
			current_audio_num--;
			ALOGD("-lfy-pcm_close-current_audio_num = %d\n",current_audio_num);	
			pcm->fd=-1;
			free(pcm);
			if(current_audio_num==0){
				free(databuffer);
				databuffer=NULL;
				}
			return 0;
			}
		if(socketid_s != -1 && pid_s == getpid()){
			pid_s = -1;
			close(socketid_s);
			socketid_s = -1;
			ALOGE("-lfy-vicent-vicent socketid_s------pcm close ----------------close\n");
		}
	}	

#endif	
    if (pcm == &bad_pcm)
        return 0;

    pcm_hw_munmap_status(pcm);

    if (pcm->flags & PCM_MMAP) {
        pcm_stop(pcm);
        munmap(pcm->mmap_buffer, pcm_frames_to_bytes(pcm, pcm->buffer_size));
    }

    if (pcm->fd >= 0)
        close(pcm->fd);
    pcm->prepared = 0;
    pcm->running = 0;
    pcm->buffer_size = 0;
    pcm->fd = -1;
    free(pcm);
    return 0;
}

struct pcm *pcm_open(unsigned int card, unsigned int device,
                     unsigned int flags, struct pcm_config *config)
{
    struct pcm *pcm;
    struct snd_pcm_info info;
    struct snd_pcm_hw_params params;
    struct snd_pcm_sw_params sparams;
    char fn[256];
    char lfy_prop[256];
    int rc;
	int i,j;
	pid_t tid_c;
	
    pcm = calloc(1, sizeof(struct pcm));
    if (!pcm || !config)
        return &bad_pcm; /* TODO: could support default config here */

    pcm->config = *config;

    snprintf(fn, sizeof(fn), "/dev/snd/pcmC%uD%u%c", card, device,
             flags & PCM_IN ? 'c' : 'p');

    pcm->flags = flags;
#ifdef MUTI_CAP
	if(flags & PCM_IN){
		
		tid_c = syscall(SYS_gettid);
		ALOGD("-lfy-vicent-vicent-----------------------pid = %d\n",getpid());
		ALOGD("-lfy-vicent-vicent-----------------------tid = %ld\n",tid_c);
		
		pcm->fd = open(fn, O_RDWR | O_NONBLOCK);
		if(errno == EBUSY && pcm->fd < 0){
			
			int optval = 1;  
			struct timeval ti;   
	        ti.tv_sec=0;  
	        ti.tv_usec=WAIT_CAP_BUFFER_TIMER_US;
			
			pid_c = getpid(); 
			//ALOGD("-lfy-vicent-vicent-----------------------pid_c = %d\n",pid_c);
			
			ALOGD("-lfy-vicent-vicent-----------------------just build a client\n");
			if((socketid_c=socket(AF_INET,SOCK_DGRAM,0))<0){
			//if((socketid_c=socket(AF_INET,SOCK_STREAM,0))<0){
				//ALOGD("-lfy-vicent-vicent socket----------------------faild");
			}
			setsockopt(socketid_c, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));  
			bzero (&addr_c, sizeof(addr_c));
			addr_c.sin_family=AF_INET;
			addr_c.sin_port=htons(SERV_PORT);
			addr_c.sin_addr.s_addr=htonl(INADDR_ANY) ;
			if (bind(socketid_c, (struct sockaddr *)&addr_c, sizeof(addr_c))<0){
				//ALOGD("-lfy-vicent-vicent connect----------------------faild");
			}
			setsockopt(socketid_c,SOL_SOCKET,SO_RCVTIMEO,&ti,sizeof(ti));
			
			if(databuffer==NULL){
				databuffer = (void *)malloc(config->period_size * config->period_count * 4);
				memset(databuffer, 0 , config->period_size * config->period_count * 4);
				}
			pcm->fd = 99;
			pcm->flags = PCM_IN,
			pcm->config.channels = config->channels;
			pcm->config.rate = config->rate;
			pcm->config.period_size = config->period_size;
			pcm->config.period_count = config->period_count;
			pcm->buffer_size = config->period_count * config->period_size;
			
			//ALOGD("-lfy-vicent-vicent-----------------------channels = %d\n",pcm->config.channels);
			//ALOGD("-lfy-vicent-vicent-----------------------rate = %d\n",pcm->config.rate);
			//ALOGD("-lfy-vicent-vicent-----------------------period_size = %d\n",pcm->config.period_size);
			//ALOGD("-lfy-vicent-vicent-----------------------period_count = %d\n",pcm->config.period_count);
			tid_socket_c[current_audio_num][0]= tid_c;
			tid_socket_c[current_audio_num][1]= socketid_c;
			ALOGD("-lfy-pcm_open-current_audio_num = %d\n",current_audio_num);
			current_audio_num++;
			ALOGD("-lfy-pcm_open-current_audio_num++ = %d\n",current_audio_num);
			return pcm;
		}else  if(pcm->fd >= 0){
			close(pcm->fd);
			pid_s = getpid();
			ALOGD("-lfy-vicent-vicent-----------------------build a server\n");
			ALOGD("-lfy-vicent-vicent-----------------------pid_s = %d\n",pid_s);
			ALOGD("-lfy-vicent-vicent-----------------------channels = %d\n",pcm->config.channels);
			ALOGD("-lfy-vicent-vicent-----------------------rate = %d\n",pcm->config.rate);
			ALOGD("-lfy-vicent-vicent-----------------------period_size = %d\n",pcm->config.period_size);
			ALOGD("-lfy-vicent-vicent-----------------------period_count = %d\n",pcm->config.period_count);
			ALOGD("-lfy-vicent-vicent-----------------------format = %d\n",pcm->config.format);
			ALOGD("-lfy-vicent-vicent-----------------------start_threshold = %d\n",pcm->config.start_threshold);
			ALOGD("-lfy-vicent-vicent-----------------------stop_threshold = %d\n",pcm->config.stop_threshold);
			ALOGD("-lfy-vicent-vicent-----------------------silence_threshold = %d\n",pcm->config.silence_threshold);
			ALOGD("-lfy-vicent-vicent-----------------------silence_size = %d\n",pcm->config.silence_size);
			ALOGD("-lfy-vicent-vicent-----------------------avail_min = %d\n",pcm->config.avail_min);
			ALOGD("-lfy-vicent-vicent-----------------------flag = %d\n",pcm->config.flag);
			if((socketid_s = socket(AF_INET,SOCK_DGRAM,0))<0){
			//if((socketid_s = socket(AF_INET,SOCK_STREAM,0))<0){
				ALOGD("-lfy-vicent-vicent--------socket");
			}
			int optval = 1;
			setsockopt(socketid_s, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));  
			//int bBroadcast = 1;
			//setsockopt(socketid_s, SOL_SOCKET, SO_BROADCAST, &bBroadcast,sizeof(int));

			bzero(&addr_s,sizeof(addr_s));
			addr_s.sin_family = AF_INET;
			addr_s.sin_port = htons(SERV_PORT);
			addr_s.sin_addr.s_addr = inet_addr(SERVER_IP);
		}
	}
#endif

    pcm->fd = open(fn, O_RDWR);
    if (pcm->fd < 0) {
        oops(pcm, errno, "cannot open device '%s'", fn);
        return pcm;
    }

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_INFO, &info)) {
        oops(pcm, errno, "cannot get info");
        goto fail_close;
    }

    param_init(&params);
    param_set_mask(&params, SNDRV_PCM_HW_PARAM_FORMAT,
                   pcm_format_to_alsa(config->format));
    param_set_mask(&params, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                   SNDRV_PCM_SUBFORMAT_STD);
    param_set_min(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, config->period_size);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
                  pcm_format_to_bits(config->format));
    param_set_int(&params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
                  pcm_format_to_bits(config->format) * config->channels);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_CHANNELS,
                  config->channels);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_PERIODS, config->period_count);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_RATE, config->rate);
    param_set_flag(&params, config->flag);

    if (flags & PCM_NOIRQ) {
        if (!(flags & PCM_MMAP)) {
            oops(pcm, -EINVAL, "noirq only currently supported with mmap().");
            goto fail;
        }

        params.flags |= SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP;
        pcm->noirq_frames_per_msec = config->rate / 1000;
    }

    if (flags & PCM_MMAP)
        param_set_mask(&params, SNDRV_PCM_HW_PARAM_ACCESS,
                       SNDRV_PCM_ACCESS_MMAP_INTERLEAVED);
    else
        param_set_mask(&params, SNDRV_PCM_HW_PARAM_ACCESS,
                       SNDRV_PCM_ACCESS_RW_INTERLEAVED);

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_HW_PARAMS, &params)) {
        oops(pcm, errno, "cannot set hw params");
        goto fail_close;
    }

    /* get our refined hw_params */
    config->period_size = param_get_int(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
    config->period_count = param_get_int(&params, SNDRV_PCM_HW_PARAM_PERIODS);
    pcm->buffer_size = config->period_count * config->period_size;
	//ALOGD("-lfy-vicent-vicent------------get our refined hw_params-----------period_size = %d\n",pcm->config.period_size);
	//ALOGD("-lfy-vicent-vicent-------------get our refined hw_params----------period_count = %d\n",pcm->config.period_count);
    if (flags & PCM_MMAP) {
        pcm->mmap_buffer = mmap(NULL, pcm_frames_to_bytes(pcm, pcm->buffer_size),
                                PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, pcm->fd, 0);
        if (pcm->mmap_buffer == MAP_FAILED) {
            oops(pcm, -errno, "failed to mmap buffer %d bytes\n",
                 pcm_frames_to_bytes(pcm, pcm->buffer_size));
            goto fail_close;
        }
    }

    memset(&sparams, 0, sizeof(sparams));
    sparams.tstamp_mode = SNDRV_PCM_TSTAMP_ENABLE;
    sparams.period_step = 1;

    if (!config->start_threshold) {
        if (pcm->flags & PCM_IN)
            pcm->config.start_threshold = sparams.start_threshold = 1;
        else
            pcm->config.start_threshold = sparams.start_threshold =
                config->period_count * config->period_size / 2;
    } else
        sparams.start_threshold = config->start_threshold;

    /* pick a high stop threshold - todo: does this need further tuning */
    if (!config->stop_threshold) {
        if (pcm->flags & PCM_IN)
            pcm->config.stop_threshold = sparams.stop_threshold =
                config->period_count * config->period_size * 10;
        else
            pcm->config.stop_threshold = sparams.stop_threshold =
                config->period_count * config->period_size;
    }
    else
        sparams.stop_threshold = config->stop_threshold;

    if (!pcm->config.avail_min) {
        if (pcm->flags & PCM_MMAP)
            pcm->config.avail_min = sparams.avail_min = pcm->config.period_size;
        else
            pcm->config.avail_min = sparams.avail_min = 1;
    } else
        sparams.avail_min = config->avail_min;

    sparams.xfer_align = config->period_size / 2; /* needed for old kernels */
    sparams.silence_threshold = config->silence_threshold;
    sparams.silence_size = config->silence_size;
    pcm->boundary = sparams.boundary = pcm->buffer_size;

    while (pcm->boundary * 2 <= INT_MAX - pcm->buffer_size)
        pcm->boundary *= 2;

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sparams)) {
        oops(pcm, errno, "cannot set sw params");
        goto fail;
    }

    rc = pcm_hw_mmap_status(pcm);
    if (rc < 0) {
        oops(pcm, rc, "mmap status failed");
        goto fail;
    }

#ifdef SNDRV_PCM_IOCTL_TTSTAMP
    if (pcm->flags & PCM_MONOTONIC) {
        int arg = SNDRV_PCM_TSTAMP_TYPE_MONOTONIC;
        rc = ioctl(pcm->fd, SNDRV_PCM_IOCTL_TTSTAMP, &arg);
        if (rc < 0) {
            oops(pcm, rc, "cannot set timestamp type");
            goto fail;
        }
    }
#endif

    pcm->underruns = 0;
    return pcm;

fail:
    if (flags & PCM_MMAP)
        munmap(pcm->mmap_buffer, pcm_frames_to_bytes(pcm, pcm->buffer_size));
fail_close:
    close(pcm->fd);
    pcm->fd = -1;
    return pcm;
}

int pcm_is_ready(struct pcm *pcm)
{
    return pcm->fd >= 0;
}

int pcm_prepare(struct pcm *pcm)
{
    if (pcm->prepared)
        return 0;

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PREPARE) < 0)
        return oops(pcm, errno, "cannot prepare channel");

    pcm->prepared = 1;
    return 0;
}

int pcm_start(struct pcm *pcm)
{
    int prepare_error = pcm_prepare(pcm);
    if (prepare_error)
        return prepare_error;

    if (pcm->flags & PCM_MMAP)
	    pcm_sync_ptr(pcm, 0);

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START) < 0)
        return oops(pcm, errno, "cannot start channel");

    pcm->running = 1;
    return 0;
}

int pcm_stop(struct pcm *pcm)
{
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_DROP) < 0)
        return oops(pcm, errno, "cannot stop channel");

    pcm->prepared = 0;
    pcm->running = 0;
    return 0;
}

static inline int pcm_mmap_playback_avail(struct pcm *pcm)
{
    int avail;

    avail = pcm->mmap_status->hw_ptr + pcm->buffer_size - pcm->mmap_control->appl_ptr;

    if (avail < 0)
        avail += pcm->boundary;
    else if (avail > (int)pcm->boundary)
        avail -= pcm->boundary;

    return avail;
}

static inline int pcm_mmap_capture_avail(struct pcm *pcm)
{
    int avail = pcm->mmap_status->hw_ptr - pcm->mmap_control->appl_ptr;
    if (avail < 0)
        avail += pcm->boundary;
    return avail;
}

int pcm_mmap_avail(struct pcm *pcm)
{
    pcm_sync_ptr(pcm, SNDRV_PCM_SYNC_PTR_HWSYNC);
    if (pcm->flags & PCM_IN)
        return pcm_mmap_capture_avail(pcm);
    else
        return pcm_mmap_playback_avail(pcm);
}

static void pcm_mmap_appl_forward(struct pcm *pcm, int frames)
{
    unsigned int appl_ptr = pcm->mmap_control->appl_ptr;
    appl_ptr += frames;

    /* check for boundary wrap */
    if (appl_ptr > pcm->boundary)
         appl_ptr -= pcm->boundary;
    pcm->mmap_control->appl_ptr = appl_ptr;
}

int pcm_mmap_begin(struct pcm *pcm, void **areas, unsigned int *offset,
                   unsigned int *frames)
{
    unsigned int continuous, copy_frames, avail;

    /* return the mmap buffer */
    *areas = pcm->mmap_buffer;

    /* and the application offset in frames */
    *offset = pcm->mmap_control->appl_ptr % pcm->buffer_size;

    avail = pcm_mmap_avail(pcm);
    if (avail > pcm->buffer_size)
        avail = pcm->buffer_size;
    continuous = pcm->buffer_size - *offset;

    /* we can only copy frames if the are availabale and continuos */
    copy_frames = *frames;
    if (copy_frames > avail)
        copy_frames = avail;
    if (copy_frames > continuous)
        copy_frames = continuous;
    *frames = copy_frames;

    return 0;
}

int pcm_mmap_commit(struct pcm *pcm, unsigned int offset, unsigned int frames)
{
    /* update the application pointer in userspace and kernel */
    pcm_mmap_appl_forward(pcm, frames);
    pcm_sync_ptr(pcm, 0);

    return frames;
}

int pcm_avail_update(struct pcm *pcm)
{
    pcm_sync_ptr(pcm, 0);
    return pcm_mmap_avail(pcm);
}

int pcm_state(struct pcm *pcm)
{
    int err = pcm_sync_ptr(pcm, 0);
    if (err < 0)
        return err;

    return pcm->mmap_status->state;
}

int pcm_set_avail_min(struct pcm *pcm, int avail_min)
{
    if ((~pcm->flags) & (PCM_MMAP | PCM_NOIRQ))
        return -ENOSYS;

    pcm->config.avail_min = avail_min;
    return 0;
}

int pcm_wait(struct pcm *pcm, int timeout)
{
    struct pollfd pfd;
    int err;

    pfd.fd = pcm->fd;
    pfd.events = POLLOUT | POLLERR | POLLNVAL;

    do {
        /* let's wait for avail or timeout */
        err = poll(&pfd, 1, timeout);
        if (err < 0)
            return -errno;

        /* timeout ? */
        if (err == 0)
            return 0;

        /* have we been interrupted ? */
        if (errno == -EINTR)
            continue;

        /* check for any errors */
        if (pfd.revents & (POLLERR | POLLNVAL)) {
            switch (pcm_state(pcm)) {
            case PCM_STATE_XRUN:
                return -EPIPE;
            case PCM_STATE_SUSPENDED:
                return -ESTRPIPE;
            case PCM_STATE_DISCONNECTED:
                return -ENODEV;
            default:
                return -EIO;
            }
        }
    /* poll again if fd not ready for IO */
    } while (!(pfd.revents & (POLLIN | POLLOUT)));

    return 1;
}

int pcm_get_poll_fd(struct pcm *pcm)
{
    return pcm->fd;
}

int pcm_mmap_transfer(struct pcm *pcm, const void *buffer, unsigned int bytes)
{
    int err = 0, frames, avail;
    unsigned int offset = 0, count;

    if (bytes == 0)
        return 0;

    count = pcm_bytes_to_frames(pcm, bytes);

    while (count > 0) {

        /* get the available space for writing new frames */
        avail = pcm_avail_update(pcm);
        if (avail < 0) {
            fprintf(stderr, "cannot determine available mmap frames");
            return err;
        }

        /* start the audio if we reach the threshold */
	    if (!pcm->running &&
            (pcm->buffer_size - avail) >= pcm->config.start_threshold) {
            if (pcm_start(pcm) < 0) {
               fprintf(stderr, "start error: hw 0x%x app 0x%x avail 0x%x\n",
                    (unsigned int)pcm->mmap_status->hw_ptr,
                    (unsigned int)pcm->mmap_control->appl_ptr,
                    avail);
                return -errno;
            }
            pcm->wait_for_avail_min = 0;
        }

        /* sleep until we have space to write new frames */
        if (pcm->running) {
            /* enable waiting for avail_min threshold when less frames than we have to write
             * are available. */
            if (!pcm->wait_for_avail_min && (count > (unsigned int)avail))
                pcm->wait_for_avail_min = 1;

            if (pcm->wait_for_avail_min && (avail < pcm->config.avail_min)) {
                int time = -1;

                /* disable waiting for avail_min threshold to allow small amounts of data to be
                 * written without waiting as long as there is enough room in buffer. */
                pcm->wait_for_avail_min = 0;

                if (pcm->flags & PCM_NOIRQ)
                    time = (pcm->config.avail_min - avail) / pcm->noirq_frames_per_msec;

                err = pcm_wait(pcm, time);
                if (err < 0) {
                    pcm->prepared = 0;
                    pcm->running = 0;
                    oops(pcm, err, "wait error: hw 0x%x app 0x%x avail 0x%x\n",
                        (unsigned int)pcm->mmap_status->hw_ptr,
                        (unsigned int)pcm->mmap_control->appl_ptr,
                        avail);
                    pcm->mmap_control->appl_ptr = 0;
                    return err;
                }
                continue;
            }
        }

        frames = count;
        if (frames > avail)
            frames = avail;

        if (!frames)
            break;

        /* copy frames from buffer */
        frames = pcm_mmap_transfer_areas(pcm, (void *)buffer, offset, frames);
        if (frames < 0) {
            fprintf(stderr, "write error: hw 0x%x app 0x%x avail 0x%x\n",
                    (unsigned int)pcm->mmap_status->hw_ptr,
                    (unsigned int)pcm->mmap_control->appl_ptr,
                    avail);
            return frames;
        }

        offset += frames;
        count -= frames;
    }

    return 0;
}

int pcm_mmap_write(struct pcm *pcm, const void *data, unsigned int count)
{
    if ((~pcm->flags) & (PCM_OUT | PCM_MMAP))
        return -ENOSYS;

    return pcm_mmap_transfer(pcm, (void *)data, count);
}

int pcm_mmap_read(struct pcm *pcm, void *data, unsigned int count)
{
    if ((~pcm->flags) & (PCM_IN | PCM_MMAP))
        return -ENOSYS;

    return pcm_mmap_transfer(pcm, data, count);
}

int pcm_ioctl(struct pcm *pcm, int request, ...)
{
    va_list ap;
    void * arg;

    if (!pcm_is_ready(pcm))
        return -1;

    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);

    return ioctl(pcm->fd, request, arg);
}
