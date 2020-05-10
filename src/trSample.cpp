#include "trSample.h"
#include <strings.h>
#include <cstddef>

const uint8_t trgSampleTypeSizes[] = { trSAMPLETYPESIZES };

trSAMPLETYPE trSampleBitsToType(unsigned bits, int sign)
{
    trSAMPLETYPE type;

    switch(bits)
    {
    case 8:  type=trSAMPLETYPE_U8; break;
    case 24: type=trSAMPLETYPE_U16; break;
    case 16: type=trSAMPLETYPE_U24; break;
    case 32: type=trSAMPLETYPE_U32; break;
    default: type=trSAMPLETYPE_UNKNOWN; break;
    }

    if (sign) type = (trSAMPLETYPE)(((int)type)+1);

    return type;
}

trSample::trSample()
{
    bzero((char*)this, sizeof(trSample));
}

trSample::~trSample()
{
}

trERR trSample::Init(trBuffer* pBuf)
{
    mpBuf = pBuf;
    mFrameSize = 256;
    return trEOK;
}

void trSample::Destroy()
{
}

void trSample::SetType(trSAMPLETYPE const type)
{
    assert(type < trSAMPLETYPECOUNT);
    mType = (uint8_t)type;
    mBits = trgSampleTypeSizes[mType] << 3;
    UpdateLen();
}

void trSample::SetChannels(unsigned const channels)
{
    assert(channels <= 256);
    mChannels = (uint16_t)channels;
    UpdateLen();
}

void trSample::UpdateLen()
{
    unsigned bytes  = trgSampleTypeSizes[mType];
    unsigned factor = mChannels * bytes;

    if (mFormat == trSAMPLERAWFORMAT_INTERLEAVE)
    {
        mLen = mByteLen / factor;
        if (mByteLen-(mLen*factor)) mLen++;
        mFrameLen = mLen / mFrameSize;
        if (mLen-(mFrameLen*mFrameSize)) mFrameLen++;
    }
    else if (mFormat == trSAMPLERAWFORMAT_CHUNKY)
    {
        factor *= mFrameSize;
        mFrameLen = mByteLen / factor;
        if (mByteLen-(mFrameLen*factor)) mFrameLen++;
        mLen = mFrameLen * mFrameSize;
    }
}
    
void trSample::SetRawFormat(trSAMPLERAWFORMAT const format)
{
    assert(format < trSAMPLERAWFORMATCOUNT);
    mFormat = (uint8_t)format;
}

