#include "trdefs.h"
#include <cstdio>

thread_local int g_trLastErr;

static uint8_t g_LogEnable[trLOGLEVELCOUNT] = { 1,1,1,1,1,1,1,1 }; 

extern "C" void trLogVarList(const trLOGLEVEL loglevel, const trChar* pFmt, va_list args)
{
    vprintf(pFmt, args);
}

static trLOGHANDLER g_logHandler = trLogVarList;

extern "C" int trLogLevelEnable(trLOGLEVEL level, int enable)
{
    const int oldstate = g_LogEnable[level];
    g_LogEnable[level] = (uint8_t) enable;
    return oldstate;
}

extern "C" trLOGHANDLER trLogSetHandler(trLOGHANDLER logHandler)
{
    trLOGHANDLER old_handler = g_logHandler;
    g_logHandler = logHandler;
    return old_handler;
}

extern "C" void trLog(const trLOGLEVEL level, const trChar* pFmt, ...)
{
    if (g_LogEnable[level])
    {
        va_list args;
        va_start(args, pFmt);
        trLogVarList(level, pFmt, args);
        va_end(args);
    }
}

extern "C" unsigned trGetTopBit(uint32_t val)
{
    unsigned pos = 0;

    if (val >= 0x10000UL) pos = 16, val >>= 16;
    if (val & 0xFF00U)    pos |= 8, val >>= 8;
    if (val & 0xF0U)      pos |= 4, val >>= 4;
    if (val & 0xCU)       pos |= 2, val >>= 2;
    if (val & 0x2)        pos |= 1;

    return pos;
}
