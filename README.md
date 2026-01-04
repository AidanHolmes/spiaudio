# SPIAudio
SPI MHI library for the VS1053 SPIder device. 

Do you want to play MP3, OGG or other compressed formats on your Amiga? Maybe it doesn't have a super fast CPU or you want to offload the work whilst getting on with other things. 
This library/device driver provides an MHI interface through the SPIder clockport. 

Includes standard VS1053B patches and ADMIX patch for audio passthrough.

## Setup
Wire the following together from SPIder to VS1053. The pin names refer to Adafruits VS1053 v2 board
```
3V3 -> 3V3 (can alternatively be 5V to VCC if 3.3 regulator built into board)
GND -> GND
IO16 -> MISO
IO17 -> CS
IO18 -> SCLK
IO19 -> MOSI
IO21 -> DREQ
IO29 -> RESET (optional, but if excluded then add 1k pull up to reset pin on VS1053)
```

RST pin on the VS1053 must be pulled up to V3.3 if RESET conneciton excluded.
Other wiring required for audio out. See [Adafruit wiring guide](https://learn.adafruit.com/adafruit-vs1053-mp3-aac-ogg-midi-wav-play-and-record-codec-tutorial/simple-audio-player-wiring) for more info.
You may damage your audio equipment or VS1053 board if audio capacitors are not provided to line out or input lines [see Adafruit guidance](https://learn.adafruit.com/adafruit-vs1053-mp3-aac-ogg-midi-wav-play-and-record-codec-tutorial/audio-connections) for more info.

## Using MHI library
Copy **mhispiaudio.library** to your **LIBS:MHI/** directory. If you don't have an MHI directory then create one to put the library into.

[AmigaAMP](https://www.amigaamp.de/download.shtml) and Hippoplayer (with the [Hippoplayer update](https://aminet.net/package/mus/play/hippoplayerupdate) from Aminet) both support MHI playback. 
Install these and point the players (via configuration) to the copied MHI library. 

VS1053 can play a lot more formats than these players support. The command line **audio-test** can be used to try different files out and these should play. Note that this test program should not
be used at the same time as the MHI library due to it closing resources that MHI wants. 

Sometimes the VS1053 can get into an unrecoverable state, which a softreset (performed by MHI and audio-test) cannot fix. The only solution at the moment is a powercycle to hard reset the VS1053.

Even though this offloads to a decoder, the system will still be busy transferring file data. You may notice slower performance for file IO when playing music. Stopping playback should restore
the system to normal.

## Known issues
Some clipping on the line in passthrough. 

## Build
Requires fd2pragma 2.171 [Aminet Download](https://aminet.net/package/dev/misc/fd2pragma).
Code is setup to build using SAS/C 6.5 and tools such as **oml**, **smake** and **splat** which come with SAS/C. 

Install https://github.com/AidanHolmes/spiderdev into a sibling directory called lib.

Like this:
```
Projects/
  lib/
    spiderdev
  spiaudio
```

Type **smake** in spiaudio directory to build Release and Debug targets

Requires 1.0.2 SPIder firmware
