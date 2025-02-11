//
// Simple IPS Patcher
// Apply IPS Patches to compatible ROM files
//
// v0.1 - 05/02/25

#include "ipspatch.h"
#include <string.h>

// File Operations
// Open a file from the filesystem
FILE *openFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("%s [%s]\n", "Error opening file:", filename);
        return NULL;
    }

    printf("%s [%s]\n", "Opened File:", filename);
    return file;
}

// close a file descriptor
void closeFile(FILE *fileDescriptor) {
  if (fileDescriptor) {
    printf("%s [0x%X]\n", "Closing file...", fileDescriptor);
    fclose(fileDescriptor);
  } else {
    printf("%s\n", "File Descriptor does not point to and open file. Ignoring.");
  }
}

// Utility Functions
// Validates patch file by reading and checking its IPS Header block.
int checkValidPatch(FILE *ipsDescriptor) {
  // read magic bytes from the ips patch
  uint8_t magic_bytes[5];
  // rewind descriptor
  fseek(ipsDescriptor, 0L, SEEK_SET);
  // read magic bytes
  fread(magic_bytes, IPS_MAGIC_SIZE, 1, ipsDescriptor);

  // compare header with MAGIC BLOCK
  if (memcmp(magic_bytes, IPS_MAGIC_TAG, IPS_MAGIC_SIZE) == 0) {
    // file is vaild
    printf("[SANITY CHECK] %s\n", "checkValidPatch(): Magic Bytes Match: Patch is VALID.");
    return 1;
  } else {
    printf("[SANITY CHECK] %s\n", "checkValidPatch(): Magic Bytes Mismatch: Patch is INVALID.");
    return 0;
  }
}

// Check block for the EOF Tag
int checkEof(uint8_t *mem_offset) {
  if (memcmp(mem_offset, IPS_END_TAG, IPS_END_SIZE) == 0) {
    printf("[SANITY CHECK]: %s\n", "checkEof(): Reached EOF of IPS Patch File.");
    return 1; // eof reached
  } else {
    return 0; // still more to read
  }
}

// MEMORY STRUCTURE FOR PATCHES
// Count patch items currently placed in the linked list
uint count(recordEntry *head) {
  uint c = 0;
  recordEntry *current = head;
  while (current) {
    c++; current = current->next;
  }
  return c;
}

// return the pointer to the last patch entry in the linked list
recordEntry *last(recordEntry *first) {
  recordEntry *current = first;
  while (current) {
    if (current->next == NULL) { return current;}
    current=current->next;
  }
  return NULL;
}

// allocate a new Patch Item from memory
recordEntry *new() {
  // allocate memory for a record
  recordEntry *temp = (recordEntry *)malloc(sizeof(struct IPS_PATCH_RECORD));
  // ok memory allocated, set up the initial item contents
  if (temp) {
    temp->next = NULL;
    temp->r = NULL;
    temp->patchValue = NULL;
    return temp;
  }
  // error during allocation, return NULL
  return NULL;
}

// append a new Patch Item to the linked list
void appendItem(recordEntry **first, recordEntry *newEntry) {
  // base case, insert first element
  if (*first == NULL) {
    *first = newEntry;
  } else if ((*first)->next == NULL) {
    // enqueue first item to a non-null head
    (*first)->next = newEntry;
  } else {
    // generic case, append after the last element
    last(*first)->next = newEntry;
  }
}

// -debug function-
// Dump the contents of the linked list to stdout
void dump(recordEntry *first) {
  recordEntry *current = first;
  while (current) {
    // is this entry rle encoded?
    if (current->patchValue == NULL) {
      printf("%s\n", "-------------------------------");
      printf("ROM OFFSET: 0x%X\tPATCH SIZE IN BYTES: 0x%X\tBYTE VALUE: %d\tRLE ENCODING: %s\n", LINEAR_24(current->r->offset), LINEAR_16(current->rle.length), current->rle.byte_val, "YES");
      printf("%s\n", "-------------------------------");
    } else {
      printf("ROM OFFSET: 0x%X\tPATCH SIZE IN BYTES: 0x%X\tRLE ENCODING: %s\n", LINEAR_24(current->r->offset), LINEAR_16(current->r->size), "NO");
      for (unsigned int i = 0; i < (LINEAR_16(current->r->size)); i++) {
        printf("0x%X ", current->patchValue[i]);
        if ((i % 16) == 0) {
          printf("\n");
        }
      }
      printf("%s\n", "-------------------------------");
    }
    current = current->next;
  }
}

// deallocate the patch structure
void destroy(recordEntry *head) {
  // no-op, no items in the linked list
  if (head == NULL) return;

  // deallocate current node
  if (head->next == NULL) {
    // deallocate
    if (head->r) free(head->r);
    if (head->patchValue) free (head->patchValue);
    return;
  } else destroy(head->next); // recursively advance until the last element
}

