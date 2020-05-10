hctape - 8-Bit Homecomputer Datatape Extractor
==============================================

The repository contains:

* hctape - a program to extract 8-Bit computer datatapes
* libtr  - a library for reading raw audio files (WAV)


Contact
-------

Author: David Schalig aka mrhill/Icebird

WWW:   http://kc85.datahammer.de
Email: kc85 () datahammer.de.spam (replace () and remove .spam)


Introduction
------------

This tool allows to extract data from 8-bit homecomputer datatape recordings.
It takes a WAV sample of the cassette tape as input and batch-extracts all data.

To extract your tapes, first digitally record them to WAV samples. You can
use programs like Audacity, GoldWave, or Soundforge for this. For good results
choose a high quality sampling format, like 44.1 kHz, 16 bit, stereo.
Then feed the WAV sample into hctape to batch-extract all files.

Currently only the modulation format for KC85/HC900/KC87/Z9001 series
computers is supported. Eventually I will add support for Z1013 and KC
turboloader formats.

Usage
-----

hctape is a command line tool. See command line help (run without parameters)
for syntax.

How hctape works
----------------

hctape uses some special techniques to reconstruct even bad recordings.
To detect waveforms it uses correlation, this allows to extract data from
recordings with background noise. It is also jitter-resistant to some extend.

Todos
-----

I am planning to do the following improvements if I find the time:

- recording speed detection (some tapes cannot be extracted, because
  the basespeed differs from the internal reference speed)
- advanced jitter correction using automatic resync
- automatic fall back to other stereo channel on biterrors
- biterror correction using the packet CRC
- add Z1013 support
- add turboloader support

Data Modulation, Packet and File Formats
----------------------------------------

### KC85/HC900/KC87/Z9001 CAOS (Cassette Aided Operating System)

   #### Modulation format

   130 byte packets:
   - Synctone, N waves, 1200 Hz
   - Stopbit,  1 wave,  600 Hz
   - Byte,     8 waves, 0 = 2400 Hz, 1 = 1200 Hz
   - Stopbit,  1 wave,  600 Hz

   #### Packet format

   Files are stored in packets (chunks) of 130 bytes each:

   <Long Synctone><Packet 1> <Synctone><Packet 2> ... <Synctone><Packet N>

   - 1 byte    : Packet ID
   - 128 bytes : Data
   - 1 byte    : CRC, all 128 data bytes added up

   The first packet of a file has ID 0x01. Each subsequent packet will increase
   the ID by 1, wrapping at 0xFE. The last packet always has ID 0xFF. If a
   file's size is not multiple of 128, the last packet will be padded with 0
   bytes.

   #### File formats

   Packet 0x01 contains a file header, which is specific to the type of the 
   file.

   CAOS Machine Code program or memory dump
   BASIC program

   See CAOS system handbook for packet formats.

### Z1013

   tbd

### KC Turboloader

   tbd

Version History
---------------

1.0 Initial release from 2008
2.0 recovered lost source code, made it compile on Linux with CMake
