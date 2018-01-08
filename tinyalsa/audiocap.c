/* audio capture process */
#include <tinyalsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <log/log.h>

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};
int capturing = 1;

unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            enum pcm_format format, unsigned int period_size,
                            unsigned int period_count);

void sigint_handler(int sig)
{
    capturing = 0;
}

int main(int argc, char **argv)
{
	FILE *file;
    struct wav_header header;
    unsigned int card = 3;
    unsigned int device = 0;
    unsigned int channels = 2;
    unsigned int rate = 44100;
    unsigned int bits = 16;
    unsigned int frames;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;
    enum pcm_format format;
	fprintf(stderr,"stone:start service audiocap");
 
	
	file = fopen(argv[1], "wb");
    if (!file) {
        fprintf(stderr, "Unable to create file '%s'\n", argv[1]);
        return 1;
    }
   /* parse command line arguments */
    argv += 2;
    while (*argv) {
        if (strcmp(*argv, "-d") == 0) {
            argv++;
            if (*argv)
                device = atoi(*argv);
        } else if (strcmp(*argv, "-c") == 0) {
            argv++;
            if (*argv)
                channels = atoi(*argv);
        } else if (strcmp(*argv, "-r") == 0) {
            argv++;
            if (*argv)
                rate = atoi(*argv);
        } else if (strcmp(*argv, "-b") == 0) {
            argv++;
            if (*argv)
                bits = atoi(*argv);
        } else if (strcmp(*argv, "-D") == 0) {
            argv++;
            if (*argv)
                card = atoi(*argv);
        } else if (strcmp(*argv, "-p") == 0) {
            argv++;
            if (*argv)
                period_size = atoi(*argv);
        } else if (strcmp(*argv, "-n") == 0) {
            argv++;
            if (*argv)
                period_count = atoi(*argv);
        }
        if (*argv)
            argv++;
    }
	
    header.riff_id = ID_RIFF;
    header.riff_sz = 0;
    header.riff_fmt = ID_WAVE;
    header.fmt_id = ID_FMT;
    header.fmt_sz = 16;
    header.audio_format = FORMAT_PCM;
    header.num_channels = channels;
    header.sample_rate = rate;
	
    switch (bits) {
    case 32:
        format = PCM_FORMAT_S32_LE;
        break;
    case 24:
        format = PCM_FORMAT_S24_LE;
        break;
    case 16:
        format = PCM_FORMAT_S16_LE;
        break;
    default:
        fprintf(stderr, "%d bits is not supported.\n", bits);
        return 1;
    }
	sleep(5);
	header.bits_per_sample = pcm_format_to_bits(format);
    header.byte_rate = (header.bits_per_sample / 8) * channels * rate;
    header.block_align = channels * (header.bits_per_sample / 8);
    header.data_id = ID_DATA;
	    /* leave enough room for header */
    fseek(file, sizeof(struct wav_header), SEEK_SET);
   
    /* install signal handler and begin capturing */
    signal(SIGTERM, sigint_handler);
    frames = capture_sample(file,card, device, channels,
                            rate, format,
                            period_size, period_count);
    ALOGD("Captured %d frames\n", frames);
	printf("Captured %d frames\n", frames);
	   /* write header now all information is known */
    header.data_sz = frames * header.block_align;
    header.riff_sz = header.data_sz + sizeof(header) - 8;
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(struct wav_header), 1, file);

    fclose(file);
    return 0;
}

unsigned int capture_sample(FILE *file,unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            enum pcm_format format, unsigned int period_size,
                            unsigned int period_count)
{
    struct pcm_config config;
    struct pcm *pcm;
    signed short *buffer;
    unsigned int size;
    unsigned int bytes_read = 0;

    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    config.format = format;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;
    config.avail_min = 0;
    config.flag = 0;
    pcm = pcm_open(card, device, PCM_IN, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "Unable to open PCM device (%s)\n",
                pcm_get_error(pcm));
        return 0;
    }

   // size = pcm_get_buffer_size(pcm);//pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
	size =  pcm_frames_to_bytes(pcm, config.period_size);
	ALOGD("Capturing pcm_get_buffer_size: %d \n",size );
    fprintf(stderr,"Capturing pcm_get_buffer_size: %d \n",size );
    buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "Unable to allocate %d bytes\n", size);
        free(buffer);
        pcm_close(pcm);
        return 0;
    }

    ALOGD("Capturing sample: %u ch, %u hz, %u bit\n", channels, rate,
           pcm_format_to_bits(format));
    fprintf(stderr,"Capturing sample: %u ch, %u hz, %u bit\n", channels, rate,
           pcm_format_to_bits(format));

    while (capturing && !pcm_read(pcm, buffer, size)) {
        if (fwrite(buffer, 1, size, file) != size) {
            fprintf(stderr,"Error capturing sample\n");
            break;
        }
        bytes_read += size;
    }

    free(buffer);
    pcm_close(pcm);
    return pcm_bytes_to_frames(pcm, bytes_read);
}

