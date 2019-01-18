#include "PopH264.h"
#include "BroadwayDecoder.h"
#include "SoyLib/src/SoyPixels.h"

class TNoParams;
class TDecoderInstance;
using TInstanceObject = TDecoderInstance;
using TInstanceParams = TNoParams;

#include "InstanceManager.inc"

class TFrame
{
public:
	std::shared_ptr<SoyPixelsImpl>	mPixels;
	uint32_t						mTimeMs;
};
	
class TDecoderInstance
{
public:
	void									PushData(const uint8_t* Data,size_t DataSize);
	void									PopFrame(int32_t& FrameTimeMs,ArrayBridge<uint8_t>&& Plane0,ArrayBridge<uint8_t>&& Plane1,ArrayBridge<uint8_t>&& Plane2);
	bool									PopFrame(TFrame& Frame);
	void									PushFrame(const SoyPixelsImpl& Frame);
	const SoyPixelsMeta&					GetMeta() const	{	return mMeta;	}
	
private:
	uint32_t								mFrameCounter = 0;
	Broadway::TDecoder						mDecoder;
	std::mutex								mFramesLock;
	Array<TFrame>							mFrames;
	SoyPixelsMeta							mMeta;
};



#if defined(TARGET_WINDOWS)
BOOL APIENTRY DllMain(HMODULE /* hModule */, DWORD ul_reason_for_call, LPVOID /* lpReserved */)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}
#endif



void TDecoderInstance::PushData(const uint8_t* Data,size_t DataSize)
{
	auto DataArray = GetRemoteArray( Data, DataSize );
	auto PushFrame = [this](const SoyPixelsImpl& Pixels)
	{
		this->PushFrame( Pixels );
	};
	mDecoder.Decode( GetArrayBridge(DataArray), PushFrame );
}

void TDecoderInstance::PopFrame(int32_t& FrameTimeMs,ArrayBridge<uint8_t>&& Plane0,ArrayBridge<uint8_t>&& Plane1,ArrayBridge<uint8_t>&& Plane2)
{
	TFrame Frame;
	if ( !PopFrame( Frame ) )
	{
		FrameTimeMs = -1;
		return;
	}
	
	//	if we don't set the correct time the c# thinks we have a bad frame!
	FrameTimeMs = Frame.mTimeMs;
	
	//	emulating TPixelBuffer interface
	BufferArray<SoyPixelsImpl*, 10> Textures;
	Textures.PushBack( Frame.mPixels.get() );

	BufferArray<std::shared_ptr<SoyPixelsImpl>, 10> Planes;
	
	//	get all the planes
	for ( auto t = 0; t < Textures.GetSize(); t++ )
	{
		auto& Texture = *Textures[t];
		Texture.SplitPlanes(GetArrayBridge(Planes));
	}
	
	ArrayBridge<uint8_t>* PlanePixels[] = { &Plane0, &Plane1, &Plane2 };
	for ( auto p = 0; p < Planes.GetSize() && p<3; p++ )
	{
		auto& Plane = *Planes[p];
		auto& PlaneDstPixels = *PlanePixels[p];
		auto& PlaneSrcPixels = Plane.GetPixelsArray();
		
		auto MaxSize = std::min(PlaneDstPixels.GetDataSize(), PlaneSrcPixels.GetDataSize());
		//	copy as much as possible
		auto PlaneSrcPixelsMin = GetRemoteArray(PlaneSrcPixels.GetArray(), MaxSize);
		PlaneDstPixels.Copy(PlaneSrcPixelsMin);
	}

}

bool TDecoderInstance::PopFrame(TFrame& Frame)
{
	std::lock_guard<std::mutex> Lock(mFramesLock);
	if ( mFrames.IsEmpty() )
		return false;
	
	Frame = mFrames[0];
	mFrames.RemoveBlock(0,1);
	return true;
}

void TDecoderInstance::PushFrame(const SoyPixelsImpl& Frame)
{
	TFrame NewFrame;
	NewFrame.mTimeMs = mFrameCounter++;
	NewFrame.mPixels.reset( new SoyPixels( Frame ) );

	std::lock_guard<std::mutex> Lock(mFramesLock);
	mFrames.PushBack(NewFrame);
	mMeta = Frame.GetMeta();
}

	

__export int32_t PopFrame(int32_t Instance,uint8_t* Plane0,int32_t Plane0Size,uint8_t* Plane1,int32_t Plane1Size,uint8_t* Plane2,int32_t Plane2Size)
{
	auto Function = [&]()
	{
		auto& Decoder = InstanceManager::GetInstance(Instance);
		//	Decoder.PopFrame
		auto Plane0Array = GetRemoteArray(Plane0, Plane0Size);
		auto Plane1Array = GetRemoteArray(Plane1, Plane1Size);
		auto Plane2Array = GetRemoteArray(Plane2, Plane2Size);
		int32_t FrameTimeMs = -1;
		Decoder.PopFrame( FrameTimeMs, GetArrayBridge(Plane0Array), GetArrayBridge(Plane1Array), GetArrayBridge(Plane2Array));
		return FrameTimeMs;
	};
	return SafeCall(Function, __func__, -99 );
}

__export int32_t PushData(int32_t Instance,uint8_t* Data,int32_t DataSize)
{
	auto Function = [&]()
	{
		auto& Decoder = InstanceManager::GetInstance(Instance);
		Decoder.PushData( Data, DataSize );
		return 0;
	};
	return SafeCall(Function, __func__, -1 );
}


__export void GetMeta(int32_t Instance, int32_t* pMetaValues, int32_t MetaValuesCount)
{
	auto Function = [&]()
	{
		auto& Device = InstanceManager::GetInstance(Instance);
		
		auto& Meta = Device.GetMeta();
		
		size_t MetaValuesCounter = 0;
		auto MetaValues = GetRemoteArray(pMetaValues, MetaValuesCount, MetaValuesCounter);
		
		BufferArray<SoyPixelsMeta, 3> PlaneMetas;
		Meta.GetPlanes(GetArrayBridge(PlaneMetas));
		MetaValues.PushBack(PlaneMetas.GetSize());
		
		for ( auto p=0;	p<PlaneMetas.GetSize();	p++ )
		{
			auto& PlaneMeta = PlaneMetas[p];
			MetaValues.PushBack(PlaneMeta.GetWidth());
			MetaValues.PushBack(PlaneMeta.GetHeight());
			MetaValues.PushBack(PlaneMeta.GetChannels());
			MetaValues.PushBack(PlaneMeta.GetFormat());
			MetaValues.PushBack(PlaneMeta.GetDataSize());
		}
		
		return 0;
	};
	auto x = SafeCall(Function, __func__, 0 );
}