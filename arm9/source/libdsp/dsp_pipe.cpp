#include <nds.h>
#include <stdio.h>
#include "dsp.h"
#include "dsp_mem.h"
#include "dsp_pipe.h"

static u32 sPipeMntrAddr = 0;
static dsp_pipe_port_callback_t sPipePortCallbackFuncs[DSP_PIPE_PORT_COUNT];
static void* sPipePortCallbackArgs[DSP_PIPE_PORT_COUNT];

void dsp_initPipe()
{
    sPipeMntrAddr = 0;
    for(int i = 0; i < DSP_PIPE_PORT_COUNT; i++)
    {
        sPipePortCallbackFuncs[i] = NULL;
        sPipePortCallbackArgs[i] = NULL;
    }
}

void dsp_pipeIrqCallback(void* arg)
{
    iprintf("dsp_pipeCallback!\n");
    while((dsp_getSemaphore() & 0x8000) || dsp_receiveDataReady(DSP_PIPE_CMD_REG))
    {
        dsp_clearSemaphore(0x8000);
        while(dsp_receiveDataReady(DSP_PIPE_CMD_REG))
        {
            if(!sPipeMntrAddr)
            {
                sPipeMntrAddr = DSP_MEM_ADDR_TO_CPU(dsp_receiveData(DSP_PIPE_CMD_REG));
                iprintf("sPipeMntrAddr = %x\n", sPipeMntrAddr);
            }
            else
            {
                u16 data = dsp_receiveData(DSP_PIPE_CMD_REG);
                iprintf("Received %x\n", data);
                int port = data >> 1;
                int dir = data & 1;
                if(sPipePortCallbackFuncs[port])
                    sPipePortCallbackFuncs[port](sPipePortCallbackArgs[port], port, dir);
                else
                {
                    
                }                
            }            
        }
    }
}

void dsp_setPipePortCallback(int port, dsp_pipe_port_callback_t func, void* arg)
{
    sPipePortCallbackFuncs[port] = func;
    sPipePortCallbackArgs[port] = arg;
}