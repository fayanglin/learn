
#ifndef _AUDIO_QUEUE_H_
#define _AUDIO_QUEUE_H_

#include <pthread.h>
#include <cutils/log.h>

#define LOGD(...) {ALOGD(__VA_ARGS__);/*ALOGD("husanzai  LINE = %d",__LINE__);*/}
#define LOGI(...) {ALOGI(__VA_ARGS__);/*ALOGD("husanzai  LINE = %d",__LINE__);*/}
#define LOGW(...) {ALOGW(__VA_ARGS__);/*ALOGD("husanzai  LINE = %d",__LINE__);*/}
#define LOGE(...) {ALOGE(__VA_ARGS__);/*ALOGE("husanzai  LINE = %d",__LINE__);*/}

//#define LOGD printf
//#define LOGE printf


#define AUDIO_QUEUE_SIZE (sizeof(audio_queue_t) + 50*1024)

//队列数据结构：|***控制块***|***数据区***|
typedef struct audio_queue_t {
	pthread_mutex_t mutex;			// 互斥锁
	int capacity;					// 队列容量
	int write_ptr;					// 写指针,指向当前要写的位置
	int read_ptr;					// 读指针,指向当前要读的位置
	int bfull;						// 当 write_ptr == read_ptr 时,为真代表已经满了,为假代表为空
	int offset;						// 控制块的长度即 sizeof(audio_queue_t)
	unsigned char *buffer_header;	// 指向队列buff的头部
} audio_queue_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 将一块内存初始化为队列数据结构
 */
int queue_init(audio_queue_t* queue);

void queue_destroy(audio_queue_t* queue);

int queue_write(audio_queue_t* queue, unsigned char *data, int dataLen);

int queue_read(audio_queue_t* queue, unsigned char *data,int dataLen);

int queue_clear(audio_queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif /* _AUDIO_QUEUE_H_ */
