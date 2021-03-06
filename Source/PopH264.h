#pragma once

/*
	gr: this header should be C
	it probably isn't strict C at the moment, but no classes, namespaces etc.
*/

#include <stdint.h>


#if !defined(__export)

#if defined(_MSC_VER) && !defined(TARGET_PS4)
#define __export			extern "C" __declspec(dllexport)
#else
#define __export			extern "C"
#endif

#endif



//	function pointer type for new frame callback
typedef void PopH264_Callback(void* Meta);



__export int32_t			PopH264_GetVersion();

//	cleanup all resources (on some platforms this may need to be called to prevent a crash on exit/dll unload)
__export void				PopH264_Shutdown();

//	get embedded h264 data. Returns size of the data (which may be larger than the buffer)
//	if Buffer is null, this will just return the size
//	returns -1 on error
//	returns 0 if Name doesnt exist
__export int32_t			PopH264_GetTestData(const char* Name,uint8_t* Buffer,int32_t BufferSize);




#define POPH264_DECODER_KEY_DECODERNAME		"Decoder"



//	All options are optional
//	returns an instance id. 0 on error.
__export int32_t			PopH264_CreateDecoder(const char* OptionsJson, char* ErrorBuffer, int32_t ErrorBufferLength);
__export void				PopH264_DestroyDecoder(int32_t Instance);

//	deprecated for PopH264_DestroyDecoder
__export void				PopH264_DestroyInstance(int32_t Instance);

//	get a list of valid POPH264_DECODER_KEY_DECODERNAME values supported on this system
//	in json format. More meta & debug may get added here
__export void				PopH264_EnumDecoders(char* DecodersJsonBuffer,int32_t DecodersJsonBufferLength);


//	deprecate meta values for json
__export void				PopH264_GetMeta(int32_t Instance, int32_t* MetaValues, int32_t MetaValuesCount);
__export void				PopH264_PeekFrame(int32_t Instance,char* JsonBuffer,int32_t JsonBufferSize);

//	push NALU packets (even fragmented)
//	This decodes the packet, but may not have an immediate output, and may have more than one frame output.
//	This is synchronous, any scheduling should be on the caller.
//	todo; fix framenumber to mix with fragmented data; for now, if framenumber is important, defragment nalu packets at high level
__export int32_t			PopH264_PushData(int32_t Instance,uint8_t* Data,int32_t DataSize,int32_t FrameNumber);

//	returns frame number. -1 on error
__export int32_t			PopH264_PopFrame(int32_t Instance,uint8_t* Plane0,int32_t Plane0Size,uint8_t* Plane1,int32_t Plane1Size,uint8_t* Plane2,int32_t Plane2Size);

//	register a callback function when a new packet is ready. This is expected to exist until released
__export void				PopH264_DecoderAddOnNewFrameCallback(int32_t Instance,PopH264_Callback* Callback, void* Meta);


//	generic
#define POPH264_ENCODER_KEY_ENCODERNAME		"Encoder"
#define POPH264_ENCODER_KEY_PROFILELEVEL	"ProfileLevel"

//	avf & nvidia
#define POPH264_ENCODER_KEY_AVERAGEKBPS		"AverageKbps"
#define POPH264_ENCODER_KEY_MAXKBPS			"MaxKbps"
#define POPH264_ENCODER_KEY_REALTIME		"Realtime"	//	on nvidia, this is "max performance" setting

//	x264 & nvidia
#define POPH264_ENCODER_KEY_VERBOSEDEBUG	"VerboseDebug"

//	avf
#define POPH264_ENCODER_KEY_MAXFRAMEBUFFERS	"MaxFrameBuffers"
#define POPH264_ENCODER_KEY_MAXSLICEBYTES	"MaxSliceBytes"
#define POPH264_ENCODER_KEY_MAXIMISEPOWEREFFICIENCY	"MaximisePowerEfficiency"
#define POPH264_ENCODER_KEY_KEYFRAMEFREQUENCY	"KeyFrameFrequency"	//	keyframe every N frames

