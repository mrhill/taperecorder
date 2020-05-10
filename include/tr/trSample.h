#pragma once

#include "trdefs.h"
#include "trBuffer.h"

/** Sample types. */
enum trSAMPLETYPE
{
    trSAMPLETYPE_U8 = 0,
    trSAMPLETYPE_S8,
    trSAMPLETYPE_U16,
    trSAMPLETYPE_S16,
    trSAMPLETYPE_U24,
    trSAMPLETYPE_S24,
    trSAMPLETYPE_U32,
    trSAMPLETYPE_S32,
    trSAMPLETYPE_FLOAT,
    trSAMPLETYPE_DOUBLE,
    trSAMPLETYPE_UNKNOWN,
    trSAMPLETYPECOUNT,
};

#define trSAMPLETYPENAMES \
    trT("8 bit unsigned"),\
    trT("8 bit signed"),\
    trT("16 bit unsigned"),\
    trT("16 bit signed"),\
    trT("24 bit unsigned"),\
    trT("24 bit signed"),\
    trT("32 bit unsigned"),\
    trT("32 bit signed"),\
    trT("32 bit float"),\
    trT("64 bit float"),\
    trT("unknown")

#define trSAMPLETYPESIZES 1,1,2,2,3,3,4,4,4,8,0
extern const uint8_t trgSampleTypeSizes[trSAMPLETYPECOUNT];

/** Sample endianess. Has effect for non 8-bit samples. */
enum trSAMPLEENDIAN
{
    trSAMPLEENDIAN_LE = 0,  //!< Little endian
    trSAMPLEENDIAN_BE,      //!< Big endian
#if bbCPUE == bbCPUE_LE
    trSAMPLEENDIAN_NATIVE = trSAMPLEENDIAN_LE,  //!< Native endianess
#else
    trSAMPLEENDIAN_NATIVE = trSAMPLEENDIAN_BE,  //!< Native endianess
#endif
};


/** Sample raw data format.
    This is the buffer format returned by trSample::Map().
    trSample::Read supports a number of standard raw formats.
*/
enum trSAMPLERAWFORMAT
{
    trSAMPLERAWFORMAT_INTERLEAVE,   //!< Channels are interleaved, each channel consumes mBytes, stride between samples is mStep
    trSAMPLERAWFORMAT_CHUNKY,       //!< Channels are chunky, chunksize is mStep
    trSAMPLERAWFORMAT_SPECIAL,      /**< trSample implementation specific raw format.
                                         Implementations must override trSample::Read.
                                    */
    trSAMPLERAWFORMATCOUNT
};

#define trSAMPLERAWFORMATNAMES \
trT("interleaved"), \
trT("chunky"), \
trT("special")

/** Get sample type ID from number of bits per sample.
    @param bits Bits per sample
    @param sign True for signed samples
*/
trSAMPLETYPE trSampleBitsToType(unsigned bits, int sign);

/** Interface to a sample.
    An implementation should make the actual sample available
    in signed 32 bit per sample channel. If the sample file is
    in a different format, it must be converted.
*/
class trSample
{
public:
    trBuffer*   mpBuf;      //!< Data source
    uint64_t    mLen;       //!< Sample length in samples
    uint64_t    mStart;     //!< Sample start file offset in bytes
    uint64_t    mByteLen;   //!< Sample length in bytes
    uint64_t    mFrameLen;  //!< Sample length in frames
    uint32_t    mFrameSize; //!< Size of a frame for chunky format
    uint32_t    mRate;      //!< Sample rate, samples per second
    uint16_t    mChannels;  //!< Number of channels
    uint8_t     mBits;      //!< Sample bits per channel
    uint8_t     mType;      //!< Format of a sample, see trSAMPLETYPE
    uint8_t     mEndianess; //!< 0 native, 1 LE, 2 BE
    uint8_t     mFormat;    //!< Raw data format, see trSAMPLERAWFORMAT

public:
    //
    // - internal helpers
    //
    trSample();
    ~trSample();

    void UpdateLen();

    //
    // - Accessors
    //

    void SetType(trSAMPLETYPE const type);
    void SetChannels(unsigned const channels);
    void SetRawFormat(trSAMPLERAWFORMAT const format);
    inline void SetEndianess(trSAMPLEENDIAN const endianess) { mEndianess = endianess; }
    inline void SetRate(uint32_t const rate) { mRate = rate; }

    //
    // - Interface
    //

    /** Init sample on buffer.
        Attaches buffer to sample, and initializes trSample attributes
        after scanning sample header, etc.
        @param pBuf Data buffer
    */
    virtual trERR Init(trBuffer* pBuf);

    /** Destroy sample and detach from buffer. */
    virtual void Destroy();

    /** Read raw sample data for current channel in specified sample format.
        The returned samples will be scaled in amplitude to fit destination format
        @param channel Channel to read, must be valid
        @param pBuf    Pointer to buffer to return data
        @param offset  Offset in samples
        @param size    Number of sample to read
        @param dstType Target sample format
    */
    virtual trERR ReadChannel(unsigned const channel, void* pBuf, uint64_t offset, uint32_t size, trSAMPLETYPE dstType);

    /** Map sample section in native raw format.
        If a buffer was returned, it must be released with Discard() or Commit() later.
        @param offset Sample offset in bytes
        @param size   Number of bytes to read.
        @return Pointer to data, or NULL on error
    */
    virtual uint8_t* Map(uint64_t offset, uint32_t size) = 0;

    /** Discard mapped sample section.
        The default implementation does nothing.
        @param pBuf Pointer to data as returned from Map(), can be NULL for no action
    */
    virtual void Discard(uint8_t* pBuf);

    /** Commit modified mapped sample section.
        The default implementation does nothing and returns trENOTSUP error.
        @param pBuf Pointer to data as returned from Map()
    */
    virtual trERR Commit(uint8_t* pBuf);
};
