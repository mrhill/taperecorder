#pragma once

#include <cstdint>
#include <cassert>
#include <cstdarg>

typedef int trERR;
enum
{
    trEOK = 0,
    trEEOF,
    trEIO,
    trENOMEM,
    trENOTFOUND,
    trEFILEFORMAT,
    trEBADPARAM,
    trENOTSUP,
    trELAST = ~0
};

extern thread_local int g_trLastErr;
#define trErrSet(err) g_trLastErr=err
#define trErrGet() g_trLastErr

// abstract strings, so they can be switched to wchar
#define trT(s) s
typedef char trChar;

typedef enum
{
    trFatal = 0,/**< Log level: Fatal error message. To be instantly displayed to the user. */
    trErr,      /**< Log level: Error message. To be instantly displayed to the user. */
    trWrn,      /**< Log level: Warning message, To be instantly displayed to the user. */
    trInf,      /**< Log level: Informational message. To be instantly displayed to the user. */
    trStatus,   /**< Log level: Status message. To be indirectly displayed in a status facility, usually a very short message */
    trVerbose,  /**< Log level: Verbose message. To be displayed to the user with possible delay, usually this log level will be disabled. */
    trDebug,    /**< Log level: Debug messages. To be displayed to the user with possible delay, intended for the development process. */
    trUser,     /**< Log level: User defined. A user defined log level. */
    trLOGLEVELCOUNT /**< Number of defined log levels. */

} trLOGLEVEL;


#ifdef  __cplusplus
extern "C" {
#endif

int trLogLevelEnable(trLOGLEVEL level, int enable);

typedef void (*trLOGHANDLER)(trLOGLEVEL level, const trChar* pFmt, va_list);
trLOGHANDLER trLogSetHandler(trLOGHANDLER loghandler);

void trLog(const trLOGLEVEL loglevel, const trChar* pFmt, ...);

unsigned trGetTopBit(uint32_t val);

#ifdef  __cplusplus
}
#endif

class trSample;
class trDataAnalyzer;

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X86)

#define trLD16LE(adr) ((uint32_t)*(const uint16_t*)(adr))
#define trLD24LE(adr) ((uint32_t)*((uint16_t*)(adr))|((uint32_t)*(((uint8_t*)(adr))+2)<<16))
#define trLD32LE(adr) (*(const uint32_t*)(adr))
#define trLD64LE(adr) (*(const uint64_t*)(adr))
#define trST16LE(adr,w) *(uint16_t*)(adr)=(uint16_t)(w)
#define trST24LE(adr,w) *(uint16_t*)(adr)=(uint16_t)(w),*((uint8_t*)(adr)+2)=(uint8_t)((w)>>16)
#define trST32LE(adr,w) *(uint32_t*)(adr)=(uint32_t)(w)
#define trST64LE(adr,w) *(uint64_t*)(adr)=(uint64_t)(w)
#define trLDS16LE(adr) ((int32_t)*((int16_t*)(adr)))
#define trLDS24LE(adr) (int32_t)((uint32_t)*(const uint16_t*)(adr)|((int32_t)*(((int8_t*)(adr))+2)<<16))

#else

