#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "sdcard.h"

#define UART_NUM 1

void send_byte(uint8_t data) {
    uart_write_bytes(UART_NUM, &data, 1);
}

bool request_baud_change(uint8_t opcode) {
    //Send
    send_byte(opcode);

    //Read result
    uint8_t result = 0;
    uart_read_bytes(UART_NUM, &result, 1, 400 / portTICK_RATE_MS);

    return result == opcode;
}

bool request_page(uint32_t addr, uint8_t* buffer) {
    //Create request
    uint8_t payload[3];
    payload[0] = 0xFF;               // opcode
    payload[1] = (addr >> 8) & 0xFF; // address middle bit
    payload[2] = (addr >> 16) & 0xFF; // address high bit

    //Send
    uart_write_bytes(UART_NUM, payload, sizeof(payload));

    //Read
    int pageLen = uart_read_bytes(UART_NUM, buffer, 256, 400 / portTICK_RATE_MS);

    return pageLen == 256;
}

uint16_t request_status() {
    //Send request
    send_byte(0x70);

    //Read back the two status bytes
    uint16_t status = 0;
    int read = uart_read_bytes(UART_NUM, &status, 2, 40 / portTICK_RATE_MS);
    if (read != 2)
        printf("WARN: Failed to read status! Expected 2 bytes, got %i instead!\n", read);
    
    return status;
}

bool test_status() {
    //Send request
    send_byte(0x70);

    //Read back the two status bytes
    uint8_t payload[8];
    int read = uart_read_bytes(UART_NUM, &payload, sizeof(payload), 80 / portTICK_RATE_MS);
    if (read != 2)
        printf("WARN: Failed to read status! Expected 2 bytes, got %i instead!\n", read);
    
    //Get
    uint8_t status = (payload[1] >> 2) & 3;
    return status != 1;
}

bool request_unlock(uint32_t addr, uint64_t key) {
    //Create request payload
    uint8_t payload[12];
    payload[ 0] = 0xF5;                // opcode
    payload[ 1] = (addr >>  0) & 0xFF; // address, byte 1/3
    payload[ 2] = (addr >>  8) & 0xFF; // address, byte 2/3
    payload[ 3] = (addr >> 16) & 0xFF; // address, byte 3/3
    payload[ 4] = 0x07;                // length of key
    payload[ 5] = (key >>  0) & 0xFF;  // key, byte 1/7
    payload[ 6] = (key >>  8) & 0xFF;  // key, byte 2/7
    payload[ 7] = (key >> 16) & 0xFF;  // key, byte 3/7
    payload[ 8] = (key >> 24) & 0xFF;  // key, byte 4/7
    payload[ 9] = (key >> 32) & 0xFF;  // key, byte 5/7
    payload[10] = (key >> 40) & 0xFF;  // key, byte 6/7
    payload[11] = (key >> 48) & 0xFF;  // key, byte 7/7

    //Send request
    uart_write_bytes(UART_NUM, payload, sizeof(payload));

    return test_status();
}

void uart_resync() {
    //Send 16 NULL bytes spaced 40 ms apart
    for (int i = 0; i < 16; i++) {
        send_byte(0x00);
        vTaskDelay(40 / portTICK_PERIOD_MS);
    }
}