// PATCH FILE MANAGEMENT FUNCTIONS
// load patches from an IPS file descriptor
recordEntry *loadIpsPatch(FILE *patchFile) {
  // patch record entries
  recordEntry *patchHead = NULL;
  // pointer to the last entry in the patch list
  recordEntry *latestEntry = NULL;
  // record header
  recordHeader *patchRecord = NULL;

  // rewind descriptor: start from the beginning of the file
  fseek(patchFile, 0L, SEEK_SET);
  // seek past the IPS magic tag
  fseek(patchFile, IPS_MAGIC_SIZE, SEEK_CUR);

  // read first patch record header
  patchRecord = (recordHeader *)malloc(sizeof(union IPS_RECORD_HEADER));
  fread(patchRecord, sizeof(union IPS_RECORD_HEADER), 1, patchFile);

  // loop over patches, stop at EOF
  while (!checkEof(patchRecord->offset)) {
    // read & insert patch into the linked list
    latestEntry = new();
    if (latestEntry) {
      latestEntry->r = patchRecord;
    } else {
      if (patchRecord) free(patchRecord);
      destroy(patchHead);
      return NULL;
    }

    // Read Patch Record contents from file
    uint16_t size = LINEAR_16(latestEntry->r->size);
    // Patch Record RLE Encoded....
    if (size == 0) {
      // load data into this entry
      fread(&(latestEntry->rle.length), 2, 1, patchFile);
      // read rle byte
      fread(&(latestEntry->rle.byte_val), 1, 1, patchFile);
      // set patchvalue to null
      latestEntry->patchValue = NULL;
    } else { // patch record is BYTEPATCH
      // load data into the entry
      latestEntry->patchValue = (uint8_t *)malloc(LINEAR_16(latestEntry->r->size));
      if (latestEntry->patchValue) {
        fread(latestEntry->patchValue, LINEAR_16(latestEntry->r->size), 1, patchFile);
      } else {
        if (patchRecord) free(patchRecord);
        destroy(patchHead);
        return NULL;
      }
    }

    // append entry in the linked list
    appendItem(&patchHead, latestEntry);

    // move to the next record
    patchRecord = (recordHeader *)malloc(sizeof(union IPS_RECORD_HEADER));
    fread(patchRecord, sizeof(union IPS_RECORD_HEADER), 1, patchFile);
  }

  // return pointer to the patch list
  printf("[INFO]: File Contains %d unique byte patches\n", count(patchHead));
  return patchHead;
}

// checks whether a patch set is already applied to a ROM image
uint8_t patchApplied(FILE *romFile, recordEntry *patches) {
  // patch selector pointer
  recordEntry *currentPatch = patches;
  // rom bytes to be matched against patch
  uint8_t *rom_data = NULL;

  // verify that all patches are applied to the rom file
  for (unsigned int i=0; i<count(patches); i++) {
    // seek to the correct offset
    fseek(romFile, 0L, SEEK_SET); fseek(romFile, LINEAR_24(currentPatch->r->offset), SEEK_CUR);
    // compare patch bytes
    if (currentPatch->patchValue == NULL) {
      // patch is RLE, read data from rom
      rom_data = (uint8_t *)malloc(LINEAR_16(currentPatch->rle.length));
      if (!rom_data) {
        printf("%s\n", "Out of Memory.");
        return 0;
      }
      fread(rom_data, LINEAR_16(currentPatch->rle.length), 1, romFile);
      for (unsigned int j=0; j<(LINEAR_16(currentPatch->rle.length)); j++) {
        if (rom_data[j] != currentPatch->rle.byte_val) {
          printf("[VERIFY] Byte Mismatch @offset: 0x%X\n", LINEAR_24(currentPatch->r->offset));
          if (rom_data) free(rom_data); rom_data = NULL;
          return 0;
        }
      }
      free(rom_data); rom_data = NULL;
    } else {
      // compare bytes, read data from rom
      rom_data = (uint8_t *)malloc(LINEAR_16(currentPatch->r->size));
      if (!rom_data) {
        printf("%s\n", "Out of Memory.");
        return 0;
      }
      fread(rom_data, LINEAR_16(currentPatch->r->size), 1, romFile);
      if (memcmp(rom_data, currentPatch->patchValue, LINEAR_16(currentPatch->r->size)) != 0) {
        printf("[VERIFY] Byte Mismatch @offset: 0x%X\n", LINEAR_24(currentPatch->r->offset));
        if (rom_data) free(rom_data); rom_data = NULL;
        return 0;
      }
      free(rom_data); rom_data = NULL;
    }
    // advance next patch
    if (rom_data) free(rom_data);
    currentPatch = currentPatch->next;
  }

  printf("%s\n", "[VERIFY]: Patch Applied OK or ROM Already Patched.");
  return 1;
}

// duplicate files
FILE *dupeFile(FILE *source, unsigned char *destName) {
  FILE *destination = NULL;
  uint8_t chunk;

  // descriptor info
  struct stat fileStats;
  unsigned long headerSize = 0L;
  if (fstat(fileno(source), &fileStats) == 0) {
      headerSize = fileStats.st_size % 1024; // is the rom headered?
      if (headerSize != 0) { printf("%s\n", "Headered Rom Detected");}
  }

  if (destName) {
    destination = fopen((const char *)destName, "w");
    rewind(source);
    for (unsigned int i=0; i<fileStats.st_size; i++) {
      fread(&chunk, sizeof(uint8_t), 1, source);
      fwrite(&chunk, sizeof(uint8_t), 1, destination);
    }
    fflush(destination);
    rewind(destination);
    return destination;
  }
  else return NULL;
}

