#pragma once

#include "trdefs.h"
#include <cstdio>

#ifdef USE_DTBUFFER

#include "dt/dtBuffer.h"
using trBuffer = dtBuffer;
using trSection = trSection;

enum trMAP
{
    trMAP_READONLY = dtMAP_READONLY,
    trMAP_WRITE = dtMAP_WRITE
};

#else

struct trSection
{
    uint8_t* mpData;
    uint32_t mSize;
    uint64_t mOffset;
};

enum trMAP
{
    trMAP_READONLY = 0, //!< Hint for dtBuffer::MapSeq: access will be read-only
    trMAP_WRITE = 1     //!< Hint for dtBuffer::MapSeq: access will be read-write
};

class trBuffer
{
public:
	virtual trERR Open(const trChar* url, trMAP access) = 0;
    virtual void Close() = 0;
    virtual uint64_t GetSize() const = 0;

    virtual uint32_t Read(uint8_t* pDst, uint64_t offset, uint32_t size) = 0;
    virtual trERR Write(uint64_t offset, uint8_t* pData, uint32_t size, int overwrite, void* user) = 0;

    virtual trSection* Map(uint64_t offset, uint32_t size, trMAP const accesshint) = 0;
    virtual trERR Commit(trSection* const pSection, void* const user) = 0;
    virtual void Discard(trSection* const pSection) = 0;
	virtual trSection* Insert(uint64_t offset, uint32_t size) = 0;
};

#endif
