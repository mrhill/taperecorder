#include "trDataAnalyzer.h"
#include "trSample.h"
#include <strings.h>
#include <string.h>

#define trMODKC_REFSAMPSIZE 32

static const int8_t gModKC_1200Hz_data[trMODKC_REFSAMPSIZE] =
{
    // 1200 Hz * 32 = 38400 Hz samplerate
       806/256,  8615/256, 16686/256, 23668/256, 28444/256, 30386/256, 28377/256, 25019/256,
     22115/256, 19681/256, 17336/256, 15397/256, 13424/256, 11055/256,  7975/256,  3498/256,
     -2566/256,-10715/256,-18848/256,-25855/256,-30317/256,-30475/256,-28474/256,-25313/256,
    -22119/256,-19556/256,-17474/256,-15504/256,-13298/256,-10617/256, -6965/256, -3468/256,
};

static const trDataRefSignal gModKC_1200Hz = { gModKC_1200Hz_data, trMODKC_REFSAMPSIZE, trMODKC_REFSAMPSIZE*1026, 1026 }; // 43.0 samples @ 44100 Hz
static const trDataRefSignal gModKC_600Hz  = { gModKC_1200Hz_data, trMODKC_REFSAMPSIZE, trMODKC_REFSAMPSIZE*558,   558 }; // 79.0 samples @ 44100 Hz
static const trDataRefSignal gModKC_2400Hz = { gModKC_1200Hz_data, trMODKC_REFSAMPSIZE, trMODKC_REFSAMPSIZE*1877, 1877 }; // 23.5 samples @ 44100 Hz

#define trDATAKC_CHUNKSIZE 130

int32_t trDataRefSignal::Autocorrelate(unsigned const skew) const
{
    unsigned i = mLength;
    int32_t sum = 0;
    unsigned const mask = mLength-1;
    uint8_t* const pData = (uint8_t*)mpData;
    do
    {
        --i;
        if ((pData[(i-skew)&mask] ^ pData[i]) & 0x80)
            sum--;
        else
            sum++;
    } while (i);

    return sum;
}

trData::trData(trDATAFORMAT const format, uint8_t const SampleID)
{
	bzero(this, sizeof(trData));

    mFormat = (uint8_t)format;
    mSampleID = SampleID;

    switch(format)
    {
    case trDATAFORMAT_KC:
        mChunkSize = trDATAKC_CHUNKSIZE;
        break;
    default:
        assert(0);
    }
}

trData::~trData()
{
}

uint8_t* trData::AddChunk()
{
	mData.resize(mData.size() + mChunkSize);
    return mData.data() + mChunkSize * mChunkCount++;
}

unsigned trData::GetName(trChar* pName)
{
    unsigned i, j, len = 0;

    switch(mFormat)
    {
    case trDATAFORMAT_KC:
        if (mChunkCount)
        {
            const uint8_t* pChunk = mData.data();
            if (pChunk[0] == 0x01) // is first block KC header block?
            {
                static const uint8_t fmt2ext[] = { trDATAFILEFORMATEXT };
                static const uint8_t typ2fmt[] = { 
                    (uint8_t)trDATAFILEFORMAT_OVR,'O' ,'V' ,'R', 
                    (uint8_t)trDATAFILEFORMAT_TXW,'T' ,'X' ,'W',
                };

                if ((pChunk[1]==0xD3) && (pChunk[2]==0xD3) && (pChunk[3]==0xD3))
                {
                    mFileFormat = trDATAFILEFORMAT_KCB;
                    pChunk+=3;
                }
                else if ((pChunk[1]==0xD4) && (pChunk[2]==0xD4) && (pChunk[3]==0xD4))
                {
                    mFileFormat = trDATAFILEFORMAT_TTT;
                    pChunk+=3;
                }
                else
                {
                    mFileFormat = trDATAFILEFORMAT_KCC;

                    for (j=0; j<4*2; j+=4)
                    {
                        if ((pChunk[9]==typ2fmt[j+1]) && (pChunk[10]==typ2fmt[j+2]) && (pChunk[11]==typ2fmt[j+3]))
                        {
                            mFileFormat = typ2fmt[j];
                            break;
                        }
                    }
                }

                while ((len < 8) && (pChunk[len+1]>32))
                    len++;

                if (pName)
                {
                    for (i=0; i<len; i++)
                    {
                        pName[i] = (trChar)pChunk[i+1];
                    }
                    pName[i] = '.';
                    pName[i+1] = fmt2ext[mFileFormat*3 + 0];
                    pName[i+2] = fmt2ext[mFileFormat*3 + 1];
                    pName[i+3] = fmt2ext[mFileFormat*3 + 2];
                }

                len+=4;
            }
        }
        break;
    }

    return len;
}

