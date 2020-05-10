#pragma once

#include "trSample.h"

/** File format IDs for trSample_raw class. */
enum trSAMPLE_RAW_FORMAT
{
    trSAMPLE_RAW_FORMAT_RAW = 0,    //!< Raw sample data
    trSAMPLE_RAW_FORMAT_WAV,        //!< WAV files
    trSAMPLE_RAW_FORMATCOINT        //!< Number of supported fileformats
};

class trSample_raw : public trSample
{
protected:
    uint8_t mFileFormat;   //!< Fileformat ID, see trSAMPLE_RAW_FORMAT
    trSection* mpSection;

    /** Scan buffer from current position until named WAV chunk is found.
        On success, the buffer offset remains on the first byte after the found chunk header.
        @param pName   4 character name of chunk
        @param pOffset Buffer offset to start scanning at, will be updated on exit.
        @return Chunksize in bytes, or -1 not found (last error is trENOTFOUND) or error
    */
    uint32_t FindWavChunk(const char* const pName, uint64_t* const pOffset);

    trERR InitWav();

public:
    //
    // - trSample interface implementation
    //
    virtual trERR Init(trBuffer* pBuf);
    virtual void Destroy();
    virtual uint8_t* Map(uint64_t offset, uint32_t size);
    virtual void Discard(uint8_t* pBuf);
    virtual trERR Commit(uint8_t* pBuf);
};
