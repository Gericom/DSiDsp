#pragma once

#define REG_DSP_PDATA           (*(vu16*)0x4004300)
#define REG_DSP_PADR            (*(vu16*)0x4004304)

#define DSP_PCFG_RESET          1

#define DSP_PCFG_IE_REP_SHIFT   9

#define DSP_PCFG_IE_REP0        (1 << DSP_PCFG_IE_REP_SHIFT)
#define DSP_PCFG_IE_REP1        (1 << (DSP_PCFG_IE_REP_SHIFT + 1))
#define DSP_PCFG_IE_REP2        (1 << (DSP_PCFG_IE_REP_SHIFT + 2))

#define REG_DSP_PCFG            (*(vu16*)0x4004308)

#define DSP_PSTS_PERI_RESET     (1 << 2)

#define DSP_PSTS_REP_NEW_SHIFT  10

#define DSP_PSTS_REP0_NEW       (1 << DSP_PSTS_REP_NEW_SHIFT)
#define DSP_PSTS_REP1_NEW       (1 << (DSP_PSTS_REP_NEW_SHIFT + 1))
#define DSP_PSTS_REP2_NEW       (1 << (DSP_PSTS_REP_NEW_SHIFT + 2))

#define DSP_PSTS_CMD_UNREAD_SHIFT 13

#define DSP_PSTS_CMD0_UNREAD    (1 << DSP_PSTS_CMD_UNREAD_SHIFT)
#define DSP_PSTS_CMD1_UNREAD    (1 << (DSP_PSTS_CMD_UNREAD_SHIFT + 1))
#define DSP_PSTS_CMD2_UNREAD    (1 << (DSP_PSTS_CMD_UNREAD_SHIFT + 2))

#define REG_DSP_PSTS            (*(vu16*)0x400430C)

#define REG_DSP_PSEM            (*(vu16*)0x4004310)
#define REG_DSP_PMASK           (*(vu16*)0x4004314)
#define REG_DSP_PCLEAR          (*(vu16*)0x4004318)
#define REG_DSP_SEM             (*(vu16*)0x400431C)

#define REG_DSP_CMD0            (*(vu16*)0x4004320)
#define REG_DSP_REP0            (*(vu16*)0x4004324)

#define REG_DSP_CMD1            (*(vu16*)0x4004328)
#define REG_DSP_REP1            (*(vu16*)0x400432C)

#define REG_DSP_CMD2            (*(vu16*)0x4004330)
#define REG_DSP_REP2            (*(vu16*)0x4004334)

void dsp_setBlockReset(bool reset);
void dsp_setClockEnabled(bool enabled);
void dsp_resetInterface();
void dsp_setCoreResetOn();
void dsp_setCoreResetOff(u16 repIrqMask);
void dsp_powerOn();
void dsp_powerOff();

void dsp_sendData(int id, u16 data);
u16 dsp_receiveData(int id);
bool dsp_receiveDataReady(int id);

static inline void dsp_setSemaphore(u16 mask)
{
    REG_DSP_PSEM = mask;
}

static inline void dsp_setSemaphoreMask(u16 mask)
{
    REG_DSP_PMASK = mask;
}

static inline void dsp_clearSemaphore(u16 mask)
{
    REG_DSP_PCLEAR = mask;
}

static inline u16 dsp_getSemaphore()
{
    swiDelay(8);
    return REG_DSP_SEM;
}