bool compare_buffers(uint8_t* a, uint8_t* b, size_t size) {
    for (int i = 0; i < size; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

void dump_region(uint32_t start, uint32_t end, FILE* mot, const char* name) {
    //Format filename
    char filename[64];
    sprintf(filename, "/sdcard/%08x.bin", start);

    //Open file
    FILE* output = fopen(filename, "wb");

    //Dump
    uint8_t bufferA[256];
    uint8_t bufferB[256];
    for (uint32_t i = start; i < end; i += 256) {
        //Log
        printf("Dumping region \"%s\" (0x%08x-0x%08x) at 0x%08x...\n", name, start, end, i);

        //Read twice and make sure the buffers match
        int attempt = 0;
        do {
            //Log if failed
            if (attempt++ != 0)
                printf("Buffers didn't match; Retrying... (attempt %i)\n", attempt);
            
            //Do the two reads
            while (!request_page(i, bufferA))
                printf("Read failed; Retrying...\n");
            while (!request_page(i, bufferB))
                printf("Read failed; Retrying...\n");
        } while (!compare_buffers(bufferA, bufferB, 256));

        //Dump to .mot file
        int mOffset = 0;
        for (int p = 0; p < 16; p++) {
            //Calculate checksum
            uint8_t checksum = 0x15 + (((i + mOffset) >> 0) & 0xFF) + (((i + mOffset) >> 8) & 0xFF) + (((i + mOffset) >> 16) & 0xFF) + (((i + mOffset) >> 24) & 0xFF);
            for (int o = 0; o < 16; o++)
                checksum += bufferA[mOffset + o];
            
            //Write
            fprintf(mot, "S315%08x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
                i + mOffset,
                bufferA[mOffset + 0],
                bufferA[mOffset + 1],
                bufferA[mOffset + 2],
                bufferA[mOffset + 3],
                bufferA[mOffset + 4],
                bufferA[mOffset + 5],
                bufferA[mOffset + 6],
                bufferA[mOffset + 7],
                bufferA[mOffset + 8],
                bufferA[mOffset + 9],
                bufferA[mOffset + 10],
                bufferA[mOffset + 11],
                bufferA[mOffset + 12],
                bufferA[mOffset + 13],
                bufferA[mOffset + 14],
                bufferA[mOffset + 15],
                checksum ^ 0xFF
            );
            mOffset += 16;
        }

        //Write to file
        fwrite(bufferA, 256, 1, output);
    }

    //Close file
    fclose(output);
}

void app_main(void)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, 512, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, 5, 4, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    //Open SD card
    if (!sdcard_open()) {
        printf("Failed to open SD card!\n");
        abort();
    }

    //Request the bootloader version
    int versionLen = 0;
    uint8_t version[64];
    do {
        uart_resync();
        send_byte(0xFB);
        
        versionLen = uart_read_bytes(UART_NUM, version, sizeof(version) - 1, 400 / portTICK_RATE_MS);
        version[versionLen] = 0;
        printf("Bootloader version: %s\n", version);
    } while (version[0] != 'V' || version[1] != 'E' || version[2] != 'R');

    //Request unlock
    if (request_unlock(0x0FFFDF, 0)) {
        printf("unlocked!\n");
    } else {
        printf("failed to unlock chip...\n");
        abort();
    }

    //Increase baud rate
    if (request_baud_change(0xb2)) {
        printf("baud rate changed successfully.\n");
        uart_config.baud_rate = 38400;
        uart_param_config(UART_NUM, &uart_config);
    } else {
        printf("failed to change baud rate!\n");
        abort();
    }

    //Read regions
    FILE* mot = fopen("/sdcard/fw.mot", "wb");
    dump_region(0x00F000, 0x00FFFF, mot, "block A");
    dump_region(0x0FF000, 0x0FFFFF, mot, "block 0");
    dump_region(0x0FE000, 0x0FEFFF, mot, "block 1");
    dump_region(0x0FC000, 0x0FDFFF, mot, "block 2");
    dump_region(0x0FA000, 0x0FBFFF, mot, "block 3");
    dump_region(0x0F8000, 0x0F9FFF, mot, "block 4");
    dump_region(0x0F0000, 0x0F7FFF, mot, "block 5");
    dump_region(0x0E0000, 0x0EFFFF, mot, "block 6");
    dump_region(0x0D0000, 0x0DFFFF, mot, "block 7");
    dump_region(0x0C0000, 0x0CFFFF, mot, "block 8");
    dump_region(0x0B0000, 0x0BFFFF, mot, "block 9");
    dump_region(0x0A0000, 0x0AFFFF, mot, "block 10");
    dump_region(0x090000, 0x09FFFF, mot, "block 11");
    dump_region(0x080000, 0x08FFFF, mot, "block 12");
    dump_region(0x0FF000, 0x0FFFFF, mot, "boot rom");
    fclose(mot);

    //Unmount SD card
    sdcard_close();

    printf("done.\n");
}
