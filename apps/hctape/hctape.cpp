#include "tr/tr.h"
#include "tr/trBuffer_file.h"
#include <stdlib.h>
#include <strings.h>
#include <filesystem>
#include <string>
#include <sstream>

using namespace std;

#define PROGRAM trT("hctape")
#define VERSION trT("V2.0")
static const trChar* const gDataFormatNames[trDATAFORMATCOUNT] = { trDATAFORMATNAMES };
static const trChar* const gSampleTypeNames[trSAMPLETYPECOUNT] = { trSAMPLETYPENAMES };
static const trChar* const gSampleRawFormatNames[trSAMPLERAWFORMATCOUNT] = { trSAMPLERAWFORMATNAMES };

void syntax()
{
    trLog(trInf, trT("Syntax: ") PROGRAM trT(" [<options>] <sample.wav> [startoffset [endoffset]]\n\n"));
    trLog(trInf, trT("Batch-extract data from WAV samples. Supported data formats:\n  "));
    for (unsigned i=0; i<trDATAFORMATCOUNT-1; i++)
        trLog(trInf, trT("%s, "), gDataFormatNames[i]);
    trLog(trInf, trT("%s\n\n"), gDataFormatNames[trDATAFORMATCOUNT-1]);
    trLog(trInf, trT("Options:\n"));
    trLog(trInf, trT("  -d <dir> Output directory for extracted files, default not saving files\n"));
    trLog(trInf, trT("  -c <n> Scan channel n, default is 0 (left)\n"));
    trLog(trInf, trT("  -v Verbose\n"));
    trLog(trInf, trT("  -vv Very verbose\n"));
    trLog(trInf, trT("  -i Print only sample format information, do not scan for data\n"));
    trLog(trInf, trT("  -h Display this syntax help\n"));
}

enum HCTapeAction
{
    HCTapeAction_Help,
    HCTapeAction_Scan,
    HCTapeAction_Info,
    HCTapeAction_SyntaxError
};

struct HCTapeParam
{
    char*    mpFileWav;
	char*    mpOutDir;
    uint8_t  mVerbose;       // Verbose level 0,1,2
    uint8_t  mWavSection;    // !=0 if subsection to scan is valid
    unsigned mChannel;
    uint64_t mWavStart;      // Start offset of subsection
    uint64_t mWavLength;     // Length of subsection

    HCTapeParam()
    {
    	bzero(this, sizeof(HCTapeParam));
    }

    HCTapeAction Parse(unsigned argc, char** argv)
    {
        unsigned i, free=0;
        HCTapeAction action = HCTapeAction_Scan;
        int64_t val64;

        for (i=1; i<argc; i++)
        {
            char* pArg = argv[i];

            if (pArg[0] == '-')
            {
                switch (pArg[1] & 0xDF)
                {
                case 'H':
                    return HCTapeAction_Help;
                case 'C':
                    if (++i >= argc)
                    {
                        trLog(trInf, trT("Missing channel for parameter -c\n"));
                        return HCTapeAction_SyntaxError;
                    }
					val64 = atoll(argv[i]);
                    mChannel = (unsigned)val64;
                    break;
                case 'V':
                    mVerbose = ((pArg[2] & 0xDF) == 'V') ? 2 : 1;
                    break;
                case 'I':
                    action = HCTapeAction_Info;
                    break;
				case 'D':
                    if (++i >= argc)
                    {
                        trLog(trInf, trT("Missing directory name for parameter -d\n"));
                        return HCTapeAction_SyntaxError;
                    }
					mpOutDir = argv[i];
					break;
                default:
                    trLog(trInf, trT("Unknown option '-%c'\n"), pArg[1]);
                    return HCTapeAction_SyntaxError;
                }
            }
            else
            {
                switch (free++)
                {
                case 0: mpFileWav = pArg; break;
                case 1:
					mWavStart = atoll(pArg);
                    mWavSection = 1;
                    break;
                case 2:
					mWavLength = atoll(pArg); 
                    break;
                default:
                    trLog(trInf, trT("Surplus parameter '%s'\n"), pArg);
                    return HCTapeAction_SyntaxError;
                }
            }
        }

        if (!mpFileWav)
        {
            trLog(trInf, trT("Wav filename not specified\n"));
            return HCTapeAction_SyntaxError;
        }

        return action;
    }
};

