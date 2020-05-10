#pragma once

#include "trdefs.h"
#include "trBuffer.h"
#include <vector>

enum trDATAFORMAT
{
    trDATAFORMAT_KC,
    trDATAFORMAT_Z1013,
    trDATAFORMATCOUNT
};

#define trDATAFORMATNAMES \
trT("KC"),\
trT("Z1013")

enum trDATAFILEFORMAT
{
    trDATAFILEFORMAT_KCC, //!< KC COM file
    trDATAFILEFORMAT_KCB, //!< KC BASIC file
	trDATAFILEFORMAT_OVR, //!< Overlay file
    trDATAFILEFORMAT_TXW, //!< Wordpro text file
    trDATAFILEFORMAT_TTT, //!< KC BASIC Data
};

#define trDATAFILEFORMATEXT \
	'K','C','C',\
	'K','C','B',\
	'O','V','R',\
	'T','X','W',\
	'T','T','T'
	
/** Container for a data file extracted from a sample. */
struct trData
{
    uint8_t    mFormat;        //!< Modulation format, see trDATAFORMAT
    uint8_t    mSampleID;      //!< Source sample ID, from trDataAnalyzer::Scan parameter sampleID
    uint8_t    mFileFormat;    //!< System specific fileformat, see trDATAFILEFORMAT
    uint32_t   mChunkCount;    //!< Number of data chunks
    uint32_t   mChunkSize;     //!< Byte size of a chunk
    uint64_t   mSampleStart;   //!< Start offset of data recording in sample
    uint64_t   mSampleEnd;     //!< End offset of data recording in sample
    std::vector<uint8_t> mData;//!< Data, concatenation of chunks

    /** Init data container. */
    trData(trDATAFORMAT const format, uint8_t const SampleID);

    /** Destroy data container. */
    ~trData();

    uint8_t* AddChunk();

    trERR SaveFile(trBuffer* const pBuf);

    /** Get file name for this data.
        The string will be written to the passed buffer without terminator.
        @param pName Pointer to buffer, or NULL to read string length
        @return String length in number of characters, or -1 if no name can be returned
    */
    unsigned GetName(trChar* pName);
};

/** Descriptor for a reference signal. */
struct trDataRefSignal
{
    const int8_t*  mpData;        //!< Pointer to sample data, signed 8 bit
    unsigned       mLength;       //!< Length of sample data in samples, must be power of 2
    uint32_t        mRate;         //!< Sample rate
    uint32_t        mHighestFreq;  //!< Highest frequency component

    /** Calculate auto-correlation sum.
        @param skew Correlation skew
        @return Correlation sum
    */
    int32_t Autocorrelate(unsigned const skew) const;
};

class trDataAnalyzer
{
protected:
	std::vector<trData*> mFiles;//!< Extracted file list

    std::vector<uint16_t> mSampBuf;/**< Circular sample cache.
                                     Buffer read position corresponds to current mSampBufOffset
                                     Size is always power of 2.
                                */
    uint32_t    mSampBufWr;     //!< Write position in sample buffer
    uint32_t    mSampBufRd;     //!< Read position in sample buffer

    uint64_t    mSampOffset;    //!< Scan offset in active section, corresponds to mSampBufRd
    uint64_t    mSampStart;     //!< Sample start offset of active section
    uint64_t    mSampEnd;       //!< Sample end offset of active section

    trSample*   mpSample;       //!< Active sample, can be NULL
    uint8_t     mSampleID;      //!< ID of active sample
    unsigned    mChannel;       //!< Active channel

    // - internal helpers

    /** Ensure mSampBuf to contain samples starting at specified offset.

        This call sets mSampBufOffset to offset, and advances mSampBufRd to
        the corresponding position, i.e. the start of the circular mSampBuf
        will be aligned to offset. Also this call ensures that mSampBuf contains
        at least mSampBuf.GetSize()/2 samples starting from offset.

        @param offset Start offset in underlying sample
    */
    trERR SampBufPrefill(uint64_t offset);

    /** Advance read position in sample buffer.
        mSampOffset and mSampBufRd will be updated, but sample buffer will not be refilled.
        @param offset Number of samples to skip
    */
    void SampBufSkip(uint32_t offset);

    /** Advance read position in sample buffer and find zero-crossing close to new sample position.
        mSampOffset and mSampBufRd will be updated, but sample buffer will not be refilled.
        If no zero crossing is found, the behaviour is identical to SampBufSkip().
        @param offset Number of samples to skip
        @param range Range in samples to search forward and backward for zero crossing
    */
    void SampBufSkipZeroCross(uint32_t offset, uint32_t range);

    /** Correlate circular sample buffer with reference signal.
        Circular sample buffer mSampBuf is assumed to be prefilled with
        enough samples starting at mSampBufRd. \a step indirectly controls
        sample rate normalization.
        @param pRefWave Pointer to reference wave descriptor
        @param step     Step to advance in circular buffer, 16.16 fixpoint
        @param count    Repeat count of reference signal
        @param skew     Additional skew to shift sample against reference signal, 16.16 fixpoint
        @retrun Absolute of Correlation sum, consisting of \a refsize summands
    */
    uint32_t Correlate(const trDataRefSignal* pRefWave, uint32_t step, unsigned count, uint32_t skew);

    /** Scan active section of sample for occurance of reference signal.

        On success mSampBufOffset returns the sample offset of the
        start of the found reference signal.

        @param pRefWave         Pointer to reference wave descriptor
        @param count            Repeat count of reference signal
        @param SampStep         16.16 fixpoint step in sample which corresponds to reference signal samplerate
        @param LinearSearchStep 16.16 fixpoint step in sample which for linear correlation search of reference signal
        @param LinearSearchThreshold Correlation sum threshold.
                                If count is larger than 1, the threshold must be mutliplied by count.

        @return If the reference signal was found, the return value is the size
                of the located signal in samples. Otherwise 0 is returned, and
                trErrGet() should be called to get the error code.
                if the signal was not found, the error code will be trENOTFOUND.
    */
    uint32_t ScanRefWave(const trDataRefSignal* pRefWave, unsigned count,
                      uint32_t const SampStep, uint32_t const LinearSearchStep, uint32_t const LinearSearchThreshold);

public:
    // - public API

    trDataAnalyzer();
    ~trDataAnalyzer();

    /** Set new active sample.
        @param pSample  Sample to set, must be initialized
        @param sampleID Application defined sample ID
        @retval trENOTSUP Sample format not supported
    */
    trERR SetSample(trSample* const pSample, uint8_t const sampleID);

    /** Set new active section in sample.
        If this function is not called, the entire sample will be used as the
        active section.
        A sample must have been set before calling this function.
        @param offset Sample start offset of section
        @param length Length of section in samples, can be 0 to scan until EOF
    */
    trERR SetSection(uint64_t offset, uint64_t length);

    /** Set channel to scan, default is 0.
        @param channel Channel
    */
    trERR SetChannel(unsigned channel);

    /** Scan sample for data.
    */
    trERR Scan();

    /** Scan sample for KC data.
    */
    trERR ScanKC();

	unsigned getFilesCount() const { return mFiles.size(); }
	trData* getFile(unsigned index) const { return mFiles.at(index); }
};

