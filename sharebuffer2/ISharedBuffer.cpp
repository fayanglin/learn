
#define LOG_TAG "SharedBuffer"

#include <utils/Log.h>
#include <binder/MemoryBase.h>

#include "ISharedBuffer.h"
#include "audio_queue.h"

using namespace android;

enum
{
	GET_BUFFER = IBinder::FIRST_CALL_TRANSACTION,
	SET_STATUS =IBinder::FIRST_CALL_TRANSACTION +1
};

class BpSharedBuffer: public BpInterface<ISharedBuffer>
{
public:
	BpSharedBuffer(const sp<IBinder>& impl)
		: BpInterface<ISharedBuffer>(impl)
	{
 
	}

public:
	sp<IMemory> getBuffer()
	{
		LOGD("ISharedBuffer LINE = %d \n",__LINE__);
		Parcel data;
		data.writeInterfaceToken(ISharedBuffer::getInterfaceDescriptor());

		Parcel reply;
		remote()->transact(GET_BUFFER, data, &reply);

		sp<IMemory> buffer = interface_cast<IMemory>(reply.readStrongBinder());

		return buffer;
	}
  
	void setStatus(int status){
		LOGD("ISharedBuffer LINE = %d, status = %d \n",__LINE__, status);
		Parcel data;
		data.writeInt32(status);
		Parcel reply;
		remote()->transact(SET_STATUS, data, &reply);
	}
};

IMPLEMENT_META_INTERFACE(SharedBuffer, SHARED_BUFFER);

status_t BnSharedBuffer::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
	switch(code)
	{
	case GET_BUFFER:
		{
			LOGD("ISharedBuffer LINE = %d \n",__LINE__ );
			//CHECK_INTERFACE(ISharedBuffer, data, reply);  //FIX ME
			sp<IMemory> buffer = getBuffer();
			if(buffer != NULL)
			{
				//reply->writeStrongBinder(buffer->asBinder());
				reply->writeStrongBinder(IInterface::asBinder(buffer));
			}
			return NO_ERROR;
		}
	case SET_STATUS:
		{
			LOGD("ISharedBuffer LINE = %d \n",__LINE__ );
			//CHECK_INTERFACE(ISharedBuffer, data, reply);  //FIX ME!
			setStatus(data.readInt32());
			return NO_ERROR;
		}
	default:
		{
			LOGD("ISharedBuffer LINE = %d, status = %d \n",__LINE__,data.readInt32() );
			return BBinder::onTransact(code, data, reply, flags);
		}
	}
}
