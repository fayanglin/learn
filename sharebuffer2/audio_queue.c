/*
 *  Created on: 2016.11.17
 */
 #define LOG_TAG "audio_queue"
 
#include<stdlib.h>
#include<string.h>
#include "audio_queue.h"

/**
 * base 分配给队列的内存块首地址
 * capacity 队列数据区大小
 */
int queue_init(audio_queue_t* queue)
{
	//audio_queue_t* queue = (audio_queue_t *) malloc(AUDIO_QUEUE_SIZE);
	if(queue == NULL){
		LOGE("husanzai queue_init error \n");
		return -1;
	}
	LOGD("husanzai queue_init queue = [%d]\n",queue);
#if 1
	//跨进程的互斥锁
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&(queue->mutex), &attr);
	pthread_mutexattr_destroy(&attr);
#else	
	pthread_mutex_init(&(queue->mutex), NULL);
#endif
	queue->buffer_header = (unsigned char *)queue;
	queue->offset = sizeof(audio_queue_t);
	queue->capacity = AUDIO_QUEUE_SIZE - queue->offset;
	queue->write_ptr = 0;
	queue->read_ptr = 0;
	queue->bfull = 0;
	return 0;
}

void queue_destroy(audio_queue_t* queue)
{
	LOGD("husanzai queue_destroy \n");
	if (NULL != queue) {
		pthread_mutex_destroy(&(queue->mutex));
		//free(queue);
		//queue = NULL;
	}
}

static int queue_used(audio_queue_t* queue)
{
	//LOGD("husanzai queue_used \n");
	if(queue->bfull == 1){
		return queue->capacity;
	}
	return (queue->write_ptr - queue->read_ptr + queue->capacity) % queue->capacity;
}

static int queue_unused(audio_queue_t* queue)
{	
	//LOGD("husanzai queue_unused \n");
	return queue->capacity - queue_used(queue);
}
/*
static void my_memcpy(unsigned char* dest,unsigned char* src,int len)
{
	int i;
	if( (dest==NULL) || (src == NULL) ){
		return;
	}
	for(i=0;i<len;i++){
		dest[i] = src[i];
	}
}*/

int queue_clear(audio_queue_t* queue)
{
	if (queue == NULL) {
		return -1;
	}

	pthread_mutex_lock(&(queue->mutex));
	queue->write_ptr = 0;
	queue->read_ptr = 0;
	queue->bfull = 0;
	pthread_mutex_unlock(&(queue->mutex));
	LOGD("husanzai queue_clear ok \n");
	
	return 0;
}

int queue_write(audio_queue_t* queue, unsigned char *data, int dataLen)
{
	if (queue == NULL || data == NULL || dataLen <= 0) {
		return -1;
	}
	//int over_write = 0;
	//LOGD("husanzai queue_write start , queue = %d \n",queue);
	pthread_mutex_lock(&(queue->mutex));
	int unused = queue_unused(queue);
	//LOGD("husanzai queue_write ,unused[%d] , dataLen[%d] \n",unused,dataLen);
	if ( unused < dataLen) {
		pthread_mutex_unlock(&(queue->mutex));
		//LOGE("husanzai queue_write error,unused[%d] < dataLen[%d] \n",unused,dataLen);
		//over_write = 1;
		return -1;
	}
	
	// 计算数据区起始地址
	queue->buffer_header = (unsigned char *)queue;//多进程时各各进程空间地址映射的值不一样
	unsigned char* queueBase = queue->buffer_header + queue->offset;
	unsigned char* begin = &(queueBase[queue->write_ptr]);

	if (queue->write_ptr + dataLen <= queue->capacity) {
		memcpy(begin, data, dataLen);
	} else {
		// 分两段写入
		int dataLen1 = queue->capacity - queue->write_ptr;
		int dataLen2 = dataLen - dataLen1;

		memcpy(begin, data, dataLen1);
		memcpy(queueBase, data + dataLen1, dataLen2);
	}
	queue->write_ptr = (queue->write_ptr + dataLen) % queue->capacity;
	if(queue->write_ptr == queue->read_ptr){
		queue->bfull = 1;//已满
	}else{
		queue->bfull = 0;
	}
	//if(over_write){
	//	queue->read_ptr = queue->write_ptr;
	//}
	pthread_mutex_unlock(&(queue->mutex));
	//LOGD("husanzai queue_write end \n");
	
	return dataLen;
}

int queue_read(audio_queue_t* queue, unsigned char *data,int dataLen)
{
	// 计算数据区起始地址
	if (queue == NULL || data == NULL || dataLen <= 0) {
		return -1;
	}
	//LOGD("husanzai queue_read start \n");
	//LOGD("husanzai queue_read start queue = [%d] ,data = %d,dataLen = %d mutex = %d \n",queue,data,dataLen,&(queue->mutex));
	pthread_mutex_lock(&(queue->mutex));
	int used = queue_used(queue);

	/*if(used == 0){
		pthread_mutex_unlock(&(queue->mutex));
		LOGE("husanzai queue_read error,used == [%d] < dataLen[%d] \n",used,dataLen);
		//over_write = 1;
		return -1;
	}else if( used < dataLen){
		LOGD("husanzai queue_read not enough,used[%d] < dataLen[%d] \n",used,dataLen);
		//over_write = 1;
		dataLen = used;//ajust read dataLen
		//return -1;
	}*/

	if( used < dataLen){
		pthread_mutex_unlock(&(queue->mutex));
		//LOGW("husanzai queue_read error,used[%d] < dataLen[%d] \n",used,dataLen);
		//over_write = 1;
		return -1;
	}
	queue->buffer_header = (unsigned char *)queue;//多进程时各各进程空间地址映射的值不一样
	unsigned char* queueBase = queue->buffer_header + queue->offset;
	unsigned char* begin = &(queueBase[queue->read_ptr]);
	if (queue->read_ptr + dataLen <= queue->capacity ) {
		memcpy(data, begin, dataLen);
	} else {
		// 分两段读取
		int readLen1 = queue->capacity - queue->read_ptr;
		int readLen2 = dataLen - readLen1;

		memcpy(data, begin, readLen1);
		memcpy(&(data[readLen1]), queueBase, readLen2);
	}
	queue->bfull = 0;
	queue->read_ptr = (queue->read_ptr + dataLen) % queue->capacity;
	pthread_mutex_unlock(&(queue->mutex));
	//LOGD("husanzai queue_read end \n");
	return dataLen;
}


