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

### IPSPatch

Yet another IPS patcher. I know that there are tons upon tons of different (and better) patchers out there... but I was bored and I wrote my own.

### Compile & install

    gcc -o /usr/local/bin/ips ipspatch.c

### Usage

    ips -i <unpatched_rom_file.smc> -d <patched_rom_file.smc> -p <ips_patch_file.ips>