struct HCTape
{
    HCTapeParam*    mpParam;
    trBuffer_file   mWavBuf;
    trSample_raw    mWavSample;
    trDataAnalyzer  mAnalyzer;

    trERR Init(HCTapeParam* pParam)
    {
        mpParam = pParam;

        if (mWavBuf.Open(mpParam->mpFileWav, trMAP_READONLY) != trEOK)
        {
            trLog(trErr, trT("HCTape::Init: Error %d opening '%s'\n"), trErrGet(), mpParam->mpFileWav);
            return trELAST;
        }

        if (mWavSample.Init(&mWavBuf) != trEOK)
        {
            trLog(trErr, trT("HCTape::Init: Error %d initializing WAV parser\n"), trErrGet());
            return trELAST;
        }

        return trEOK;
    }

    void Destroy()
    {
        mWavSample.Destroy();
		mWavBuf.Close();
    }

    trERR Info()
    {
        uint64_t min;
        unsigned sec;

        trLog(trInf, trT("File '%s':\n"), mpParam->mpFileWav);
        trLog(trInf, trT("Fileformat: WAV, Raw data format: %s\n"), gSampleRawFormatNames[mWavSample.mFormat]);        
        trLog(trInf, trT("Samplerate: %lu Hz, Channels: %u, Bits per sample: %d (%s)\n"),
            mWavSample.mRate, mWavSample.mChannels, mWavSample.mBits, gSampleTypeNames[mWavSample.mType]);

        trLog(trInf, trT("Length: %llu samples ("), mWavSample.mLen);
        min = mWavSample.mLen / mWavSample.mRate;
        sec = (unsigned)(min % 60);
        min = min / 60;
        if (min) trLog(trInf, trT("%llu min "), min);
        trLog(trInf, trT("%u sec)\n"), sec);

        return trEOK;
    }

    trERR Scan()
    {
        trERR err;

        if (mAnalyzer.SetSample(&mWavSample, 0) != trEOK)
        {
            trLog(trErr, trT("HCTape::Scan: Error %d on SetSample\n"), trErrGet());
            return trELAST;
        }

        if (mpParam->mWavSection)
        {
            if (mAnalyzer.SetSection(mpParam->mWavStart, mpParam->mWavLength) != trEOK)
            {
                trLog(trErr, trT("HCTape::Scan: Error %d on SetSection\n"), trErrGet());
                return trELAST;
            }
        }

        if (mpParam->mChannel)
        {
            if (mAnalyzer.SetChannel(mpParam->mChannel) != trEOK)
            {
                trLog(trErr, trT("HCTape::Scan: Error %d on SetChannel\n"), trErrGet());
                return trELAST;
            }
        }

        if ((err = mAnalyzer.Scan()) != trEOK)
        {
            if (err == trENOTFOUND)
            {
                trLog(trErr, trT("no data found\n"), trErrGet());
            }
            else if (err != trEEOF)
            {
                trLog(trErr, trT("HCTape::Scan: Error %d on mAnalyzer.Scan\n"), trErrGet());
                return trELAST;
            }
        }

        return trEOK;
    }

    void ValidateFileName(trChar* pName)
    {
        static const unsigned char lu[] = {
            '*','x',
            '\\','_',
            '/','_',
            '?','_',
            '<','[',
            '>',']',
            ':','_',
        };

        for(;;)
        {
            unsigned c = *pName;
            if (c==0)
                return;

            if (c>=127)
                c='_';
            else
                for(unsigned j=0; j<sizeof(lu); j+=2)
                {
                    if (lu[j] == c)
                    {
                        c = lu[j+1];
                        break;
                    }
                }
            *pName++ = c;
        }
    }