//	x264
#define POPH264_ENCODER_KEY_QUALITY				"Quality"
#define POPH264_ENCODER_KEY_ENCODERTHREADS		"EncoderThreads"
#define POPH264_ENCODER_KEY_LOOKAHEADTHREADS	"LookaheadThreads"
#define POPH264_ENCODER_KEY_BSLICEDTHREADS		"BSlicedThreads"
#define POPH264_ENCODER_KEY_DETERMINISTIC		"Deterministic"
#define POPH264_ENCODER_KEY_CPUOPTIMISATIONS	"CpuOptimisations"

//	nvidia
#define POPH264_ENCODER_KEY_CONSTANTBITRATE		"ConstantBitRate"	//	else variable
#define POPH264_ENCODER_KEY_SLICELEVELENCODING	"SliceLevelEncoding"


//	All options are optional
//	.Encoder = "avf"|"x264"
//	.Quality = [0..9]				x264
//	.AverageKbps = int				avf kiloBYTES
//	.MaxKbps = int					avf kiloBYTES
//	.Realtime = true				avf: kVTCompressionPropertyKey_RealTime
//	.MaxFrameBuffers = undefined	avf: kVTCompressionPropertyKey_MaxFrameDelayCount
//	.MaxSliceBytes = number			avf: kVTCompressionPropertyKey_MaxH264SliceBytes
//	.MaximisePowerEfficiency = true	avf: kVTCompressionPropertyKey_MaximizePowerEfficiency
//	.ProfileLevel = 30(int)			Baseline only at the moment. 30=3.0, 41=4.1 etc this also matches the number in SPS. Default will try and pick correct for resolution or 3.0
__export int32_t			PopH264_CreateEncoder(const char* OptionsJson,char* ErrorBuffer,int32_t ErrorBufferSize);
__export void				PopH264_DestroyEncoder(int32_t Instance);

//	This encodes the frame, but may not have an immediate output, and may have more than one packet output.
//	This is synchronous, any scheduling should be on the caller.
//	meta should contain
//		.Width
//		.Height
//		.LumaSize (bytes)
//		.ChromaUSize (bytes if not null)
//		.ChromaVSize (bytes if not null)
//		.Keyframe (bool, defaults to false)
//	All fields will be copied to output meta (including those unused above)
//	Error will be a string mentioning any missing fields (if provided)
//	Currently .Keyframe is COPIED to output, not updated to the actual state (should check NALU for this)
__export void				PopH264_EncoderPushFrame(int32_t Instance,const char* MetaJson,const uint8_t* LumaData,const uint8_t* ChromaUData,const uint8_t* ChromaVData,char* ErrorBuffer,int32_t ErrorBufferSize);

//	Use this to try and cause a flush of any remaining frames
__export void				PopH264_EncoderEndOfStream(int32_t Instance);

//	copies & removes next packet and returns buffer size written (may be greater than input buffer size, in which case data will be lost)
//	if DataBuffer is null, the size is returned, but data NOT discarded. This can be used to Peek for buffer size. (Use Peek function to also get meta)
//	returns 0 if there is no Data to pop.
//	returns -1 on error
__export int32_t			PopH264_EncoderPopData(int32_t Instance,uint8_t* DataBuffer,int32_t DataBufferSize);

//	get meta for next packet as json. If MetaJsonSize isn't big enough, it writes as much as possible.
//	members if pending data
//		.DataSize			byte-size of packet (missing if no packet)
//		.Meta				all meta passed in to PopH264_EncoderPushFrame
//		.EncodeDurationMs	time it took to encode
//		.DelayDurationMs	time spent in queue before encoding (lag)
//	members regardless of pending data
//		.OutputQueueCount	number of packets waiting to be popped
__export void				PopH264_EncoderPeekData(int32_t Instance,char* MetaJsonBuffer,int32_t MetaJsonBufferSize);

//	register a callback function when a new packet is ready. This is expected to exist until released
__export void				PopH264_EncoderAddOnNewPacketCallback(int32_t Instance,PopH264_Callback* Callback, void* Meta);



