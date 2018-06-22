
#ifndef ISHAREDBUFFER_H_  
#define ISHAREDBUFFER_H_  
  
#include <utils/RefBase.h>  
#include <binder/IInterface.h>  
#include <binder/Parcel.h>  
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
  
#define SHARED_BUFFER "HidAudioSharedBuffer"

using namespace android;  
  
class ISharedBuffer: public IInterface  
{
public:
	DECLARE_META_INTERFACE(SharedBuffer);  
	virtual sp<IMemory> getBuffer() = 0;  
	virtual void setStatus(int status)=0;
};

class BnSharedBuffer: public BnInterface<ISharedBuffer>  
{
public:  
	virtual status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0);  
};  
  
#endif 



