#include "trSample_raw.h"

uint32_t trSample_raw::FindWavChunk(const char* const pName, uint64_t* const pOffset)
{
    uint32_t chunksize, name;
    uint64_t offset = *pOffset;
    trSection* pSec;
    uint8_t* pMap;

    for(;;)
    {
        // align RIFF chunk to 2 bytes
        if (offset & 1)
            offset++;

        if ((pSec = mpBuf->Map(offset, 8, trMAP_READONLY)) == nullptr)
        {
            chunksize = (uint32_t)-1;
            break;
        }
        pMap = pSec->mpData;
        offset += 8;

        chunksize = trLD32LE(pMap + 4);
        name = *(uint32_t*)pMap;

        mpBuf->Discard(pSec);

        if (*(uint32_t*)pName == name)
            break;

        offset += chunksize;
        if ((offset + 8) > mpBuf->GetSize())
        {
            chunksize = (uint32_t)-1;
            trErrSet(trENOTFOUND);
            break;
        }
    }

    *pOffset = offset;
    return chunksize;
}

trERR trSample_raw::InitWav()
{
    uint32_t   chunksize;
    uint64_t   offset;
    trSection* pSec;
    uint8_t*   pMap;
    static char const magic[8] = {'R','I','F','F','W','A','V','E' };

    // - test for wav file (check for magic strings)

    if ((pSec = mpBuf->Map(0, 12, trMAP_READONLY)) == nullptr)
        goto trSample_wav_Open_err;

    pMap = pSec->mpData;
    if ( (*(uint32_t*)&pMap[0] != *(uint32_t*)&magic[0]) ||
         (*(uint32_t*)&pMap[8] != *(uint32_t*)&magic[4]) )
    {
        trLog(trInf, trT("WAV file format not recognized"));
        trErrSet(trEFILEFORMAT);
        goto trSample_wav_Open_err;
    }

    mpBuf->Discard(pSec);
    pSec = nullptr;

    // - read format chunk

    offset = 12;
    if ((chunksize = FindWavChunk("fmt ", &offset)) == (uint32_t)-1)
    {
        if (trErrGet() == trENOTFOUND)
            trLog(trInf, trT("WAV fmt chunk not found"));

        goto trSample_wav_Open_err;
    }

    if ((pSec = mpBuf->Map(offset, 16, trMAP_READONLY)) == nullptr)
        goto trSample_wav_Open_err;

    pMap = pSec->mpData;
    if ((pMap[0]!=1) || (pMap[1]!=0))
    {
        trLog(trInf, trT("Error reading WAV, format not supported\n"));
        trErrSet(trENOTSUP);
        goto trSample_wav_Open_err;
    }

    mChannels = (unsigned)pMap[2] | ((unsigned)pMap[3]<<8);
    mRate = trLD32LE(&pMap[4]);
    mBits = (unsigned)pMap[14] | ((unsigned)pMap[15]<<8);

    if (mBits > 16)
    {
        trLog(trInf, trT("Error reading WAV, %d bits per sample not supported\n"), mBits);
        trErrSet(trENOTSUP);
        goto trSample_wav_Open_err;
    }

    mType = (mBits<=8) ? trSAMPLETYPE_U8 : trSAMPLETYPE_S16;
    mFormat = trSAMPLERAWFORMAT_INTERLEAVE;
    
    offset += chunksize;
    mpBuf->Discard(pSec);
    pSec = nullptr;

    if ((chunksize = FindWavChunk("data", &offset)) == (uint32_t)-1)
    {
        if (trErrGet() == trENOTFOUND)
            trLog(trInf, trT("WAV data chunk not found"));
        goto trSample_wav_Open_err;
    }

    mStart   = (uint64_t) offset;
    mByteLen = (uint64_t) chunksize;
    mLen     = (uint64_t) chunksize / (mChannels * trgSampleTypeSizes[mType]);

    mFileFormat = trSAMPLE_RAW_FORMAT_WAV;

    mpBuf->Discard(pSec);

    return trEOK;

    trSample_wav_Open_err:
    mpBuf->Discard(pSec);
    Destroy();
    return trELAST;
}

trERR trSample_raw::Init(trBuffer* pBuf)
{
    if (trSample::Init(pBuf) != trEOK)
        goto trSample_wav_Init_err;

    if (InitWav() == trEOK)
        return trEOK;

    mFileFormat = trSAMPLE_RAW_FORMAT_RAW;
    mStart      = 0;
    mByteLen    = pBuf->GetSize();
    mRate       = 32000;
    mBits       = 8;
    mType       = trSAMPLETYPE_U8;
    mFormat     = trSAMPLERAWFORMAT_INTERLEAVE;
    mEndianess  = trSAMPLEENDIAN_LE;
    SetChannels(1); //  updates mLen and mChannels

    return trEOK;

    trSample_wav_Init_err:
    Destroy();
    return trELAST;
}

void trSample_raw::Destroy()
{
    trSample::Destroy();
}

uint8_t* trSample_raw::Map(uint64_t offset, uint32_t size)
{
    assert((mFormat!=trSAMPLERAWFORMAT_INTERLEAVE) || (((offset + size) % trgSampleTypeSizes[mType]) == 0));

    mpSection = mpBuf->Map(mStart + offset, size, trMAP_READONLY);
    if (!mpSection)
        return nullptr;
        
    return mpSection->mpData;
}

void trSample_raw::Discard(uint8_t* /*pBuf*/)
{
    mpBuf->Discard(mpSection);
}

trERR trSample_raw::Commit(uint8_t* /*pBuf*/)
{
    return mpBuf->Commit(mpSection, nullptr);
}

