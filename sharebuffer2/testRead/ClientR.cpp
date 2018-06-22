
#define LOG_TAG "SharedBufferClient"

#include <utils/Log.h>
#include <binder/MemoryBase.h>
#include <binder/IServiceManager.h>

#include "../ISharedBuffer.h"
#include "../audio_queue.h"

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
		LOGE("get the queue failed \n!");
		return -4;
	}

	unsigned char buf[10];
	int num=0;
	
	LOGD("ClientR start \n");
	while(1){

		num = queue_read(queue, buf,10);
		
		LOGD("queue_read num = %d \n",num);
		for(int i=0;i <num;i++){
			LOGD("0x%x ",buf[i]);
		}

		LOGD("\n");
		
		sleep(1);
	}
	
	return 0;
}





















