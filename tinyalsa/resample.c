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



int main(int argc, char **argv)
{
	int inlen =0;
	int outlen =0;
	int readlen =0;
	int ret = -1;
    FILE *fp=fopen("/data/output_r.pcm","rb+");
    FILE *fp2=fopen("/data/esample.pcm","wb+");
	unsigned char *sample=(unsigned char *)malloc(3072);
	unsigned char *Resample=(unsigned char *)malloc(3072*3);
    SpeexResamplerState *st  = speex_resampler_init(1, 48000, 16000, 0, NULL);
	speex_resampler_skip_zeros(st);
    do{

       readlen = fread(sample, 1, 3072, fp);

       if (readlen > 0)

       {

           inlen = readlen;

           outlen = 1024;

           ret = speex_resampler_process_int(st,0,sample, &inlen, Resample, &outlen);

           if (ret == RESAMPLER_ERR_SUCCESS)

           {
			  printf("speex_resampler_process_int----= %d\n",outlen);
              fwrite(Resample, 1, outlen, fp2);
           }

       }

      

    }while(readlen == 3072);
    speex_resampler_destroy(st);
	fclose(fp);
	fclose(fp2);
	return 0;
}