trERR trData::SaveFile(trBuffer* const pBuf)
{
    unsigned i;
    uint8_t* pChunk;
    trSection* pMap = nullptr;

    switch(mFormat)
    {
    case trDATAFORMAT_KC:

        if ((pMap = pBuf->Insert(pBuf->GetSize(), mChunkCount*128)) == nullptr)
            return trELAST;
        
        pChunk = mData.data() + 1;
        for (i=0; i<mChunkCount; i++)
        {
        	memmove(pMap->mpData + i*128, pChunk, 128);
            pChunk += mChunkSize;
        }
        break;

    default:
        return trENOTSUP;
    }

    return pBuf->Commit(pMap, 0);
}

trDataAnalyzer::trDataAnalyzer()
{
    mpSample = nullptr;
}

trDataAnalyzer::~trDataAnalyzer()
{
    unsigned i = mFiles.size();
    while(i--)
        delete mFiles[i];
}

trERR trDataAnalyzer::SetSample(trSample* const pSample, uint8_t const sampleID)
{
    if (pSample->mLen >= ((uint64_t)1 << 48))
    {
        trLog(trWrn, trT("SetSample: sample too large"), pSample->mRate);
        return trErrSet(trENOTSUP);
    }

    //
    // Allocate approx. 1 second of circular sample buffer, aligned to next power of 2
    //
    uint32_t sampSize = 1UL << trGetTopBit(pSample->mRate);
    if (sampSize != pSample->mRate) sampSize<<=1;

    if ((sampSize < 32) || (sampSize > 65536))
    {
        trLog(trWrn, trT("SetSample: non-supported sample rate %u Hz"), pSample->mRate);
        return trErrSet(trENOTSUP);
    }

    mSampBuf.resize(sampSize);

    mSampStart = mSampOffset = 0;
    mSampEnd = pSample->mLen;
    mSampBufWr = mSampBufRd = 0;

    mpSample = pSample;
    mSampleID = sampleID;
	mChannel = 0;

    return trEOK;
}

trERR trDataAnalyzer::SetSection(uint64_t offset, uint64_t length)
{
    assert(mpSample);

    if (length == 0)
        length = mpSample->mLen - offset;

    uint64_t const end = offset + length;

    if ((offset > mpSample->mLen) || (end > mpSample->mLen))
        return trEBADPARAM;

    mSampStart = mSampOffset = offset;
    mSampEnd = end;
    mSampBufWr = mSampBufRd = 0;

    return trEOK;
}

trERR trDataAnalyzer::SetChannel(unsigned channel)
{
    assert(mpSample);

    if (channel >= mpSample->mChannels)
        return trErrSet(trEBADPARAM);
    mChannel = channel;
    return trEOK;
}

