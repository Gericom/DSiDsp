#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include "libdsp/CmnDspProcess.h"
#include "libdsp/dsp_fifo.h"
#include "libdsp/dsp_pipe.h"

static CmnDspProcess* sAudioProc;

int main()
{
	fatInitDefault();
	consoleDemoInit();  //setup the sub screen for printing

	iprintf("DSP Test\n");
	sAudioProc = new CmnDspProcess();
	if(!sAudioProc->Execute("/_dsi/dsp/audio.a", 0xFF, 0xFF))
	{
		iprintf("DSP Init Fail!\n");
		while(1);
	}
	iprintf("DSP Init OK!\n");

	//test playing audio
	void* audio = malloc(4 * 1024 * 1024);
	FILE* tmp = fopen("audio.bin", "rb");
	iprintf("fopen done! %p\n", tmp);
	fread(audio, 1, 4 * 1024 * 1024, tmp);
	fclose(tmp);
	
	iprintf("Press A to start and B to stop\n");
	while(true)
	{
		swiWaitForVBlank();
		scanKeys();
		if (keysDown()&KEY_START) 
			break;
		if (keysDown() & KEY_A)
			sAudioProc->PlaySound(audio, 4 * 1024 * 1024, false);
		else if (keysDown() & KEY_B)
			sAudioProc->StopSound();
	}

	return 0;
}
