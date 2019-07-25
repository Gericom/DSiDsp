#include <nds.h>
#include <stdio.h>
#include <filesystem.h>
#include <fat.h>
#include "libdsp/DspProcess.h"

static DspProcess* sAudioProc;

int main()
{
	fatInitDefault();
	consoleDemoInit();  //setup the sub screen for printing

	iprintf("DSP Test\n");
	sAudioProc = new DspProcess();
	if(!sAudioProc->Execute("audio.a", 0xFF, 0xFF))
	{
		iprintf("DSP Init Fail!\n");
		while(1);
	}
	iprintf("DSP Init OK!\n");

	while(true)
	{
		swiWaitForVBlank();
		scanKeys();
		if (keysDown()&KEY_START) 
			break;
	}

	return 0;
}