trERR trDataAnalyzer::SampBufPrefill(uint64_t newOffset)
{
    assert(mpSample);

    uint32_t const mask   = mSampBuf.size() - 1;
    uint32_t       filled = (mSampBufWr - mSampBufRd) & mask; // current number of samples in buffer
    uint32_t const diff   = (uint32_t)(newOffset - mSampOffset);

    mSampOffset = newOffset;

    if (diff < filled)
    {
        // new offset is within cached sample section, reuse data
        mSampBufRd = (mSampBufRd + diff) & mask;
        filled -= diff;

        if (filled > (mask >> 1)) // ensure at least mSampBuf.size()/2 samples to be available
            return trEOK;

        newOffset += filled;
        uint32_t toread = mask - filled;
        int32_t const towrap = toread - (mSampBuf.size() - mSampBufWr);

        if (towrap > 0)
            toread -= towrap;

        if (mpSample->ReadChannel(mChannel, (int16_t*)mSampBuf.data() + mSampBufWr, newOffset, toread, trSAMPLETYPE_S16) != trEOK)
            return trELAST;

        mSampBufWr = (mSampBufWr + toread) & mask;

        if (towrap > 0)
        {
            if (mpSample->ReadChannel(mChannel, (int16_t*)mSampBuf.data(), newOffset + toread, towrap, trSAMPLETYPE_S16) != trEOK)
                return trELAST;

            mSampBufWr = (mSampBufWr + towrap) & mask;
        }
    }
    else
    {
        // new offset is outside cached sample section, start over
        mSampBufRd = 0;
        mSampBufWr = mSampBuf.size() >> 1;

        if (mpSample->ReadChannel(mChannel, (int16_t*)mSampBuf.data(), newOffset, mSampBufWr, trSAMPLETYPE_S16) != trEOK)
        {
            mSampBufWr = 0;
            return trELAST;
        }
    }

    return trEOK;
}

void trDataAnalyzer::SampBufSkip(uint32_t offset)
{
    mSampOffset += offset;
    assert(mSampOffset < mSampEnd);

    uint32_t const mask = mSampBuf.size() - 1;
    uint32_t const left = (mSampBufWr - mSampBufRd) & mask; // current number of samples in buffer
    if (offset < left)
        mSampBufRd = (mSampBufRd + offset) & mask;
    else
        mSampBufRd = mSampBufWr;
}

void trDataAnalyzer::SampBufSkipZeroCross(uint32_t offset, uint32_t range)
{
    uint32_t const mask = mSampBuf.size() - 1;
#ifndef NDEBUG
    uint32_t const left = (mSampBufWr - mSampBufRd) & mask; // current number of samples in buffer
#endif
    assert(range <= offset);
    //assert((mSampOffset + offset + range) < mSampEnd);
    assert((offset + range) <= left);

    uint32_t RdM = mSampBufRd + offset;
    uint32_t RdL = RdM - range;
    uint32_t RdR = RdM + range;

    uint16_t* pData = mSampBuf.data();
    unsigned a = (unsigned)pData[RdL & mask];
    unsigned b = (unsigned)pData[RdR & mask];

    if ((a ^ b) & 0x8000U)
    {
        for(;;)
        {
            RdM = (RdL+RdR)>>1;

            if (RdM==RdL)
                break;

            if ((a ^ (unsigned)pData[RdM & mask]) & 0x8000U)
            {
                RdR = RdM;
                b = (unsigned)pData[RdR & mask];
            }
            else
            {
                RdL = RdM;
                a = (unsigned)pData[RdL & mask];
            }
        }

        if (a & 0x8000) a ^= 0xFFFF;
        if (b & 0x8000) b ^= 0xFFFF;
        RdM = (a<b) ? RdL : RdR;
    }
    else
    {
        if (a & 0x8000)
        {
            a ^= 0xFFFF;
            b ^= 0xFFFF;
        }
        RdM = (a<b) ? RdL : RdR;
    }

    mSampOffset += RdM - mSampBufRd;
    mSampBufRd = RdM & mask;
}

uint32_t trDataAnalyzer::Correlate(const trDataRefSignal* pRefWave, uint32_t step, unsigned count, uint32_t skew)
{
    int16_t*  pBuf    = (int16_t*)mSampBuf.data();
    uint32_t  bufMask = (mSampBuf.size() << 16) - 1;
    uint32_t  bufRd   = ((mSampBufRd<<16) + skew) & bufMask;
    int32_t   sum     = 0;

    assert(((skew + step * pRefWave->mLength * count)>>16) <= ((mSampBufWr - mSampBufRd) & (bufMask>>16)));

    const uint8_t*       pRef    = (uint8_t*)pRefWave->mpData;
    const uint8_t* const pRefEnd = pRef + pRefWave->mLength;

    for(;;)
    {
        //printf("%d: %d: %04X %04X\n", i++, bufRd>>16, pBuf[bufRd>>16], *pRef<<8);

        if (((unsigned)pBuf[bufRd>>16] ^ ((unsigned)*(pRef++)<<8)) & 0x8000U) // xxx optimize with 32 bitmask
            sum--;
        else
            sum++;

        if (pRef >= pRefEnd)
        {
            if (--count == 0)
                break;
            pRef = (uint8_t*)pRefWave->mpData;
        }

        bufRd = (bufRd + step) & bufMask;
    }

    if (sum<0) sum=0-sum;

    return (uint32_t) sum;
}

