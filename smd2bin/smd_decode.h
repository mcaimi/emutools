//
//  SMD (Super MagicDrive) ROM Format Decoder
//  Originally developed as a companion utility for Mednafen multisystem emulator
//
//  Decodes Packed ROM Data from an SMD Formatted ROM Cartridge Dump
//  Optionally Converts SMD Dumps to BIN (RAW) Dumps suitable to be run in Mednafen
//
//  17/02/2012 - Added support for SMD-to-BIN rom conversion
//
//

// SMD Header
struct SMD_HEADER {
    int interleaved_blocks_num;
    int is_split_rom;
    int binary_size;
    unsigned char magic_num[2]; // 8-bit values
};

typedef struct SMD_HEADER smd_header_t;

// BIN Header
struct BIN_HEADER {
    char *system_name;
    char *software_name;
    char *copyright_notice;
    char *regional_lockout;
};

typedef struct BIN_HEADER bin_header_t;

//  SMD is an interleaved format, the rom is composed of 16KB blocks
//  in which odd bytes are encoded in the beginning of the block (first half)
//  and even bytes are encoded in the end of the block (second half)
//
//  The header is 512 bytes long:
//  Byte    0x00:   Number of 16KB Blocks in the ROM image
//  Byte    0x02:   Split ROM magic number (0 == Standalone ROM or last ROM in a splitserie, 1 == Split ROM)
//  Byte    0x08:   Value 0xAA  -- SMD Header Magic Number
//  Byte    0x09:   Value 0xBB  -- SMD Header Magic Number
//  SMD Format Constants
#define SMD_HEADER_SIZE   0x200   // 512 Bytes
#define SMD_ROM_BLOCK_SIZE  0x4000  // 16 KBytes
#define SMD_BANK_MID_POINT  0x2000  // 8 KBytes
// Header Offsets
#define NUM_BLOCK_OFFSET    0x00
#define SPLIT_ROM_OFFSET    0x02
#define SMD_MAGIC_OFFSET    0x08
#define SMD_INTERLEAVE_STEP 0x02

// BIN (RAW) ROM Dump Header
// The BIN Format is simply a RAW byte dump of the content of the cartridge.
//
// Interesting Offset in BIN Header (see hexdump -C for reference)
// Byte     0x100:  System Name Tag
// Byte     0x110:  Copyright Notice
// Byte     0x120:  Software Title (DOMESTIC)
// Byte     0x150:  Software Title (OVERSEAS)
// Byte     0x1F0:  Software Country Lockdown Code
#define BIN_SYSTEM_NAME_OFFSET          0x100
#define BIN_SW_COPYRIGHT_NOTICE         0x110
#define BIN_SOFTWARE_TITLE_DOMESTIC     0x120
#define BIN_SOFTWARE_TITLE_OVERSEAS     0x150
#define BIN_SOFTWARE_LOCK_CODE          0x1F0

// sizes
#define SYSTEM_STR_LEN          0x10    // 16 bytes
#define COPYRIGHT_NOTICE_LEN    0x10    // 16 bytes
#define SWNAME_STR_LEN          0x30    // 48 bytes
#define SWNAME_STR_LEN_OVERSEAS 0x30    // 48 bytes
#define LOCKDOWN_CODE_LEN       0x03    // 3 bytes

// misc defines
#define AUTHOR  "m"

//
