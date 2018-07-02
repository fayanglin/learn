/* audio capture process */
#include <tinyalsa/asoundlib.h>
#include "sys/un.h"
#include "sys/socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <log/log.h>
#include <errno.h>       
#define LOCAL_SOCKET_NAME "/data/ktv_local_socket"
#define LOG_TAG "socket_clien"
int capturing = 1;
int sock_t = 0;


void sigint_handler(int sig)
{
    capturing = 0;
}
int  socket_clien(void) 
{
    int err;
    long result;
    socklen_t len;

    struct sockaddr_un addr;
    addr.sun_family = AF_LOCAL;
   // addr.sun_path[0] = '\0';
    strcpy(addr.sun_path, LOCAL_SOCKET_NAME);
   // len = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(&addr.sun_path[1]);
   len =sizeof(addr);
    ALOGD("before creating socket");
    sock_t = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (sock_t < 0) {
        ALOGD("%s: cannot open socket: %s (%d)\n", __FUNCTION__, strerror(err), err);
        sock_t = 0;
        return -1;
    }
    ALOGD("before connect to java local socket");
    if (connect(sock_t, (struct sockaddr *) &addr, len) < 0) {
        ALOGD("%s: cannot connect to socket: %s (%d)\n", __FUNCTION__, strerror(err), err);
        close(sock_t);
        sock_t = 0;
        return -1;
    }
    ALOGD("connect to java local socket success");
    return 0;

}
int client_write(int fd,void *buffer,int length)
{
	int bytes_left;
	int written_bytes;
	char *ptr;

	ptr=buffer;
	bytes_left=length;
	while(bytes_left>0)
	{
	        
		written_bytes=write(fd,ptr,bytes_left);
		if(written_bytes<=0)
		{       
		  if(errno==EINTR)
		  {
		  	 ALOGD("socket_write EINTR\n");
   	 		fprintf(stderr,"socket_write EINTR\n"); 
		    written_bytes=0;
		  }
		  else   
		  {          
		     return -1;
		  	 ALOGD("socket_write failed\n");
   	 		fprintf(stderr,"socket_write failed\n"); 
		  }
		}
		bytes_left-=written_bytes;
		ptr+=written_bytes;     
	}
	return 0;
}
int main(int argc, char **argv)
{
	unsigned int card;
	unsigned int device;
    struct pcm_config config;
    struct pcm *pcm;
    signed short *buffer;
    unsigned int size;
    int ret = -1;

	// 录音参数初始化
    config.channels = 1;
    config.rate = 48000;
    config.period_size = 256;
    config.period_count = 6;
    config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 768;
    config.stop_threshold = 1536;
    config.silence_threshold = 0;
    config.avail_min = 0;
    config.flag = 0;
    card = 3;
    device = 0;
   

   //打开设备
    while(1)
	{
		pcm = pcm_open(card, device, PCM_IN, &config);
		if (!pcm || !pcm_is_ready(pcm)) {
			fprintf(stderr, "Unable to open PCM device (%s)\n",
					pcm_get_error(pcm));
			ALOGE(" Unable to open PCM device : %d\n",card );
		   sleep(2);
		}
		else
		{
			//分配数据空间
			size = pcm_get_buffer_size(pcm);//pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
			ALOGD("Capturing pcm_get_buffer_size: %d \n",size );
			fprintf(stderr,"Capturing pcm_get_buffer_size: %d \n",size );
			size =512;
			buffer = malloc(size);
			if (!buffer) {
				fprintf(stderr, "Unable to allocate %d bytes\n", size);
				free(buffer);
				ALOGE("Unable to allocate %d bytes\n", size);
				pcm_close(pcm);
				return 0;
			}
			signal(SIGTERM, sigint_handler);
			ALOGD("Capturing sample: %u ch, %u hz, %u bit\n", config.channels, config.rate, pcm_format_to_bits(config.format));
			fprintf(stderr,"Capturing sample: %u ch, %u hz, %u bit\n",config.channels, config.rate,pcm_format_to_bits(config.format));
			
				//创建socket连接
			ret =  socket_clien();
			if(ret != 0)
			{
			 ALOGD("socket_clien ERROR\n");
			 fprintf(stderr,"socket_clien ERROR\n");   	
			}
			
			//开始录音
			while (capturing) 
			{
					if(pcm != NULL)   
					{
							ret = pcm_read(pcm, buffer, size);
							if(ret != 0)  // 1、中途拔出mic
							{
								
								pcm_close(pcm);
								pcm =NULL;
								pcm = pcm_open(card, device, PCM_IN, &config);
								if (!pcm || !pcm_is_ready(pcm)) {
									fprintf(stderr, "--starting recoard but  mic is got out-- (%s)\n",
											pcm_get_error(pcm));
									ALOGE("--starting recoard but  mic is got out--" );
								}
							}
							else
							{
								// socket 向服务器发送数据
								if (0 != sock_t) 
								{
								  // write(sock_t, buffer, size);
								  ret = client_write(sock_t, buffer, size);
								  if(ret !=0)
								  {
											ALOGD("---Recoarding----server is  ERROR\n");
											fprintf(stderr,"---Recoarding----server is  ERROR\n"); 
											ret =  socket_clien();
											if(ret != 0)
											{
											 ALOGD("--- Recoarding----socket_clien  write  reconnect ERROR\n");
											 fprintf(stderr,"--Recoarding----socket_clien   write reconnect ERROR\n");   	
											}  		          	
								  }
								}
								else
								{
									ret =  socket_clien();
									if(ret != 0)
									{
									 ALOGD("---Recoarding----socket_clien  reconnect ERROR\n");
									 fprintf(stderr,"--Recoarding----socket_clien reconnect ERROR\n");   	
									}        	
								}
							}

					}
					else  // 2、开机不接mic的情况
					{
							pcm = pcm_open(card, device, PCM_IN, &config);
							if (!pcm || !pcm_is_ready(pcm))
								 {
								fprintf(stderr, "--booting no mic-- (%s)\n",pcm_get_error(pcm));
								ALOGE("--booting no mic--" );
								usleep(10*1000);
							}	
									 
					}
			}
			
			if (0 != sock_t) {
				fprintf(stderr, "lose local socket\n");
				ALOGE("close local socket ");
				close(sock_t);
			}
			if (NULL != pcm) {
				fprintf(stderr, "close pcm\n");
				ALOGE("close pcm ");
				pcm_close(pcm);
			}
			if (!buffer) {
				fprintf(stderr, "free buffer\n");
				ALOGE("free buffer");
				free(buffer);
			 }
			 fprintf(stderr, "client outr\n");
			 return 0;
		}
		
	}
    return 0;
}