    trERR Save()
    {
		unsigned         filesCount = mAnalyzer.getFilesCount();
        filesystem::path path;
        unsigned         nonamecount = 0;
        trChar           name[256];

        if (mpParam->mpOutDir)
            path = mpParam->mpOutDir;

		for (unsigned fileIdx = 0; fileIdx < filesCount; fileIdx++)
        {
            trData* pFile = mAnalyzer.getFile(fileIdx);

            unsigned namelen = pFile->GetName(nullptr);
            if (namelen > 255)
            {
                trErrSet(trENOTSUP);
                trLog(trErr, trT("Error, filename too long\n"));
                goto HCTape_Save_err;
            }
            pFile->GetName(name);
            name[namelen] = 0;

            if (namelen == 0)
                namelen = sprintf(name, "%04X.dat", nonamecount++);
            else
                ValidateFileName(name);

            if (!path.empty())
            {
                filesystem::path filePath = path / name;

                unsigned retry = 0;
                for(;;)
                {
                    if (!filesystem::exists(filePath))
                        break;
                
                    if (++retry >= 10000) // unlikely to ever happen
                    {
                        trErrSet(trENOTSUP);
                        trLog(trErr, trT("Error, cannot state free filename\n"));
                        goto HCTape_Save_err;
                    }

                    filePath = path / name;
                    filePath += '.';
                    filePath += to_string(retry);
                }                

                trBuffer_file file;
                if (file.Open(filePath.c_str(), trMAP_WRITE) != trEOK)
                {
                    trLog(trErr, trT("HCTape::Save: error %d opening file %s\n"), trErrGet(), name);
                    return trELAST;
                }

                if (pFile->SaveFile(&file) != trEOK)
                {
                    trLog(trErr, trT("HCTape::Save: error %d\n saving file %s"), trErrGet(), name);
                    return trELAST;
                }
                file.Close();
            }
        }

        return trEOK;

        HCTape_Save_err:
        return trELAST;
    }
};

int main(int argc, char** argv)
{
    int          i;
    HCTapeAction action;
    HCTapeParam  param;
    HCTape       hctape;

    trLog(trInf, PROGRAM trT(" ") VERSION trT(" 8-Bit Homecomputer Datatape Extractor, (cc) 2008-2020 David Schalig\n"));
    trLog(trInf, trT("Contact: kc85 (at) datahammer.de http://kc85.datahammer.de/\n\n"));

    // parse commandline parameters
    action = param.Parse(argc, argv);

    switch(action)
    {
    case HCTapeAction_SyntaxError:
        trLog(trInf, trT("Syntax error\n"));
    case HCTapeAction_Help:
        syntax();
        goto main_exit;
    }

    trLogLevelEnable(trVerbose, param.mVerbose >= 1);
    trLogLevelEnable(trDebug, param.mVerbose >= 2);

    // Init HCTape
    if (hctape.Init(&param) != trEOK)
    {
        trLog(trErr, trT("Error %d on HCTape::Init\n"), trErrGet());
        goto error;
    }

    if (action == HCTapeAction_Info)
    {
        if (hctape.Info() != trEOK)
        {
            trLog(trErr, trT("Error %d on HCTape::Info\n"), trErrGet());
            goto error;
        }
        goto main_exit;
    }

    // Extract data
    hctape.Scan();//xxx ignore error
    /*
    if (hctape.Scan() != trEOK)
    {
        if (trErrGet() != trEEOF)
        {
            trLog(trErr, trT("Error %d on HCTape::Scan\n"), trErrGet());
            goto error;
        }
    }
    */

    // Save data
    if (hctape.Save() != trEOK)
    {
        trLog(trErr, trT("Error %d on HCTape::Save\n"), trErrGet());
        goto error;
    }

    main_exit:
    trErrSet(0);
    error:
    hctape.Destroy();
    return trErrGet();
}