uint32_t trDataAnalyzer::ScanRefWave(const trDataRefSignal* pRefWave, unsigned count,
    uint32_t const SampStep, uint32_t const LinearSearchStep, uint32_t const LinearSearchThreshold)
{
    // Linear search in sample for count occurences of pRefWave[]
    uint64_t       SampOffset    = mSampOffset << 16;
    uint64_t const SampEnd       = mSampEnd << 16;
    uint32_t const ScaledRefSize = SampStep * pRefWave->mLength;

    assert(((LinearSearchStep + ScaledRefSize*count)>>16) <= (mSampBuf.size()>>1));

    for(;;)
    {
        if ((SampOffset + ScaledRefSize) >= SampEnd)
            break;

        if (SampBufPrefill(SampOffset >> 16) != trEOK)
            return 0;

        uint32_t Sum = Correlate(pRefWave, SampStep, count, 0);

        if (Sum > LinearSearchThreshold)
        {
            // located reference signal, now perform binary detail search to
            // determine exact start offset

            uint32_t SampSkewM, SampSkewL = 0, SampSkewR = LinearSearchStep;

            for (unsigned i=12; i!=0; --i) // xxx max steps fixed, make dynamic later
            {
                SampSkewM = (SampSkewL + SampSkewR) >> 1;

                if ((SampSkewL ^ SampSkewM) < (1UL<<16))
                    break; // early out

                uint32_t const SumBinary = Correlate(pRefWave, SampStep, count, SampSkewM);

                if (SumBinary <= Sum)
                {
                    SampSkewR = SampSkewM;
                }
                else
                {
                    SampSkewL = SampSkewM;
                    Sum = SumBinary;
                }
            }

            SampBufSkip(SampSkewL >> 16);
            return (ScaledRefSize * count) >> 16;
        }

        SampOffset += LinearSearchStep;
    }

    trErrSet(trENOTFOUND);
    return 0;
}

enum trMODSTATE
{
    trMODSTATE_SEARCH
};

trERR trDataAnalyzer::Scan()
{
    return ScanKC();
}

enum trMODKC_STATE
{
    trMODKC_STATE_SYNCSTART,    //!< Searching for file start synctone
    trMODKC_STATE_SYNCCHUNK,    //!< Searching for packet start synctone
    trMODKC_STATE_SYNCSTOP,     //!< Synctone found, searching for stopbit
    trMODKC_STATE_STOP,         //!< searching for stopbit
    trMODKC_STATE_DATA          //!< Reading 1 byte of data
};

enum trMODKC_FREQ
{
    trMODKC_FREQ_SYNC = 1200,
    trMODKC_FREQ_NULL = 2400,
    trMODKC_FREQ_EINS = 1200,
    trMODKC_FREQ_STOP = 600
};

/* KC modulation format
   128 byte Packet:
   - Synctone, N waves, 1200 Hz
   - Stopbit,  1 waves, 600 Hz
   - Byte,     8 waves, 0 = 2400 Hz, 1 = 1200 Hz
   - Stopbit,  1 waves, 600 Hz
*/

