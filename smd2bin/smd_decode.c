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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "smd_decode.h"

// Variables
char *filename = NULL;
char *output_filename = NULL;
FILE *SMD_ROM_FILE;
FILE *BIN_ROM_FILE;
int option;
smd_header_t header;
unsigned char *bin_data;

// pretty banner
void pretty_banner()
{
        printf("SMD Converter v0.1 -- Super MagicDrive ROM Dump to RAW BIN Dump Decoder Program\n");
        printf("%s %s %s\n", "Code By ", AUTHOR, ", SEGA ROCKS.");
        printf("\n");
}

// print program usage
int usage(char *prgname)
{
    printf("\n");
    printf("%s\n", "SMD_Convert");
    printf("%s", "Program Usage:\n");
    printf("\t%s %s", prgname, " -c filename.smd [-o <output bin romfile>]\n");
    printf(" ");
    exit(0);
}

// read the SMD Header from the ROM file.
smd_header_t read_smd_header_from_file(FILE *romfile)
{
        // placeholder 
        smd_header_t local_header;
        unsigned char *header_data = (unsigned char *)malloc(SMD_HEADER_SIZE*sizeof(unsigned char));
        if (header_data == NULL)
        {
            printf("%s\n", "Out Of Memory.");
            exit(-1);
        }
        // initialize memory areas
        memset(header_data, 0x00, (SMD_HEADER_SIZE*sizeof(unsigned char)));

        // read the 512-byte block from the beginning of the file
        fseek(romfile, 0, SEEK_SET);
        fread(header_data, 1, SMD_HEADER_SIZE, romfile);

        // decode fields
        local_header.interleaved_blocks_num = (int)(*(header_data + NUM_BLOCK_OFFSET));
        local_header.is_split_rom = (int)(*(header_data + SPLIT_ROM_OFFSET));
        local_header.binary_size = local_header.interleaved_blocks_num * SMD_ROM_BLOCK_SIZE;
        local_header.magic_num[0] = (unsigned char)(*(header_data + SMD_MAGIC_OFFSET));
        local_header.magic_num[1] = (unsigned char)(*(header_data + SMD_MAGIC_OFFSET + 1));

        // good, release resources..
        if (header_data != NULL )
            free(header_data);

        // return header 
        return (smd_header_t)local_header;
}

// check and decode SMD Header
int decode_smd_header(smd_header_t header)
{
        // test magic number number
        if ((header.magic_num[0] != 0xAA) || (header.magic_num[1] != 0xBB)) // header corrupted or not SMD format
        {
            printf ("%s\n", "|KO|---!> File is not in SMD format or SMD header corrupted.");
            return -1;
        }
        else
        {
            printf("\n");
            printf("%s\n", "ROM Information:");
            printf("\t%s %d\n", "Number of Interleaved 16K Data Banks: ", (int)header.interleaved_blocks_num);
            printf("\t%s %dKB\n", "Encoded Binary Size: ", (int)header.binary_size/1024);
            printf("\t%s %s\n", "Split ROM: ", (header.is_split_rom == 0) ? "no" : "yes" );
            printf("\n");
            return 0;
        }

        // ok (never reached)
        return 0;
}

