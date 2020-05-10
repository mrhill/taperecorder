#pragma once

#include "trBuffer.h"
#include <vector>

class trBuffer_file : public trBuffer
{
protected:
    FILE*                mhFile = nullptr;
    uint64_t             mSize = 0;
    trSection            mSection = {0};
    std::vector<uint8_t> mBuffer;

    uint64_t GetFileSize();
    trERR FileSeek(uint64_t offset);

public:
    virtual uint64_t GetSize() const override { return mSize; }
	virtual trERR Open(const trChar* pFileName, trMAP access) override;
    virtual void Close() override;

    virtual uint32_t Read(uint8_t* pDst, uint64_t offset, uint32_t size) override;
    virtual trERR Write(uint64_t offset, uint8_t* pData, uint32_t size, int overwrite, void* user) override;

    virtual trSection* Map(uint64_t offset, uint32_t size, trMAP const accesshint) override;
    virtual trERR Commit(trSection* const pSection, void* const user) override;
    virtual void Discard(trSection* const pSection) override;
	virtual trSection* Insert(uint64_t offset, uint32_t size) override;
};