trERR trDataAnalyzer::ScanKC()
{
    int i;
    uint32_t size;
    uint32_t ByteCount = 0, NextValidBlock = 0;
    trData* pFile = nullptr;
    uint8_t* pChunk = nullptr;
    trMODKC_STATE modState = trMODKC_STATE_SYNCSTART;
    trChar filename[16];

    #define trMODKC_SYNCTONEREPEAT 6 // Minimum length of synctone in waves
    #define trMODKC_LINEARSCANSTEP 12 // Step for linear correlation scan in 1/N-th of a wave

    uint32_t const SyncToneThreshold2       = gModKC_1200Hz.Autocorrelate(2);
    uint32_t const SyncToneThreshold        = SyncToneThreshold2 * trMODKC_SYNCTONEREPEAT;
    uint32_t const SyncToneLinearSearchStep = (mpSample->mRate<<16) / (trMODKC_LINEARSCANSTEP * gModKC_1200Hz.mHighestFreq);

    uint32_t const StopBitThreshold        = gModKC_600Hz.Autocorrelate(2);
    uint32_t const StopBitSampStep         = (mpSample->mRate<<16) / gModKC_600Hz.mRate;
    uint32_t const StopBitLinearSearchStep = (mpSample->mRate<<16) / (trMODKC_LINEARSCANSTEP * gModKC_600Hz.mHighestFreq);
    uint32_t const StopBitSampSize         = (StopBitSampStep * gModKC_600Hz.mLength) >> 16;

    uint32_t const Bit1SampStep  = (mpSample->mRate<<16) / gModKC_1200Hz.mRate;
    uint32_t const Bit0SampStep  = (mpSample->mRate<<16) / gModKC_2400Hz.mRate;
    uint32_t const Bit1Threshold = gModKC_1200Hz.Autocorrelate(4);
    uint32_t const Bit0Threshold = gModKC_2400Hz.Autocorrelate(6);
    uint32_t const Bit1SampSize  = (Bit1SampStep * gModKC_1200Hz.mLength) >> 16;
    uint32_t const Bit0SampSize  = (Bit0SampStep * gModKC_2400Hz.mLength) >> 16;

    for(;;)
    {
        switch (modState)
        {
        case trMODKC_STATE_SYNCSTART:
        case trMODKC_STATE_SYNCCHUNK:

            if ((size = ScanRefWave(&gModKC_1200Hz, trMODKC_SYNCTONEREPEAT, Bit1SampStep, SyncToneLinearSearchStep, SyncToneThreshold)) == 0)
                goto trDataAnalyzer_ScanKC_err;

            if (modState == trMODKC_STATE_SYNCSTART)
            {
                //xxx assert((SyncToneLinearSearchStep+1) <= ((mSampBufWr - mSampBufRd) & (mSampBuf.size()-1)));

                SampBufSkipZeroCross(size, SyncToneLinearSearchStep>>16);

                for (i=0; i<16; i++)
                {
                    uint32_t const sumSync = Correlate(&gModKC_1200Hz, Bit1SampStep, 1, 0);
                    if (sumSync < SyncToneThreshold2)
                        break;

                    SampBufSkipZeroCross(size, SyncToneLinearSearchStep>>16);
                }

                if ((pFile = new trData(trDATAFORMAT_KC, mSampleID)) == nullptr)
                {
                    trErrSet(trENOMEM);
                    goto trDataAnalyzer_ScanKC_err;
                }

                pFile->mSampleStart = mSampOffset;

                NextValidBlock = 1;
            }

            if ((pChunk = pFile->AddChunk()) == nullptr)
                goto trDataAnalyzer_ScanKC_err;
            ByteCount = 0;

            trLog((modState == trMODKC_STATE_SYNCSTART) ? trVerbose : trDebug, trT("ScanKC: Found synctone at offset %llu to %llu\n"), mSampOffset, mSampOffset+size);
            SampBufSkip(size);
            modState = trMODKC_STATE_SYNCSTOP;
            //break;

        case trMODKC_STATE_SYNCSTOP:

            if ((size = ScanRefWave(&gModKC_600Hz, 1, StopBitSampStep, StopBitLinearSearchStep, StopBitThreshold)) == 0)
                goto trDataAnalyzer_ScanKC_err;

            trLog(trDebug, trT("ScanKC: Found stop bit at offset %llu to "), mSampOffset);
            SampBufSkipZeroCross(size, StopBitLinearSearchStep>>16);
            trLog(trDebug, trT("%llu\n"), mSampOffset);
            modState = trMODKC_STATE_DATA;
            //break;

        case trMODKC_STATE_DATA:
            {
            unsigned byte = 0;
            int biterror = -1;
            uint64_t RetryOffset = 0;

            assert((Bit1SampSize*9+8 + StopBitSampSize+(StopBitSampSize>>3)+1) <= ((mSampBufWr - mSampBufRd) & (mSampBuf.size()-1)));

            //
            // Parse 8 data bits
            //
            for (i=0; i<8; i++)
            {
                if (biterror==-1)
                    RetryOffset = mSampOffset;

                uint32_t const sum1 = Correlate(&gModKC_1200Hz, Bit1SampStep, 1, 0);
                if (sum1 >= Bit1Threshold)
                {
                    byte |= 1<<i;
                    SampBufSkipZeroCross(Bit1SampSize, (Bit1SampSize>>3)+1);
                }
                else
                {
                    uint32_t const sum0 = Correlate(&gModKC_2400Hz, Bit0SampStep, 1, 0);
                    if (sum0 >= sum1)
                    {
                        SampBufSkipZeroCross(Bit0SampSize, (Bit0SampSize>>3)+1);

                        if (sum0 < Bit0Threshold)
                        {
                            if (biterror==-1) biterror=i;
                            trLog(trInf, trT("ScanKC: biterror at offset %llu\n"), RetryOffset);
                        }
                    }
                }
            }

            //
            // Save parsed byte
            //
            pChunk[ByteCount++] = (uint8_t)byte;

            //
            // Parse stop bit
            //
            uint32_t const sumStop = Correlate(&gModKC_600Hz, StopBitSampStep, 1, 0);

            SampBufSkipZeroCross(StopBitSampSize, (StopBitSampSize>>3)+1);

            if (SampBufPrefill(mSampOffset) != trEOK)
                goto trDataAnalyzer_ScanKC_err;

            if (sumStop < StopBitThreshold)
            {
                trLog(trInf, trT("ScanKC: stop biterror at offset %llu\n"), mSampOffset);

                if (biterror != -1)
                {
                    if ((NextValidBlock == 1) && (ByteCount < 7))
                    {
                        modState = trMODKC_STATE_SYNCSTART;
                        break;
                    }

                    // if biterror is in byte and stop bit, save partial chunk and resync
                    trLog(trInf, trT("ScanKC: offset %llu chunk %02X %02X? truncating at %d bytes\n"), mSampOffset, NextValidBlock, pFile->mData[pFile->mData.size()-130], ByteCount%130);

                    memset(pChunk + ByteCount, 0xCD, trDATAKC_CHUNKSIZE - ByteCount);
                    modState = trMODKC_STATE_SYNCCHUNK;
                }
            }

            //
            // 130 byte chunk complete
            //
            if (ByteCount == trDATAKC_CHUNKSIZE)
            {
                // - verify chunk CRC and number
                uint32_t crc = 0;
                for (i=1; i<=128; i++)
                    crc += (uint32_t)pChunk[i];

                if (((pChunk[0] != (NextValidBlock & 255)) && (pChunk[0] != 0xFFU)) ||
                    ((crc&255) != pChunk[129]) )
                {
                    trLog(trInf, trT("ScanKC: offset %llu chunk %02X %02X?"), mSampOffset, NextValidBlock, pChunk[0]);

                    if ((crc&255) != pChunk[129])
                        trLog(trInf, trT(" CRC error\n"));
                    else
                        trLog(trInf, trT("\n"));
                }
                else
                {
                    trLog(trVerbose, trT("ScanKC: %02X>\n"), pChunk[0]);
                }

                if ((pChunk[0] == 0xFFU) || (modState == trMODKC_STATE_SYNCSTART))
                {
                    // - Last chunk, save file

                    pFile->mSampleEnd = mSampOffset;

                    i = pFile->GetName(nullptr);
                    if ((unsigned)(i-1) < 15)
                        pFile->GetName(filename);
                    else
                        i=0;
                    filename[i] = 0;

                    trLog(trInf, trT("ScanKC: Extracted file %d '%s', 0x%X chunks, %u bytes, sample offset %llu to %llu\n"),
                        mFiles.size(), filename, pFile->mChunkCount, pFile->mData.size(),
                        pFile->mSampleStart, pFile->mSampleEnd);

					mFiles.emplace_back(pFile);
                    pFile = nullptr;
                    modState = trMODKC_STATE_SYNCSTART;
                }
                else
                {
                    NextValidBlock++;
                    modState = trMODKC_STATE_SYNCCHUNK;
                }
            }
            }
            break;
        default:
            break;
        }
    }

    trDataAnalyzer_ScanKC_err:
    delete pFile;
    return trELAST;
}