// read ROM binary data and deinterleave data blocks
unsigned char *deinterleave_data_blocks(FILE *smd_file, smd_header_t smd_header)
{
    // local pointers
    // interleaved byte swap-counters
    int even_byte_counter, odd_byte_counter;
    // byte-buffer pointers
    int counter, inner_loop_counter, step, data_read;

    // allocate memory for the SMD-to-BIN unpack and conversion process
    unsigned char *binary_data = (unsigned char *)malloc(smd_header.binary_size);
    if (binary_data == NULL)
    {
        printf("%s\n", "Out Of Memory.");
        exit(-1);
    }
    memset(binary_data, 0x0, smd_header.binary_size);

    // allocate block data holder
    unsigned char *data_block = (unsigned char *)malloc(SMD_ROM_BLOCK_SIZE * sizeof(unsigned char));
    if (data_block == NULL)
    {
        if (binary_data != NULL)
            free(binary_data);

        printf("%s\n", "Out Of Memory.");
        exit(-1);
    }
    memset(data_block, 0x0, (SMD_ROM_BLOCK_SIZE * sizeof(unsigned char)));

    // begin decoding....
    printf("%s\n", "|OK|---> Beginning Data Decoding Process....");
    fseek(smd_file, SMD_HEADER_SIZE, SEEK_SET);
    step=0;
    for (counter=0; counter < smd_header.interleaved_blocks_num; counter++)
    {
        // read a data block
        data_read = fread(data_block, SMD_ROM_BLOCK_SIZE, 1, smd_file);

        #ifdef DEBUG
            printf("|INFO|----> %s%d (Data Read: %d Bytes)...\n", "ROM BANK #", counter, data_read*SMD_ROM_BLOCK_SIZE);
        #endif

        // reset byte counters
        even_byte_counter = 0;
        odd_byte_counter = 1;

        // deinterleave ROM
        for (inner_loop_counter=0; inner_loop_counter < SMD_ROM_BLOCK_SIZE; inner_loop_counter++)
        {
            if (inner_loop_counter < SMD_BANK_MID_POINT)
            {
                *(binary_data + step + odd_byte_counter) = (unsigned char)(*(data_block + inner_loop_counter));
                odd_byte_counter += SMD_INTERLEAVE_STEP;
            }
            else
            {
                *(binary_data + step + even_byte_counter) = (unsigned char)(*(data_block + inner_loop_counter));
                even_byte_counter += SMD_INTERLEAVE_STEP;
            }
        }

        // advance one block in the deinterleaved binary data buffer
        step += SMD_ROM_BLOCK_SIZE;
    }

    // free data buffers...
    if (data_block != NULL)
        free(data_block);

    // OK, return deinterleaved data
    printf("%s\n", "|OK|---> Decoding DONE.");
    printf("%s 0x%X (size: %dKB)\n", "|OK|---> Decoded data at address: ", (unsigned int)binary_data, (counter*SMD_ROM_BLOCK_SIZE)/1024);
    return (unsigned char *)binary_data;
}

// write BIN format ROM Dump file
int write_bin_rom_file(unsigned char *converted_data, FILE *outfile)
{
        printf("%s\n", "|BUSY|---> Writing converted ROM Image File (RAW BINary Format)...");
        fwrite(converted_data, header.binary_size, 1, outfile);

        // done
        printf("%s\n", "|OK|---> Conversion Complete.");
        return 0;
}

// read converted bin rom header
int parse_bin_rom_header(unsigned char *binary_data)
{
        // binary rom header data holder
        bin_header_t binary_rom_header;

        // initialize fields...
        binary_rom_header.system_name = (char *)malloc((SYSTEM_STR_LEN * sizeof(char)) + 1 );
        binary_rom_header.software_name = (char *)malloc((SWNAME_STR_LEN * sizeof(char)) + 1);
        binary_rom_header.copyright_notice = (char *)malloc((COPYRIGHT_NOTICE_LEN * sizeof(char)) + 1);
        binary_rom_header.regional_lockout = (char *)malloc((LOCKDOWN_CODE_LEN * sizeof(char)) + 1);
        memset(binary_rom_header.system_name, 0x00, SYSTEM_STR_LEN + 1);
        memset(binary_rom_header.software_name, 0x00, SWNAME_STR_LEN + 1);
        memset(binary_rom_header.copyright_notice, 0x00, COPYRIGHT_NOTICE_LEN + 1);
        memset(binary_rom_header.regional_lockout, 0x00, LOCKDOWN_CODE_LEN + 1);

        // sanity check
        if ((binary_rom_header.system_name == NULL) || (binary_rom_header.software_name == NULL) || (binary_rom_header.copyright_notice == NULL) || (binary_rom_header.regional_lockout == NULL))
        {
            printf("%s\n", "Out Of Memory.");
            return -1;
        }

        // read data
        memcpy(binary_rom_header.system_name, (unsigned char*)(binary_data + BIN_SYSTEM_NAME_OFFSET), SYSTEM_STR_LEN);
        memcpy(binary_rom_header.software_name, (unsigned char*)(binary_data + BIN_SOFTWARE_TITLE_DOMESTIC), SWNAME_STR_LEN);
        memcpy(binary_rom_header.copyright_notice, (unsigned char*)(binary_data + BIN_SW_COPYRIGHT_NOTICE), COPYRIGHT_NOTICE_LEN);
        memcpy(binary_rom_header.regional_lockout, (unsigned char*)(binary_data + BIN_SOFTWARE_LOCK_CODE), LOCKDOWN_CODE_LEN);

        // display info
        printf("\n");
        printf("%s\n", "|INFO|---> Decoded BIN ROM Image Header Contents:");
        printf("\t%s %s\n", "SYSTEM NAME: ", binary_rom_header.system_name);
        printf("\t%s %s\n", "SOFTWARE TITLE: ", binary_rom_header.software_name);
        printf("\t%s %s\n", "COPYRIGHT NOTICE: ", binary_rom_header.copyright_notice);
        printf("\t%s %s\n", "REGIONAL CODE: ", binary_rom_header.regional_lockout);

        // free resources
        free(binary_rom_header.system_name);
        free(binary_rom_header.software_name);
        free(binary_rom_header.copyright_notice);
        free(binary_rom_header.regional_lockout);

        // ok done
        return 0;
}

