#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_types.h"
#include "xil_io.h"
#include "ff.h"

#include "aes.h"
#include "sha256.h"

extern char inbyte(void);  // UART input

#define AES_KEY_SIZE 16
#define IV_SIZE 16
#define BLOCK_FILE_FORMAT "block%d.dat"

FATFS fatfs;
FIL fil;
UINT bytes_written, bytes_read;

typedef struct {
    int index;
    char temperature[16];
    uint8_t encrypted_temp[16];
    char prev_hash[65];
    char curr_hash[65];
} Block;

uint8_t aes_key[AES_KEY_SIZE] = "aesexamplekey123";
uint8_t iv[IV_SIZE] = "initialvector123";

void aes_encrypt(uint8_t *input, uint8_t *output) {
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, aes_key, iv);
    memcpy(output, input, 16);
    AES_CBC_encrypt_buffer(&ctx, output, 16);
}

void compute_sha256_string(const uint8_t *data, size_t len, char output[65]) {
    BYTE hash[SHA256_BLOCK_SIZE];
    SHA256_CTX ctx;

    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);

    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}

void create_block(Block *block, const char *temperature, const char *prev_hash) {
    strncpy(block->temperature, temperature, 15);
    block->temperature[15] = '\0';
    memcpy(block->encrypted_temp, temperature, 16);
    aes_encrypt(block->encrypted_temp, block->encrypted_temp);
    strncpy(block->prev_hash, prev_hash, 64);
    block->prev_hash[64] = '\0';

    uint8_t to_hash[128];
    memcpy(to_hash, block->prev_hash, 64);
    memcpy(to_hash + 64, block->encrypted_temp, 16);

    compute_sha256_string(to_hash, 80, block->curr_hash);
}

void print_block(const Block *block) {
    xil_printf("\r\n======================================\r\n");
    xil_printf("            Block %d\r\n", block->index);
    xil_printf("--------------------------------------\r\n");
    xil_printf("Plain Temperature : %s\r\n", block->temperature);

    xil_printf("Encrypted Temp     : ");
    for (int i = 0; i < 16; i++) xil_printf("%02X", block->encrypted_temp[i]);
    xil_printf("\r\n");

    xil_printf("Previous Hash      :\r\n%s\r\n", block->prev_hash);
    xil_printf("Current Hash       :\r\n%s\r\n", block->curr_hash);
    xil_printf("======================================\r\n");
}

int write_block(const Block *block) {
    char filename[32];
    sprintf(filename, BLOCK_FILE_FORMAT, block->index);
    if (f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return -1;
    f_write(&fil, block, sizeof(Block), &bytes_written);
    f_close(&fil);
    return 0;
}

int read_block(int index, Block *block) {
    char filename[32];
    sprintf(filename, BLOCK_FILE_FORMAT, index);
    if (f_open(&fil, filename, FA_READ) != FR_OK)
        return -1;
    f_read(&fil, block, sizeof(Block), &bytes_read);
    f_close(&fil);
    return 0;
}

int validate_block(const Block *block) {
    char computed_hash[65];
    uint8_t to_hash[128];
    memcpy(to_hash, block->prev_hash, 64);
    memcpy(to_hash + 64, block->encrypted_temp, 16);
    compute_sha256_string(to_hash, 80, computed_hash);
    return strcmp(computed_hash, block->curr_hash) == 0;
}

int get_int_input(const char *prompt) {
    xil_printf("%s", prompt);
    char ch = inbyte();
    xil_printf("%c\r\n", ch);  // Echo
    return ch - '0';
}

void get_string_input(const char *prompt, char *buffer, int max_len) {
    xil_printf("%s", prompt);
    int i = 0;
    char ch;
    while (i < max_len - 1) {
        ch = inbyte();
        if (ch == '\r' || ch == '\n') break;
        buffer[i++] = ch;
        xil_printf("%c", ch);  // Echo
    }
    buffer[i] = '\0';
    xil_printf("\r\n");
}

int mount_sd_card() {
    return f_mount(&fatfs, "0:/", 1) == FR_OK;
}

int main() {
    init_platform();
    xil_printf("=== AES-CBC Blockchain on ZedBoard with SD Card ===\r\n");

    if (!mount_sd_card()) {
        xil_printf("❌ SD Card Mount Failed! Insert and format as FAT32.\r\n");
        return -1;
    }

    xil_printf("✅ SD Card Mounted.\r\n");

    int block_count = 0;

    while (1) {
        xil_printf("\r\n============== MENU ==============\r\n");
        xil_printf("1. Add Block\r\n");
        xil_printf("2. Validate Block\r\n");
        xil_printf("3. Exit\r\n");
        int choice = get_int_input("Enter choice: ");

        if (choice == 1) {
            char temp[16];
            get_string_input("Enter temperature (e.g. 25*C): ", temp, 16);

            Block new_block;
            char prev_hash[65] = "0000000000000000000000000000000000000000000000000000000000000000";

            if (block_count > 0) {
                Block prev_block;
                if (read_block(block_count - 1, &prev_block) == 0) {
                    strcpy(prev_hash, prev_block.curr_hash);
                }
            }

            create_block(&new_block, temp, prev_hash);
            new_block.index = block_count;

            if (write_block(&new_block) == 0) {
                xil_printf("✅ Block %d written to SD card.\r\n", block_count);
                print_block(&new_block);
                block_count++;
            } else {
                xil_printf("❌ Failed to write block.\r\n");
            }

        } else if (choice == 2) {
            int index = get_int_input("Enter block index to validate: ");

            Block blk;
            if (read_block(index, &blk) == 0) {
                print_block(&blk);
                if (validate_block(&blk))
                    xil_printf("✅ Block %d is VALID.\r\n", index);
                else
                    xil_printf("❌ Block %d is INVALID!\r\n", index);
            } else {
                xil_printf("❌ Block not found.\r\n");
            }

        } else if (choice == 3) {
            xil_printf("Exiting blockchain demo...\r\n");
            break;
        } else {
            xil_printf("❌ Invalid choice. Try again.\r\n");
        }
    }

    cleanup_platform();
    return 0;
}