// apply patches to a rom file.
FILE *applyPatch(FILE *destRom, recordEntry *patches) {
  // pointer to the last unapplied patch data
  recordEntry *current = patches;

  // loop over available patches
  for (unsigned int i=0; i<count(patches); i++) {
    // update offset
    fseek(destRom, 0L, SEEK_SET); fseek(destRom, LINEAR_24(current->r->offset), SEEK_CUR);
    // apply patch
    if (current->patchValue == NULL) { // patch is RLE Encoded
      // patch bytes
      for (unsigned int r=0; r<(LINEAR_16(current->rle.length)); r++) {
        fwrite(&current->rle.byte_val, sizeof(uint8_t), 1, destRom);
      }
    } else {
      // patch bytes
      fwrite(current->patchValue, LINEAR_16(current->r->size), 1, destRom);
    }

    // advance patch pointer
    current = current->next;
  }

  // return patched rom descriptor
  fseek(destRom, 0L, SEEK_SET);
  printf("[PATCH] Complete.\n");
  return destRom;
}

// MAIN FUNCTION
int main(int argc, char **argv) {
  // local vars
  unsigned int opt;
  unsigned char *patchFileName = NULL;
  unsigned char *sourceRomFileName = NULL;
  unsigned char *destinationRomFileName = NULL;
  recordEntry *patchHead = NULL;
  // file descriptors
  FILE *srcRom = NULL, *dstRom = NULL, *patch = NULL;

  // parse command line options
  while ((opt = getopt(argc, argv, "i:d:p:?")) != -1) {
    switch (opt) {
      case 'i':
        sourceRomFileName = (unsigned char *)optarg;
        break;
      case 'd':
        destinationRomFileName = (unsigned char *)optarg;
        break;
      case 'p':
        patchFileName = (unsigned char *)optarg;
        break;
      case '?':
        if (optopt == 'i') {
          printf("[i option] : Input ROM File Name is a mandatory option: please specify a ROM File Name.\n");
        } else if (optopt == 'd') {
          printf("[d option] : Destination ROM File Name is missing: please specify a Destination ROM File Name.\n");
        } else if (optopt == 'p') {
          printf("[p option] : Missing IPS Patch File Name: Cannot patch anything without one.\n");
        } else {
          printf("Bad Option Detected: %c\n", optopt);
        }
        exit(-1);
    }
  }

  // sanity check
  if ((patchFileName == NULL) || (sourceRomFileName == NULL) || (destinationRomFileName == NULL)) {
    printf("Missing input parameters.\n");
    exit(-1);
  }
  printf("Patch File: [%s]\nSource ROM: [%s]\nDestination ROM: [%s]\n", patchFileName, sourceRomFileName, destinationRomFileName);

  // load IPS Patch
  patch = openFile((const char *)patchFileName);
  if (patch) {
    if (!checkValidPatch(patch)) {
      closeFile(patch);
      printf("[%s] Patch File Is Invalid.\n", patchFileName);
      exit(-1);
    } else {
      // load patch entries
      patchHead = loadIpsPatch(patch);
      if (!patchHead) {
        closeFile(patch);
        printf("[%s] Cannot Load Patches from IPS File.\n", patchFileName);
        exit(-1);
      }
    }
  } else {
    printf("Cannot Open File [%s]\n", patchFileName);
    exit(-1);
  }
  // load source rom
  srcRom = openFile((const char *)sourceRomFileName);
  if (!srcRom) {
    printf("Cannot Open File [%s]\n", sourceRomFileName);
    if (patch) closeFile(patch);
    exit(-1);
  }

  // check patch status
  if (patchApplied(srcRom, patchHead)) {
    printf("[PATCH VALIDATION] Source ROM Already Patched.\n");
  } else {
    // destination ROM file
    dstRom = dupeFile(srcRom, destinationRomFileName);
    if (!dstRom) {
      printf("Cannot Open File [%s]\n", destinationRomFileName);
      destroy(patchHead);
      if (patch) closeFile(patch);
      if (srcRom) closeFile(srcRom);
      exit(-1);
    }

    // apply patch to file
    dstRom = applyPatch(dstRom, patchHead);
    fflush(dstRom); fseek(dstRom, 0L, SEEK_SET); closeFile(dstRom);

    // verify patch
    dstRom = openFile((const char *)destinationRomFileName);
    if (!patchApplied(dstRom, patchHead)) {
      printf("[PATCH VALIDATION FAILED] Destination ROM Not Patched.\n");
    }
  }
  destroy(patchHead);

  // closeup and exit
  if (patch) closeFile(patch);
  if (srcRom) closeFile(srcRom);
  if (dstRom) closeFile(dstRom);
  exit(0);
}

