#include <nds.h>
#include <string.h>
#include "dsp.h"
#include "dsp_pipe.h"
#include "dsp_coff.h"
#include "DspProcess.h"

static DspProcess* sDspProcess = NULL;

void DspProcess::DspIrqHandler()
{
    if(sDspProcess)
        sDspProcess->HandleDspIrq();
}

void DspProcess::HandleDspIrq()
{
    while(true)
    {
        u32 sources = (REG_DSP_SEM | (((REG_DSP_PSTS >> DSP_PCFG_IE_REP_SHIFT) & 7) << 16)) & _callbackSources;
        if(!sources)
            break;
        while(sources)
        {
            int idx = __builtin_ctz(sources);
            sources &= ~_callbackGroups[idx];
            _callbackFuncs[idx](_callbackArgs[idx]);
        }
    }
}

DspProcess::DspProcess(bool forceDspSyncLoad)
    : _slotB(0), _slotC(0), _codeSegs(0), _dataSegs(0), _flags(forceDspSyncLoad ? DSP_PROCESS_FLAG_SYNC_LOAD : 0),
    _callbackSources(0)
{
    for(int i = 0; i < TWR_WRAM_BC_SLOT_COUNT; i++)
    {
        _codeSlots[i] = 0xFF;
        _dataSlots[i] = 0xFF;
    }
    for(int i = 0; i < DSP_PROCESS_CALLBACK_COUNT; i++)
    {
        _callbackFuncs[i] = NULL;
        _callbackArgs[i] = NULL;
        _callbackGroups[i] = 0;
    }
}

bool DspProcess::SetMemoryMapping(bool isCode, u32 addr, u32 len, bool toDsp)
{
    addr = DSP_MEM_ADDR_TO_CPU(addr);
    len = DSP_MEM_ADDR_TO_CPU(len < 1 ? 1 : len);
    int segBits = isCode ? _codeSegs : _dataSegs;
    int start = addr >> TWR_WRAM_BC_SLOT_SHIFT;
    int end = (addr + len - 1) >> TWR_WRAM_BC_SLOT_SHIFT;
    for(int i = start; i <= end; i++)
    {
        if(!(segBits & (1 << i)))
            continue;
        int slot = isCode ? _codeSlots[i] : _dataSlots[i];
        if(isCode)
            twr_mapWramBSlot(slot, toDsp ? TWR_WRAM_B_SLOT_MASTER_DSP_CODE : TWR_WRAM_B_SLOT_MASTER_ARM9, toDsp ? i : slot, true);
        else
            twr_mapWramCSlot(slot, toDsp ? TWR_WRAM_C_SLOT_MASTER_DSP_DATA : TWR_WRAM_C_SLOT_MASTER_ARM9, toDsp ? i : slot, true);
    }
    return true;
}

bool DspProcess::EnumerateSections(dsp_process_sec_callback_t callback)
{
    if(!callback)
        return false;
    dsp_coff_header_t coffHeader;
    fseek(_coffFile, 0, SEEK_SET);
    if(fread(&coffHeader, 1, sizeof(dsp_coff_header_t), _coffFile) != sizeof(dsp_coff_header_t))
        return false;
    int sectabOffset = sizeof(dsp_coff_header_t) + coffHeader.optHdrLength;
    dsp_coff_section_t section;
    for(int i = 0; i < coffHeader.nrSections; i++)
    {
        fseek(_coffFile, sectabOffset + i * sizeof(dsp_coff_section_t), SEEK_SET);
        if(fread(&section, 1, sizeof(dsp_coff_section_t), _coffFile) != sizeof(dsp_coff_section_t))
            return false;
        if((section.flags & DSP_COFF_SECT_FLAG_BLK_HDR) || section.size == 0)
            continue;
        if(!callback(this, &coffHeader, &section))
            return false;
    }
    return true;
}

