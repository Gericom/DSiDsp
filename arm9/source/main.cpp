#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include "libdsp/CmnDspProcess.h"
#include "libdsp/CtrDspProcess.h"
#include "libdsp/dsp_fifo.h"
#include "libdsp/dsp_pipe.h"

// static CmnDspProcess* sAudioProc;

// static void testAudio()
// {
// 	sAudioProc = new CmnDspProcess();
// 	if(!sAudioProc->ExecuteCoff("/_dsi/dsp/audio.a", 0xFF, 0xFF))
// 	{
// 		iprintf("DSP Init Fail!\n");
// 		while(1);
// 	}
// 	iprintf("DSP Init OK!\n");

// 	//test playing audio
// 	void* audio = malloc(4 * 1024 * 1024);
// 	FILE* tmp = fopen("/audio.bin", "rb");
// 	iprintf("fopen done! %p\n", tmp);
// 	fread(audio, 1, 4 * 1024 * 1024, tmp);
// 	fclose(tmp);
	
// 	iprintf("Press A to start and B to stop\n");
// 	while(true)
// 	{
// 		swiWaitForVBlank();
// 		scanKeys();
// 		if (keysDown()&KEY_START) 
// 			break;
// 		if (keysDown() & KEY_A)
// 			sAudioProc->PlaySound(audio, 4 * 1024 * 1024, false);
// 		else if (keysDown() & KEY_B)
// 			sAudioProc->StopSound();
// 	}
// }

static int getAdtsFrameSize(const void* data)
{
	//thanks ida :p
	u32 v2 = __builtin_bswap32(*(const u32*)data);
	u32 v4 = __builtin_bswap32(((const u32*)data)[1]);
	if ( v2 >> 20 != 0xFFF )
		return 0;
	u32 v6 = (v2 >> 10) & 0xF;
	if ( (v2 >> 17) & 3 || (u16)v2 >> 14 != 1 || v6 < 3 || v6 >= 0xC || ((v2 >> 6) & 7) > 2 || (v4 >> 8) & 3 )
		return 0;
	else
		return (v4 >> 21) | ((v2 & 3) << 11);
}

static u8 sAudioFrameBuffer[0x600] ALIGN(32);

#define AUDIO_BLOCK_SIZE	(1024)
#define NR_WAVE_DATA_BUFFERS	(16) 

static s16 mWaveDataBufferL[AUDIO_BLOCK_SIZE * NR_WAVE_DATA_BUFFERS] ALIGN(32);
static s16 mWaveDataBufferR[AUDIO_BLOCK_SIZE * NR_WAVE_DATA_BUFFERS] ALIGN(32);

static volatile int nrAudioBlocksAvailable;

static void audioBlockHandler()
{
	int savedIrq = enterCriticalSection();
	{
		nrAudioBlocksAvailable--;
	}
	leaveCriticalSection(savedIrq);
	if(nrAudioBlocksAvailable < 0)
	{
		iprintf("Audio underflow!\n");
	}
}

