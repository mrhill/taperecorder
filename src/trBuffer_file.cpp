#include "trBuffer_file.h"

#ifdef _WIN32

#if _MSC_VER < 1000
int __cdecl _fseeki64(FILE*, __int64 , int);
__int64 __cdecl _ftelli64(FILE*);
#endif

#define tr_ftell _ftelli64
#define tr_fseek _fseeki64

#else

static_assert(sizeof(off_t) == 8, "off_t is not 64-bit");

#define tr_ftell ftello
#define tr_fseek fseeko

#endif

void trBuffer_file::Close()
{
    if (mhFile) fclose(mhFile);
    mSize = 0;
    mhFile = nullptr;
}

trERR trBuffer_file::Open(const trChar* pFileName, trMAP access)
{
    Close();

    const char* mode = access == trMAP_READONLY ? "rb" : "wb";
    mhFile = fopen(pFileName, mode);
    if (!mhFile)
        return trErrSet(trEIO);

    mSize = GetFileSize();

    return trEOK;
}

uint64_t trBuffer_file::GetFileSize()
{
    uint64_t size;

    const uint64_t pos = tr_ftell(mhFile);
    if (pos == (uint64_t)-1)
	{
    	trErrSet(trEIO);
        return pos;
	}

    tr_fseek(mhFile, 0, SEEK_END);
    size = tr_ftell(mhFile);

    tr_fseek(mhFile, pos, SEEK_SET);
    return size;
}

trERR trBuffer_file::FileSeek(uint64_t offset)
{
	if (!tr_fseek(mhFile, offset, SEEK_SET))
		return trEOK;
	return trErrSet(trEIO);
}

uint32_t trBuffer_file::Read(uint8_t* pDst, uint64_t offset, uint32_t size)
{
    if (FileSeek(offset) != trEOK)
        return size;

    int actuallyRead = fread(pDst, 1, size, mhFile);
    if (actuallyRead == (int)size)
        return 0;

    trErrSet(feof(mhFile) ? trEEOF : trEIO);
    return actuallyRead > 0 ? (size - actuallyRead) : size;
}

trERR trBuffer_file::Write(uint64_t offset, uint8_t* pData, uint32_t size, int overwrite, void* user)
{
    if (!overwrite)
        return trErrSet(trENOTSUP);

    if (FileSeek(offset) != trEOK)
        return trELAST;

    int written = fwrite(pData, 1, size, mhFile);
    if (written == size)
    {
        if ((offset + size) > mSize)
            mSize = offset + size;
        return trEOK;
    }

    if ((written > 0) && ((offset + written) > mSize))
        mSize = offset + written;

    return trErrSet(trEIO);
}

trSection* trBuffer_file::Map(uint64_t offset, uint32_t size, trMAP const accesshint)
{
    if (mSection.mpData || (offset + size) > mSize)
    {
        trErrSet(trEBADPARAM);
        return nullptr;
    }

    //if (mpMmap) xxx

    if (size > mBuffer.size())
        mBuffer.resize(size);

    if (Read(mBuffer.data(), offset, size))
        return nullptr;

    mSection.mpData = mBuffer.data();
    mSection.mSize = size;
    mSection.mOffset = offset;
    return &mSection;
}

trERR trBuffer_file::Commit(trSection* const pSection, void* const user)
{
    assert(pSection == &mSection);
    assert(mSection.mpData != nullptr);
    return Write(pSection->mOffset, pSection->mpData, pSection->mSize, 1, user);
}

void trBuffer_file::Discard(trSection* const pSection)
{
    if (!pSection)
        return;
    assert(pSection == &mSection);
    assert(mSection.mpData != nullptr);
    mSection.mpData = nullptr;
    mSection.mSize = 0;
}

trSection* trBuffer_file::Insert(uint64_t offset, uint32_t size)
{
    if (mSection.mpData || offset > mSize)
    {
        trErrSet(trEBADPARAM);
        return nullptr;
    }

    if (offset != mSize)
    {
        trErrSet(trENOTSUP);
        return nullptr;
    }

    if (size > mBuffer.size())
        mBuffer.resize(size);

    mSection.mpData = mBuffer.data();
    mSection.mSize = size;
    mSection.mOffset = offset;
    return &mSection;
}
