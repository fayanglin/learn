
#define LOG_TAG "SharedRead"

#include <utils/Log.h>
#include <binder/MemoryBase.h>
#include <binder/IServiceManager.h>
#include <jni.h>

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

#else

static audio_queue_t *queue = NULL;
static int flag_init = 0;
sp<IBinder> binder;
sp<ISharedBuffer> service;
sp<IMemory> buffer;

extern "C" int share_read_init()
{
	LOGD("--lfy is God--share_read_init start \n");
	if(flag_init){
		return flag_init;
	}
	binder = defaultServiceManager()->getService(String16(SHARED_BUFFER));
	if(binder == NULL)
	{
		LOGE("Failed to get service: %s \n", SHARED_BUFFER);
		return -1;
	}

	service = interface_cast<ISharedBuffer>(binder);
	if(service == NULL)
	{
		return -2;
	}

	buffer = service->getBuffer();
	if(buffer == NULL)
	{
		return -3;
	}

	queue = (audio_queue_t *)buffer->pointer();
	LOGD("--lfy is God--share_read_init queue = %d \n",queue);
	if(queue == NULL)
	{
		LOGE("get the queue failed \n");
		return -4;
	}
	flag_init = 1;
	LOGD("share_read_init end \n");
	return flag_init;
}

extern "C" int share_read_data(unsigned char *data, int dataLen)
{
	return queue_read(queue, data,dataLen);
}

extern "C" int share_read_clear()
{
	return queue_clear(queue);
}

extern "C" JNIEXPORT jint JNICALL Java_com_iflytek_halcae_JniHalCae_shareReadInit(JNIEnv *env,
		jobject obj) {

	LOGD("--lfy is God--Java_com_iflytek_halcae_Jni_share_read_init \n");
	return share_read_init();
}

extern "C" JNIEXPORT jint JNICALL Java_com_iflytek_halcae_JniHalCae_shareReadData(JNIEnv *env,
		jobject obj , jbyteArray data,jint dataLen) {
	
	//LOGD("Java_com_iflytek_halcae_Jni_share_read_data \n");

	unsigned char* pMsg = (unsigned char*) env->GetByteArrayElements(data, NULL);
	int cnt = 3;
	int ret = 0;
	do{
		cnt--;
		ret = share_read_data(pMsg,dataLen);
		if(ret != -1){
			break;
		}
		usleep(100 * 1000);
	}while(cnt > 0);
	
	env->ReleaseByteArrayElements(data, (signed char*) pMsg, 0);
	
	return ret;
}

extern "C" JNIEXPORT jint JNICALL Java_com_iflytek_halcae_JniHalCae_shareReadClear(JNIEnv *env,
		jobject obj) {

	LOGD("--lfy is God--Java_com_iflytek_halcae_Jni_share_read_clear \n");
	return share_read_clear();
}


#endif



