## EMU TOOLS

Little utilities for my emulation needs.

### smd2bin

I am a somewhat avid retrogamer, and I absolutely love the Sega Genesis/Megadrive. 
This is a little tool I wrote some years ago to convert many of my cartridge dumps (SMD dumps, which were the norm some time ago) into the more common BIN format, which is more compatible with emulators nowadays.

The tool is fairly old but it works!

### Compile & install

    gcc -o /usr/local/bin/smd2bin smd_decode.c

### Usage

    smd2bin -c <filename>.smd -o <outfile>.bin


