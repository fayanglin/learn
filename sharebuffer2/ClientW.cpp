#define LOG_TAG "SharedWrite"

#include <utils/Log.h>
#include <binder/MemoryBase.h>
#include <binder/IServiceManager.h>

#include <unistd.h>

#include "ISharedBuffer.h"
#include "audio_queue.h"

#if 0
int main()
{
	sp<IBinder> binder = defaultServiceManager()->getService(String16(SHARED_BUFFER));
	if(binder == NULL)
	{
		LOGE("Failed to get service: %s \n", SHARED_BUFFER);
		return -1;
	}

	sp<ISharedBuffer> service = interface_cast<ISharedBuffer>(binder);
	if(service == NULL)
	{
		return -2;
	}

	sp<IMemory> buffer = service->getBuffer();
	if(buffer == NULL)
	{
		return -3;
	}

	audio_queue_t *queue = (audio_queue_t *)buffer->pointer();

	if(queue == NULL)
	{
		LOGE("get the queue failed \n");
		return -4;
	}

	unsigned char buf[20];
	for(int i=0; i<20 ;i++){

		buf[i]=i;
	}

	LOGD("ClientW start \n");
	while(1){
		LOGD("queue_write = %d \n",queue_write(queue,buf,20) );
		sleep(3);
	}

	return 0;
}
#else

static audio_queue_t *queue = NULL;
static int flag_init = 0;
extern "C" int share_write_init()
{
	LOGD("--lfy is God--share_write_init start \n");
	if(flag_init){
		return flag_init;
	}	
	sp<IBinder> binder = defaultServiceManager()->getService(String16(SHARED_BUFFER));
	if(binder == NULL)
	{
		LOGE("Failed to get service: %s \n", SHARED_BUFFER);
		return -1;
	}

	sp<ISharedBuffer> service = interface_cast<ISharedBuffer>(binder);
	if(service == NULL)
	{
		return -2;
	}

	sp<IMemory> buffer = service->getBuffer();
	if(buffer == NULL)
	{
		return -3;
	}

	queue = (audio_queue_t *)buffer->pointer();

	if(queue == NULL)
	{
		LOGE("get the queue failed \n");
		return -4;
	}
	flag_init = 1;
	LOGD("--lfy is God--share_write_init end \n");
	return flag_init;
}

extern "C" int share_write_data(unsigned char *data, int dataLen)
{
	return queue_write(queue,data,dataLen);
}

extern "C" int share_write_clear()
{
	return queue_clear(queue);
}
#endif