// Main function
#ifdef GNUC
void main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    // START!
    pretty_banner();

    // sanity check
    if (argc < 2) 
    {
        printf("%s", "|KO|---> Syntax Error\n");
        usage(argv[0]);
    }

    // parse command line
    while ((option = getopt(argc, argv, "c:o:")) != -1)
    {
        switch (option)
        {
            case 'c':
                filename = optarg;
                break;
            case 'o':
                output_filename = optarg;
                break;
            default:
                usage(argv[0]);
                break;
        }
    }

    // ok, option parsed.
    // begin action
    printf ("%s: %s\n", "|OK|---> Operating on ROM File", filename);
    SMD_ROM_FILE = fopen(filename, "r");
    if (SMD_ROM_FILE == NULL)
    {
        printf("%s (ERRNO: %d)\n", "|KO|---> main(): fopen() error! Cannot Open Specified file.", errno);
        exit(-1);
    }

    // read SMD ROM header
    header = read_smd_header_from_file(SMD_ROM_FILE);
    if (decode_smd_header(header) < 0)
    {
        // ACH, corrupted rom detected!!
        fclose(SMD_ROM_FILE);
        exit(-1);
    }

    // begin decoding SMD data...
    bin_data = deinterleave_data_blocks(SMD_ROM_FILE, header);

    // decode the BIN Header
    if (parse_bin_rom_header(bin_data) < 0)
    {
        printf("%s\n", "|KO|---> Aborting....");
        goto end;
    }

    // check if the user wants to convert the ROM image from SMD to BIN...
    if (output_filename != NULL)
    {
        printf("\n");
        printf("%s: %s\n", "|OK|---> Opening new ROM Image File", output_filename);

        // open output file
        BIN_ROM_FILE = fopen(output_filename, "w+");
        // sanity check
        if (BIN_ROM_FILE == NULL)
        {
            printf("%s (ERRNO: %d)\n", "|KO|---> main(): fopen() error! Cannot Open Specified file.", errno);
            if (bin_data != NULL)
                free(bin_data);
            fclose(SMD_ROM_FILE);
            //exit...
            exit(-1);
        }
        else
        {
            write_bin_rom_file(bin_data, BIN_ROM_FILE);
            fclose(BIN_ROM_FILE);
        }
    }
    else
    {
        printf("\n%s\n", "|NOTICE|---> No output filename specified and decode finished. Exiting...");
    }

end:
    // end
    if (bin_data != NULL)
        free(bin_data);

    // DONE!
    printf("\n%s\n", "BYE");
    fclose(SMD_ROM_FILE);
    exit(0);
}

// EOF