trERR trSample::ReadChannel(unsigned const channel, void* pBuf, uint64_t offset, uint32_t size, trSAMPLETYPE dstType)
{
    unsigned step;
    uint8_t*  pMap;
    uint8_t*  pSrc;
    uint32_t  center;//xxx , toread;
    trERR  err = trEOK;

    union
    {
        uint64_t u64;
        uint32_t u32;
        double   f64;
        float    f32;
    };

    assert(mFormat <= trSAMPLERAWFORMAT_INTERLEAVE);
    assert(mType < trSAMPLETYPE_UNKNOWN);
    assert(channel < mChannels);

    if ((offset + size) > mLen)
        return trErrSet(trEEOF); //xxx

    if (mFormat == trSAMPLERAWFORMAT_INTERLEAVE)
    {
        step = trgSampleTypeSizes[mType] * mChannels;
        pMap = Map(offset * step, size * step);
    }
    else
    {
        uint32_t const frameoffs = (uint32_t)(offset % mFrameSize);
        offset = (offset - frameoffs) * mChannels;
        step = trgSampleTypeSizes[mType];
        pMap = Map((offset + frameoffs) * step, (mFrameSize - frameoffs) * step);
    }

    do
    {
        if (!pMap)
            return trELAST;

        pSrc   = pMap + channel * trgSampleTypeSizes[mType];
        center = 0;

        if ((dstType == trSAMPLETYPE_S32) || (dstType == trSAMPLETYPE_U32))
        {
            uint32_t* pBuf32 = (uint32_t*)pBuf;
            uint32_t* const pBuf32End = pBuf32 + size;

            if (dstType == trSAMPLETYPE_U32)
                center = 0x80000000UL;

            switch (mType)
            {
            case trSAMPLETYPE_U8:
                center -= 0x80000000U;
            case trSAMPLETYPE_S8:
                while (pBuf32 < pBuf32End)
                {
                    *(pBuf32++) = ((uint32_t)*(pSrc)<<24) + center;
                    pSrc+=step;
                }
                break;

            case trSAMPLETYPE_U16:
                center -= 0x80000000U;
            case trSAMPLETYPE_S16:
                if (mEndianess == trSAMPLEENDIAN_LE)
                    while (pBuf32 < pBuf32End) { *(pBuf32++) = (trLD16LE(pSrc)<<16) + center; pSrc+=step; }
                else
                    while (pBuf32 < pBuf32End) { *(pBuf32++) = (trLD16BE(pSrc)<<16) + center; pSrc+=step; }
                break;

            case trSAMPLETYPE_U24:
                center -= 0x800000U;
            case trSAMPLETYPE_S24:
                if (mEndianess == trSAMPLEENDIAN_LE)
                    while (pBuf32 < pBuf32End) { *(pBuf32++) = (trLD24LE(pSrc)<<8) + center; pSrc+=step; }
                else
                    while (pBuf32 < pBuf32End) { *(pBuf32++) = (trLD24BE(pSrc)<<8) + center; pSrc+=step; }
                break;

            case trSAMPLETYPE_U32:
                center -= 0x80000000U;
            case trSAMPLETYPE_S32:
                if (mEndianess == trSAMPLEENDIAN_LE)
                    while (pBuf32 < pBuf32End) { *(pBuf32++) = trLD32LE(pSrc) + center; pSrc+=step; }
                else
                    while (pBuf32 < pBuf32End) { *(pBuf32++) = trLD32BE(pSrc) + center; pSrc+=step; }
                break;

            case trSAMPLETYPE_FLOAT:
                if (mEndianess == trSAMPLEENDIAN_LE)
                {
                    while (pBuf32 < pBuf32End)
                    {
                        u32 = trLD32LE(pSrc);
                        *(pBuf32++) = (uint16_t)((int)(f32 * 2147483648.0f) + center);
                        pSrc+=step;
                    }
                }
                else
                {
                    while (pBuf32 < pBuf32End)
                    {
                        u32 = trLD32BE(pSrc);
                        *(pBuf32++) = (uint16_t)((int)(f32 * 2147483648.0f) + center);
                        pSrc+=step;
                    }
                }
                break;

            case trSAMPLETYPE_DOUBLE:
                if (mEndianess == trSAMPLEENDIAN_LE)
                {
                    while (pBuf32 < pBuf32End)
                    {
                        u64 = (uint64_t)trLD32LE(pSrc) | ((uint64_t)trLD32LE(pSrc+4)<<32);
                        *(pBuf32++) = (uint16_t)((int)(f64 * 2147483648.0f) + center);
                        pSrc+=step;
                    }
                }
                else
                {
                    while (pBuf32 < pBuf32End)
                    {
                        u64 = (uint64_t)trLD32BE(pSrc+4) | ((uint64_t)trLD32BE(pSrc)<<32);
                        *(pBuf32++) = (uint16_t)((int)(f64 * 2147483648.0f) + center);
                        pSrc+=step;
                    }
                }
                break;
            }
        }
        else if ((dstType == trSAMPLETYPE_S16) || (dstType == trSAMPLETYPE_U16))
        {
            uint16_t* pBuf16 = (uint16_t*)pBuf;
            uint16_t* const pBuf16End = pBuf16 + size;

            if (dstType == trSAMPLETYPE_U16)
                center = 0x8000U;

            switch (mType)
            {
            case trSAMPLETYPE_U8:
                center -= 0x8000U;
            case trSAMPLETYPE_S8:
                while (pBuf16 < pBuf16End)
                {
                    *(pBuf16++) = (uint16_t)(((uint32_t)*(pSrc)<<8) + center);
                    pSrc+=step;
                }
                break;

            case trSAMPLETYPE_U16:
                center -= 0x8000U;
            case trSAMPLETYPE_S16:
                if (mEndianess == trSAMPLEENDIAN_LE)
                    while (pBuf16 < pBuf16End) { *(pBuf16++) = (uint16_t)(trLD16LE(pSrc) + center); pSrc+=step; }
                else
                    while (pBuf16 < pBuf16End) { *(pBuf16++) = (uint16_t)(trLD16BE(pSrc) + center); pSrc+=step; }
                break;

            case trSAMPLETYPE_U24:
                center -= 0x800000U;
            case trSAMPLETYPE_S24:
                if (mEndianess == trSAMPLEENDIAN_LE)
                    while (pBuf16 < pBuf16End) { *(pBuf16++) = (uint16_t)(((uint32_t)pSrc[1] | ((uint32_t)pSrc[2]<<8)) + center); pSrc+=step; }
                else
                    while (pBuf16 < pBuf16End) { *(pBuf16++) = (uint16_t)(((uint32_t)pSrc[1] | ((uint32_t)pSrc[0]<<8)) + center); pSrc+=step; }
                break;

            case trSAMPLETYPE_U32:
                center -= 0x8000U;
            case trSAMPLETYPE_S32:
                if (mEndianess == trSAMPLEENDIAN_LE)
                    while (pBuf16 < pBuf16End) { *(pBuf16++) = (uint16_t)(((uint32_t)pSrc[2] | ((uint32_t)pSrc[3]<<8)) + center); pSrc+=step; }
                else
                    while (pBuf16 < pBuf16End) { *(pBuf16++) = (uint16_t)(((uint32_t)pSrc[1] | ((uint32_t)pSrc[0]<<8)) + center); pSrc+=step; }
                break;

            case trSAMPLETYPE_FLOAT:
                if (mEndianess == trSAMPLEENDIAN_LE)
                {
                    while (pBuf16 < pBuf16End)
                    {
                        u32 = trLD32LE(pSrc);
                        *(pBuf16++) = (uint16_t)((int)(f32 * 32768.0f) + center);
                        pSrc+=step;
                    }
                }
                else
                {
                    while (pBuf16 < pBuf16End)
                    {
                        u32 = trLD32BE(pSrc);
                        *(pBuf16++) = (uint16_t)((int)(f32 * 32768.0f) + center);
                        pSrc+=step;
                    }
                }
                break;

            case trSAMPLETYPE_DOUBLE:
                if (mEndianess == trSAMPLEENDIAN_LE)
                {
                    while (pBuf16 < pBuf16End)
                    {
                        u64 = (uint64_t)trLD32LE(pSrc) | ((uint64_t)trLD32LE(pSrc+4)<<32);
                        *(pBuf16++) = (uint16_t)((int)(f64 * 32768.0f) + center);
                        pSrc+=step;
                    }
                }
                else
                {
                    while (pBuf16 < pBuf16End)
                    {
                        u64 = (uint64_t)trLD32BE(pSrc+4) | ((uint64_t)trLD32BE(pSrc+4)<<32);
                        *(pBuf16++) = (uint16_t)((int)(f64 * 32768.0f) + center);
                        pSrc+=step;
                    }
                }
                break;
            }
        }
        else
        {
            err = trErrSet(trENOTSUP);
        }

        Discard(pMap);
    
    } while(0);//xxx

    return err;
}

void trSample::Discard(uint8_t* /*pBuf*/)
{
}

trERR trSample::Commit(uint8_t* /*pBuf*/)
{
    return trErrSet(trENOTSUP);
}


