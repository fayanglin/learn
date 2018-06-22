
#define LOG_TAG "SharedBuffer"

#include <utils/Log.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>

#include <sys/types.h>
#include <pthread.h>
#include <media/IAudioPolicyService.h>
#include <media/AudioSystem.h>
#include <system/audio.h>

#include "stdio.h"
#include "ISharedBuffer.h"
#include "audio_queue.h"


int connected=0;

class SharedBufferService : public BnSharedBuffer
{
public:
	SharedBufferService()
	{
		sp<MemoryHeapBase> heap = new MemoryHeapBase(AUDIO_QUEUE_SIZE, 0, "SharedBuffer");
		if(heap != NULL){
			mMemory = new MemoryBase(heap, 0, AUDIO_QUEUE_SIZE);
			audio_queue_t* data = (audio_queue_t*)mMemory->pointer();
			queue_init(data);
		}
	}

	virtual ~SharedBufferService()
	{
		audio_queue_t* data = (audio_queue_t*)mMemory->pointer();
		queue_destroy(data);
		mMemory = NULL;
	}

public:
	static void instantiate()
	{
		defaultServiceManager()->addService(String16(SHARED_BUFFER), new SharedBufferService());
	}

	virtual sp<IMemory> getBuffer()
	{
		LOGD("shareBufferServer %d !\n",__LINE__ );
		return mMemory;
	}

	virtual void setStatus(int status)
	{
		LOGD("shareBufferServer LINE = %d,status = %d \n",__LINE__,status);
		connected = status;
	}

private:
	sp<MemoryBase> mMemory;
};


int main(int argc, char** argv)
{
	LOGD("shareBufferServer LINE = %d \n",__LINE__);
	SharedBufferService::instantiate();

	ProcessState::self()->startThreadPool();
	IPCThreadState::self()->joinThreadPool();

	return 0;
}

extern "C" int share_buffer_init()
{
	LOGD("--lfy is God--share_buffer_init LINE = %d \n",__LINE__);
	SharedBufferService::instantiate();

	ProcessState::self()->startThreadPool();
	IPCThreadState::self()->joinThreadPool();

	return 0;	
}




