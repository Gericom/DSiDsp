#pragma once

#define DSP_PIPE_CMD_REG        2

#define DSP_PIPE_DIR_COUNT      2

#define DSP_PIPE_DIR_IN         0
#define DSP_PIPE_DIR_OUT        1

#define DSP_PIPE_PORT_COUNT     8

#define DSP_PIPE_PORT_CONSOLE   0
#define DSP_PIPE_PORT_DMA       1
#define DSP_PIPE_PORT_AUDIO     2
#define DSP_PIPE_PORT_BINARY    3
#define DSP_PIPE_PORT_TEMP      4

typedef void (*dsp_pipe_port_callback_t)(void* arg, int port, int dir);

struct dsp_pipe_t
{
    u16 addr;
    u16 len;
    u16 readPtr;
    u16 writePtr;
    u16 flags;
};

struct dsp_pipe_mon_t
{
    dsp_pipe_t pipes[DSP_PIPE_PORT_COUNT][DSP_PIPE_DIR_COUNT];
};


void dsp_initPipe();
void dsp_pipeIrqCallback(void* arg);
void dsp_setPipePortCallback(int port, dsp_pipe_port_callback_t func, void* arg);