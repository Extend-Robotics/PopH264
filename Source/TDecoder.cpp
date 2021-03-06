#include "TDecoder.h"
#include "SoyH264.h"


PopH264::TDecoder::TDecoder(std::function<void(const SoyPixelsImpl&,FrameNumber_t)> OnDecodedFrame) :
	mOnDecodedFrame	( OnDecodedFrame )
{
}


void PopH264::TDecoder::OnDecodedFrame(const SoyPixelsImpl& Pixels,FrameNumber_t FrameNumber)
{
	//	todo: check against input frame numbers to make sure decoder is outputting same as it inputs
	mOnDecodedFrame( Pixels, FrameNumber );
}

void PopH264::TDecoder::OnDecodedEndOfStream()
{
	SoyPixels Null;
	mOnDecodedFrame(Null,0);
}

void PopH264::TDecoder::PushEndOfStream()
{
	//	send an explicit end of stream nalu
	//	todo: overload this for implementation specific flushes
	auto Eos = H264::EncodeNaluByte(H264NaluContent::EndOfStream,H264NaluPriority::Important);
	uint8_t EndOfStreamNalu[]{ 0,0,0,1,Eos };
	auto DataArray = GetRemoteArray( EndOfStreamNalu );
	
	//	mark pending data as finished
	mPendingDataFinished = true;
	Decode( GetArrayBridge(DataArray), 0 );
}

void PopH264::TDecoder::Decode(ArrayBridge<uint8_t>&& PacketData,FrameNumber_t FrameNumber)
{
	//	gr: maybe we should split when we PopNalu to move this work away from caller thread
	auto PushNalu = [&](const ArrayBridge<uint8_t>&& Nalu)
	{
		std::lock_guard<std::mutex> Lock(mPendingDataLock);
		std::shared_ptr<TInputNaluPacket> pPacket( new TInputNaluPacket() );
		pPacket->mData.Copy(Nalu);
		pPacket->mFrameNumber = FrameNumber;
		mPendingDatas.PushBack(pPacket);
	};
	H264::SplitNalu( PacketData, PushNalu );
	
	while ( true )
	{
		//	keep decoding until no more data to process
		if ( !DecodeNextPacket() )
			break;
	}
}


void PopH264::TDecoder::UnpopNalu(ArrayBridge<uint8_t>&& Nalu,FrameNumber_t FrameNumber)
{
	//	put-back data at the start of the queue
	//auto PushNalu = [&](const ArrayBridge<uint8_t>&& Nalu)
	{
		std::lock_guard<std::mutex> Lock(mPendingDataLock);
		std::shared_ptr<TInputNaluPacket> pPacket( new TInputNaluPacket() );
		pPacket->mData.Copy(Nalu);
		pPacket->mFrameNumber = FrameNumber;
		mPendingDatas.PushBack(pPacket);
	};
}


bool PopH264::TDecoder::PopNalu(ArrayBridge<uint8_t>&& Buffer,FrameNumber_t& FrameNumber)
{
	//	gr:could returnthis now and avoid the copy & alloc at caller
	std::shared_ptr<TInputNaluPacket> NextPacket;
	{
		std::lock_guard<std::mutex> Lock( mPendingDataLock );
		if ( mPendingDatas.IsEmpty() )
		{
			//	expecting more data to come
			if ( !mPendingDataFinished )
				return false;
		
			//	no more data ever
			return false;
		}
		NextPacket = mPendingDatas.PopAt(0);
	}
	Buffer.Copy( NextPacket->mData );
	FrameNumber = NextPacket->mFrameNumber;
	return true;
}