/** Load little endian uint16_t (16 bit) from unaligned address */
#define trLD16LE(adr) ((uint32_t)*((uint8_t*)(adr))|((uint32_t)*(((uint8_t*)(adr))+1)<<8))
/** Load little endian 24 bit word (low 24 bit of a uint32_t) from unaligned address */
#define trLD24LE(adr) ((uint32_t)*((uint8_t*)(adr))|((uint32_t)*(((uint8_t*)(adr))+1)<<8)|((uint32_t)*(((uint8_t*)(adr))+2)<<16))
/** Load little endian uint32_t (32 bit) from unaligned address */
#define trLD32LE(adr) ((uint32_t)*((uint8_t*)(adr))|((uint32_t)*(((uint8_t*)(adr))+1)<<8)|((uint32_t)*(((uint8_t*)(adr))+2)<<16)|((uint32_t)*(((uint8_t*)(adr))+3)<<24))
/** Load little endian uint64_t (64 bit) from unaligned address */
#define trLD64LE(adr) ((uint64_t)trLD32LE(adr)|((uint64_t)trLD32LE((adr)+4)<<32))
/** Store uint16_t (16 bit) to unaligned address, stores in little endian order */
#define trST16LE(adr,w) *(uint8_t*)(adr)=(uint8_t)(w),*((uint8_t*)(adr)+1)=(uint8_t)((w)>>8)
/** Store 24 bit word (low 24 bit of a uint32_t) to unaligned address, stores in little endian order */
#define trST24LE(adr,w) *(uint8_t*)(adr)=(uint8_t)(w),*((uint8_t*)(adr)+1)=(uint8_t)((w)>>8),*((uint8_t*)(adr)+2)=(uint8_t)((w)>>16)
/** Store uint32_t (32 bit) to unaligned address, stores in little endian order */
#define trST32LE(adr,w) *(uint8_t*)(adr)=(uint8_t)(w),*((uint8_t*)(adr)+1)=(uint8_t)((w)>>8),*((uint8_t*)(adr)+2)=(uint8_t)((w)>>16),*((uint8_t*)(adr)+3)=(uint8_t)((w)>>24)
/** Store uint64_t (64 bit) to unaligned address, stores in little endian order */
#define trST64LE(adr,w) trST32LE(adr,(uint32_t)w),trST32LE((uint8_t*)(adr)+4,(uint32_t)(w>>32))
/** Load little endian int16_t (16 bit) from unaligned address and sign-extend */
#define trLDS16LE(adr) (int32_t)((uint32_t)*((uint8_t*)(adr))|((int32_t)*(((int8_t*)(adr))+1)<<8))
/** Load little endian 24 bit word (low 24 bit of a uint32_t) from unaligned address and sign-extend */
#define trLDS24LE(adr) (int32_t)((uint32_t)*((uint8_t*)(adr))|((uint32_t)*(((uint8_t*)(adr))+1)<<8)|((int32_t)*(((int8_t*)(adr))+2)<<16))

#endif

/** Load big endian uint32_t (32 bit) from unaligned address */
#define trLD32BE(adr) ((((uint32_t)*(uint8_t*)(adr))<<24)|(((uint32_t)*((uint8_t*)(adr)+1))<<16)|(((uint32_t)*((uint8_t*)(adr)+2))<<8)|((uint32_t)*((uint8_t*)(adr)+3)))
/** Load big endian uint32_t (64 bit) from unaligned address */
#define trLD64BE(adr) (((uint64_t)trLD32BE(adr)<<32)|(uint64_t)trLD32BE((adr)+4))
/** Load big endian 24 bit word (low 24 bit of a uint32_t) from unaligned address */
#define trLD24BE(adr) ((((uint32_t)*(uint8_t*)(adr))<<16)|(((uint32_t)*((uint8_t*)(adr)+1))<<8)|((uint32_t)*((uint8_t*)(adr)+2)))
/** Load big endian uint16_t (16 bit) from unaligned address */
#define trLD16BE(adr) ((((uint32_t)*(uint8_t*)(adr))<<8)|(uint32_t)*((uint8_t*)(adr)+1))
/** Store uint32_t (32 bit) to unaligned address, stores in big endian order */
#define trST32BE(adr,w) *(uint8_t*)(adr)=(uint8_t)((w)>>24),*((uint8_t*)(adr)+1)=(uint8_t)((w)>>16),*((uint8_t*)(adr)+2)=(uint8_t)((w)>>8), *((uint8_t*)(adr)+3)=(uint8_t)(w)
/** Store 24 bit word (low 24 bit of a uint32_t) to unaligned address, stores in big endian order */
#define trST24BE(adr,w) *(uint8_t*)(adr)=(uint8_t)((w)>>16),*((uint8_t*)(adr)+1)=(uint8_t)((w)>>8),*((uint8_t*)(adr)+2)=(uint8_t)(w)
/** Store uint16_t (16 bit) to unaligned address, stores in big endian order */
#define trST16BE(adr,w) *(uint8_t*)(adr)=(uint8_t)((w)>>8),*((uint8_t*)(adr)+1)=(uint8_t)(w)
/** Load big endian 24 bit word (low 24 bit of a uint32_t) from unaligned address and sign-extend */
#define trLDS24BE(adr) (int32_t)((((int32_t)*(int8_t*)(adr))<<16)|(((uint32_t)*((uint8_t*)(adr)+1))<<8)|((uint32_t)*((uint8_t*)(adr)+2)))
/** Load big endian int16_t (16 bit) from unaligned address and sign-extend */
#define trLDS16BE(adr) (int32_t)((((int32_t)*(int8_t*)(adr))<<8)|(uint32_t)*((uint8_t*)(adr)+1))
