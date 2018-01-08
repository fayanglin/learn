/* audio capture process */
#include <tinyalsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <log/log.h>
#include <errno.h>
#include <cutils/log.h>
#include <system/audio.h>
#include <audio_utils/resampler.h>
#include <speex/speex_resampler.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>

void get_oneChannel_left_from_doubleChannel(unsigned char * pOneChannelBuf,unsigned char * pDoubleChannelBuf, int nLen, int nPerSampleBytesPerChannle)
{
	int i =0;
	int nOneChannelLen = nLen / 2;
	
	for (i = 0 ; i < nOneChannelLen/2; i++ )
	{
		memcpy((uint16_t*)pOneChannelBuf + i, ((uint32_t *)(pDoubleChannelBuf)) + i, nPerSampleBytesPerChannle);
	}


}
int simplest_pcm16le_split(void){
    FILE *fp=fopen("/data/alexa.pcm","rb+");
    FILE *fp2=fopen("/data/output_r.pcm","wb+");
	int i =0;
	int j =0;
    unsigned char *sample=(unsigned char *)malloc(1024);
	unsigned char *Rsample=(unsigned char *)malloc(512);
	int len = 512;
    while(!feof(fp)){
        fread(sample,1,1024,fp);
        //L

		for (i = 0 ; i <256; i++ )
		{
			memcpy((uint16_t*)Rsample + i, ((uint32_t *)(sample)) + i, 2);
		}

        //R
		fwrite(Rsample, 1, 512, fp2);
    }
    free(sample);
	free(Rsample);
    fclose(fp);
    fclose(fp2);
    return 0;
}
int main(int argc, char **argv)
{
	simplest_pcm16le_split();
	return 0;
}


