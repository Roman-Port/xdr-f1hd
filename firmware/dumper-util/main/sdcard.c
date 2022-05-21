#include "sdcard.h"

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

sdmmc_card_t *card;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();

int sdcard_open() {
    //Begin
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 15,
        .miso_io_num = 2,
        .sclk_io_num = 14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    //Init bus
    if (spi_bus_initialize(host.slot, &bus_cfg, host.slot) != ESP_OK)
        return false;

    //Prepare slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 13;
    slot_config.host_id = host.slot;

    //Mount FS
    if (esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card) != ESP_OK)
        return false;

    return true;
}

void sdcard_close() {
    //Close FS
    esp_vfs_fat_sdcard_unmount("/sdcard", card);

    //Free slot
    spi_bus_free(host.slot);
}