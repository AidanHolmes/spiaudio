# spiaudio
SPI MHI library for VS1053 SPIder devices

## Setup
Wire the following together from SPIder to VS1053. The pin names refer to Adafruits VS1053 v2 board
3V3 -> 3V3 (can alternatively be 5V to VCC if 3.3 regulator built into board)
GND -> GND
IO16 -> MISO
IO17 -> CS
IO18 -> SCLK
IO19 -> MOSI
IO21 -> DREQ

RST pin on the VS1053 must be pulled up to V3.3. I used a 1k resistor to pull up. 
Other wiring required for audio out. See [Adafruit wiring guide](https://learn.adafruit.com/adafruit-vs1053-mp3-aac-ogg-midi-wav-play-and-record-codec-tutorial/simple-audio-player-wiring) for more info.
You may damage your audio equipment or VS1053 board if audio capacitors are not provided to line out or input lines [see Adafruit guidance](https://learn.adafruit.com/adafruit-vs1053-mp3-aac-ogg-midi-wav-play-and-record-codec-tutorial/audio-connections) for more info.

## Build
Requires fd2pragma 2.171 [Aminet Download](https://aminet.net/package/dev/misc/fd2pragma).

Install https://github.com/AidanHolmes/spiderdev into a sibling directory called lib

Like this:
```
Projects/
  lib/
    spiderdev
  spiaudio
```

Type **smake** in spiaudio directory to build Release and Debug targets

Requires 1.0.1 SPIder firmware