int main()
{
	fatInitDefault();
	consoleDemoInit();  //setup the sub screen for printing

	iprintf("DSP Test\n");

	CtrDspProcess* testProc = new CtrDspProcess();
	if(!testProc->ExecuteDsp1("/_dsi/dsp/dspaudio.cdc"))
	{
		iprintf("DSP Init Fail!\n");
		while(1);
	}
	iprintf("DSP Init OK!\n");

	if(!testProc->AACDecInitialize(DSP_CODEC_AACDEC_FORMAT_ADTS))
	{
		iprintf("AACDecInitialize failed!\n");
		while(1);
	}

	FILE* tmp = fopen("/audio.aac", "rb");

	iprintf("Decoding now!\n");

	twr_mapWramCSlot(2, TWR_WRAM_C_SLOT_MASTER_ARM9, 2, true);

	soundEnable();

	bool hasStarted = FALSE;
	int audioWriteBlock = 0;
	nrAudioBlocksAvailable = 0;
	u32 adtsSize;

	DC_InvalidateRange(sAudioFrameBuffer, sizeof(sAudioFrameBuffer));
	fread(sAudioFrameBuffer, 1, 8, tmp);
	adtsSize = getAdtsFrameSize((u8*)sAudioFrameBuffer);
	fread((u8*)sAudioFrameBuffer + 8, 1, adtsSize - 8, tmp);

	while (*((vu16*)0x04000130) & KEY_START)
	{
		while (nrAudioBlocksAvailable < NR_WAVE_DATA_BUFFERS - 1)
		{
			memcpy((void*)(twr_getBlockAddress(TWR_WRAM_BLOCK_C) + 0x16A00), sAudioFrameBuffer, adtsSize);
			memset((void*)(twr_getBlockAddress(TWR_WRAM_BLOCK_C) + 0x16A00 + 0x600), 0, 0x1000);

			twr_mapWramCSlot(2, TWR_WRAM_C_SLOT_MASTER_DSP_DATA, 2, true);

			testProc->AACDecDecode((const void*)0xB500, adtsSize, (s16*)(0xB500 + 0x300), (s16*)(0xB500 + 0x300 + 0x400));
			
			u32 oldAdtsSize = adtsSize;

			DC_InvalidateRange(sAudioFrameBuffer, sizeof(sAudioFrameBuffer));
			fread(sAudioFrameBuffer, 1, 8, tmp);
			adtsSize = getAdtsFrameSize((u8*)sAudioFrameBuffer);
			fread((u8*)sAudioFrameBuffer + 8, 1, adtsSize - 8, tmp);

			testProc->WaitCodecResponse();

			int errorCode = testProc->AACDecGetErrorCode();
			if(errorCode != 0)
			{
				iprintf("Error (%d)!", errorCode);
				while(1);
			}

			int usedBytes = testProc->AACDecGetUsedBytes();
			if(usedBytes != oldAdtsSize)
			{
				iprintf("Number of bytes used (%d) not equal to frame size (%d)!", usedBytes, oldAdtsSize);
				while(1);
			}

			twr_mapWramCSlot(2, TWR_WRAM_C_SLOT_MASTER_ARM9, 2, true);

			memcpy(&mWaveDataBufferL[audioWriteBlock * AUDIO_BLOCK_SIZE], (void*)(twr_getBlockAddress(TWR_WRAM_BLOCK_C) + 0x16A00 + 0x600), 0x800);
			memcpy(&mWaveDataBufferR[audioWriteBlock * AUDIO_BLOCK_SIZE], (void*)(twr_getBlockAddress(TWR_WRAM_BLOCK_C) + 0x16A00 + 0x600 + 0x800), 0x800);
			DC_FlushRange(&mWaveDataBufferL[audioWriteBlock * AUDIO_BLOCK_SIZE], AUDIO_BLOCK_SIZE * 2);
			DC_FlushRange(&mWaveDataBufferR[audioWriteBlock * AUDIO_BLOCK_SIZE], AUDIO_BLOCK_SIZE * 2);
			int savedIrq = enterCriticalSection();
        	{
				nrAudioBlocksAvailable++;
			}
			leaveCriticalSection(savedIrq);
			audioWriteBlock++;
			if (audioWriteBlock == NR_WAVE_DATA_BUFFERS)
				audioWriteBlock = 0;

			if (!hasStarted && nrAudioBlocksAvailable == (NR_WAVE_DATA_BUFFERS / 4))
			{
				hasStarted = TRUE;
				int freq = testProc->AACDecGetSampleRate();
				//libnds is gay here
				timerStart(3, ClockDivider_1024, /*TIMER_FREQ(32000)*/-((0x1000000 / freq) << 1), audioBlockHandler);

				soundPlaySample(mWaveDataBufferL, SoundFormat_16Bit, sizeof(mWaveDataBufferL), freq, 127, 0, true, 0);
				soundPlaySample(mWaveDataBufferR, SoundFormat_16Bit, sizeof(mWaveDataBufferR), freq, 127, 127, true, 0);
			}
		}
		while (nrAudioBlocksAvailable >= NR_WAVE_DATA_BUFFERS - 1);
	}
	fclose(tmp);
	while(1);

	//testAudio();

	return 0;
}
