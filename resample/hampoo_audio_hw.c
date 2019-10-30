#define LOG_TAG "hampoo_audio"
#include "hampoo_audio_hw.h"
#include <system/audio.h>
#include "sys/un.h"
#include "sys/socket.h"
#include <sys/prctl.h>
#include <signal.h>
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define LOCAL_SOCKET_NAME "/data/ktv_local_socket"
#define DATA_FILE "/sdcard/a.pcm"
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
#endif

#ifdef HAMPOO_WAKE_UP
#define QUEUE_BUFF_MULTIPLE 1000
#define PERIOD_SIZE 256
#define MONO_BUFF_SIZE 512
#define STEREO_BUFF_SIZE MONO_BUFF_SIZE*2

#define RESAMPLE_BUFF_SIZE 1024

volatile int fd_client = NULL;
volatile int fd_server = NULL;
pthread_t tid_share_buffer;
pthread_t tid_data_producer;
pthread_mutex_t mutex_socket_control;
bool gIsSocketServerRunning = false;
int fd = -1;

typedef void* REACORD_HANDLE;
typedef struct _CAEUserData{
	audio_queue_t *queue;
	char *queue_buff;
}CAEUserData;

typedef struct _RecordData{
	pthread_t tid_pcm_read;
	int runing;
}RecordData;


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
static CAEUserData resampleData;
static CAEUserData userData;

#if HAMPOO_WAKE_UP_AUDIO_WRITE_DEBUG
static FILE *file_audio_write = NULL;
#endif

#if HAMPOO_WAKE_UP_DEBUG
static FILE *file_in_read = NULL;
static FILE *file_audio_write = NULL;
static FILE *file_audio_callback = NULL;
#endif
#endif

/**
 * @brief start_input_stream 
 * must be called with input stream and hw device mutexes locked 
 *
 * @param in
 *
 * @returns 
 */
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
	momobuf= (char *)malloc(MONO_BUFF_SIZE);
    if (in_resample->frames_in == 0) {
		/* do
		 {
			 Len=queue_read(resampleData.queue, &momobuf,MONO_BUFF_SIZE);
			 if(Len == MONO_BUFF_SIZE){
				 break;
			 }
			 usleep(50 * 1000);
		 }while(i>0);
		 */
		Len = read(fd,momobuf,MONO_BUFF_SIZE);
		in_resample->read_status = Len;
        if (in_resample->read_status == 0 ||in_resample->read_status < 0 ) {
            printf("get_next_buffer() read error %d", in_resample->read_status);
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
	printf("====frames_rq==in read=%d\n",frames_rq);
	ret = resample_read_frames(buffer, frames_rq);
	if(ret == 0)
	{
		printf("====in_read==end=\n");
		return	ret;
	}
	if(ret < 0)
	{
		printf("====in_read==error=\n");
		return	ret;
	}

	return bytes;
}
static int hampoo_resampler_init(void)
{
	int ret;
	printf("====hampoo_resampler_init==in=\n");
	in_resample = (struct hampoo_resampler *)calloc(1, sizeof(struct hampoo_resampler));
	if (!in_resample)
		return -1;	
	in_resample->buffer = malloc(PERIOD_SIZE* 2* 4);
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

	printf("====hampoo_resampler_init==out=\n");
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
int capturing = 1;
void sigint_handler(int sig)
{
    capturing = 0;
}

int main(int argc, char **argv)
{
	char buf[2048];
	int fdw = -1;
	int ret=0;
	hampoo_resampler_init();
	fd = open(DATA_FILE,O_RDONLY);
	if(fd<0)
	{
		printf("====open==error=\n");
		return -1;
	}
	fdw = open("/sdcard/b.pcm",O_RDWR|O_CREAT,0777);
	if(fdw<0)
	{
		printf("====creat==error=\n");
		return -1;
	}
	while(capturing)
	{
		ret = resample_read(buf,1024);
		if(ret>0)
		{
			printf(" write = %d\n",ret);
			write(fdw,buf,ret);
		}
		
	}
	close(fd);
	close(fdw);
	hampoo_resampler_exit();
}