//
// Simple IPS Patcher
// Apply IPS Patches to compatible ROM files
//
// v0.1 - 05/02/25

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>

// IPS Patch Magic Bytes
// IPS Patch files start with a 5-bytes header containing the literal
// string "PATCH". It also ends with a 3-bytes footer containing the
// literal string "EOF"
#define IPS_MAGIC_TAG "PATCH"
#define IPS_END_TAG "EOF"
#define IPS_MAGIC_SIZE strlen(IPS_MAGIC_TAG)
#define IPS_END_SIZE strlen(IPS_END_TAG)

// Patch Record
// 5-bytes header
// - 3 bytes: Linear Offset of the patch
// - 2 bytes: Size of the patch in bytes
// after the header, the actual patch bytes are stored
//
// if the patch size is 0, then the patch is RLE Encoded
// we need to read 3 more bytes to get this kind of patch
// - 2 bytes: how many bytes to patch
// - 1 byte: patch value
union IPS_RECORD_HEADER {
  uint8_t bytes[5];
  struct {
    uint8_t offset[3];
    uint8_t size[2];
  };
};

// this structure holds a patch entry
// - the patch header
// - the patch bytes
// - pointer to the next patch
typedef union IPS_RECORD_HEADER recordHeader;
struct IPS_PATCH_RECORD {
  recordHeader *r;
  struct {
    uint8_t length[2];
    uint8_t byte_val;
  } rle;
  uint8_t *patchValue;
  struct IPS_PATCH_RECORD *next;
};
typedef struct IPS_PATCH_RECORD recordEntry;

// IPS uses linear addresses, we need to rearrange bytes because of endianness
// 16-bit values
#define LINEAR_16(byte16_array) \
  ((uint16_t)(byte16_array[0] << 8) & 0xFF00 ) | ((uint16_t) byte16_array[1] & 0x00FF)

// 24-bit values (map to uint32)
#define LINEAR_24(byte24_array) \
  ((uint32_t)(byte24_array[0] << 16) & 0x00FF0000 ) | ((uint32_t) (byte24_array[1] << 8) & 0x0000FF00) | ((uint32_t) (byte24_array[2] & 0x000000FF))