bool DspProcess::LoadSection(const dsp_coff_header_t* header, const dsp_coff_section_t* section)
{
    const char* name = (const char*)section->name.name;
    char fullName[128];
    if(section->name.zeroIfLong == 0)
    {
        fseek(_coffFile, header->symTabPtr + 0x12 * header->nrSyms + section->name.longNameOffset, SEEK_SET);
        fread(fullName, 1, sizeof(fullName), _coffFile);
        name = fullName;
    }

    struct
    {
        bool isCode;
        bool noLoad;
        int address;
    } placements[4];

    int nrPlacements = 0;

    if(!strcmp(name, "SDK_USING_OS@d0"))
        _flags |= DSP_PROCESS_FLAG_SYNC_LOAD;

    if(section->flags & DSP_COFF_SECT_FLAG_MAP_IN_CODE_MEM)
    {
        bool noLoad = section->flags & DSP_COFF_SECT_FLAG_NOLOAD_CODE_MEM;
        if(strstr(name, "@c0"))
        {
            placements[nrPlacements].isCode = true;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->codeAddr);
            nrPlacements++;
        }
        if(strstr(name, "@c1"))
        {
            placements[nrPlacements].isCode = true;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->codeAddr) + TWR_WRAM_BC_SLOT_SIZE * 4;
            nrPlacements++;
        }
    }

    if(section->flags & DSP_COFF_SECT_FLAG_MAP_IN_DATA_MEM)
    {
        bool noLoad = section->flags & DSP_COFF_SECT_FLAG_NOLOAD_DATA_MEM;
        if(strstr(name, "@d0"))
        {
            placements[nrPlacements].isCode = false;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->dataAddr);
            nrPlacements++;
        }
        if(strstr(name, "@d1"))
        {
            placements[nrPlacements].isCode = false;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->dataAddr) + TWR_WRAM_BC_SLOT_SIZE * 4;
            nrPlacements++;
        }
    }

    for(int i = 0; i < nrPlacements; i++)
    {
        bool isCode = placements[i].isCode;
        bool noLoad = placements[i].noLoad;
        if(!noLoad)
            fseek(_coffFile, section->sectionPtr, SEEK_SET);
        int dst = placements[i].address;
        int left = section->size;
        while(left > 0)
        {
            int blockEnd = (dst + TWR_WRAM_BC_SLOT_SIZE) & ~(TWR_WRAM_BC_SLOT_SIZE - 1);
            int blockLeft = blockEnd - dst;
            int len = left < blockLeft ? left : blockLeft;
            int seg = dst >> TWR_WRAM_BC_SLOT_SHIFT;
            int slot = isCode ? _codeSlots[seg] : _dataSlots[seg];
            if(slot == 0xFF)
            {
                u16* slots = isCode ? &_slotB : &_slotC;
                int* segs = isCode ? &_codeSegs : &_dataSegs;
                u8* slotMap = isCode ? _codeSlots : _dataSlots;
                slot = __builtin_ctz(*slots);
                if(slot >= TWR_WRAM_BC_SLOT_COUNT)
                    return false;
                slotMap[seg] = slot;
                *slots &= ~(1 << slot);
                *segs |= 1 << seg;
                if(isCode)
                    twr_mapWramBSlot(slot, TWR_WRAM_B_SLOT_MASTER_ARM9, slot, true);
                else
                    twr_mapWramCSlot(slot, TWR_WRAM_C_SLOT_MASTER_ARM9, slot, true);
                memset(DspToArm9Address(isCode, DSP_MEM_ADDR_TO_DSP(seg << TWR_WRAM_BC_SLOT_SHIFT)), 0, TWR_WRAM_BC_SLOT_SIZE);
            }
            if(!noLoad && fread(DspToArm9Address(isCode, DSP_MEM_ADDR_TO_DSP(dst)), 1, len, _coffFile) != len)
                return false;
            left -= len;
            dst += len;
        }
    }
    return true;
}

bool DspProcess::LoadProcess(const char* path, u16 slotB, u16 slotC)
{    
    _slotB = slotB;
    _slotC = slotC;
    _coffFile = fopen(path, "rb");
    if(!_coffFile)
        return false;

    bool ok = EnumerateSections(DspProcess::LoadSection);

    fclose(_coffFile);
    _coffFile = NULL;

    if(!ok)
        return false;

    SetMemoryMapping(true, 0, DSP_MEM_ADDR_TO_DSP(TWR_WRAM_BC_SLOT_SIZE * TWR_WRAM_BC_SLOT_COUNT), true);
    SetMemoryMapping(false, 0, DSP_MEM_ADDR_TO_DSP(TWR_WRAM_BC_SLOT_SIZE * TWR_WRAM_BC_SLOT_COUNT), true);
    return true;
}

bool DspProcess::Execute(const char* path, u16 slotB, u16 slotC)
{
    if(sDspProcess)
        return false;
    
    if(!LoadProcess(path, slotB, slotC))
        return false;

    int irq = enterCriticalSection();
	{
        sDspProcess = this;
        dsp_initPipe();
        irqSet(IRQ_WIFI, DspProcess::DspIrqHandler);
        SetCallback(DSP_PROCESS_CALLBACK_SEMAPHORE(15) | DSP_PROCESS_CALLBACK_REPLY(DSP_PIPE_CMD_REG), dsp_pipeIrqCallback, NULL);
        irqEnable(IRQ_WIFI);
		dsp_powerOn();
		dsp_setCoreResetOff(_callbackSources >> 16);
		dsp_setSemaphoreMask(~(_callbackSources & 0xFFFF));
        SetupCallbacks();
		//needed for some modules
		if(_flags & DSP_PROCESS_FLAG_SYNC_LOAD)
			for(int i = 0; i < 3; i++)
				while(dsp_receiveData(i) != 1);
        DspProcess::DspIrqHandler();
	}
	leaveCriticalSection(irq);
    
    return true;
}

void DspProcess::SetCallback(u32 sources, dsp_process_irq_callback_t func, void* arg)
{
    int irq = enterCriticalSection();
	{
        for(int i = 0; i < DSP_PROCESS_CALLBACK_COUNT; i++)
        {
            if(!(sources & (1 << i)))
                continue;
            _callbackFuncs[i] = func;
            _callbackArgs[i] = arg;
            _callbackGroups[i] = sources;
        }
        if(func)
        {
            REG_DSP_PCFG |= ((sources >> 16) & 7) << DSP_PCFG_IE_REP_SHIFT;
            REG_DSP_PMASK &= ~(sources & 0xFFFF);
            _callbackSources |= sources;
        }
        else
        {
            REG_DSP_PCFG &= ~(((sources >> 16) & 7) << DSP_PCFG_IE_REP_SHIFT);
            REG_DSP_PMASK |= sources & 0xFFFF;
            _callbackSources &= ~sources;
        }        
	}
	leaveCriticalSection(irq);
}