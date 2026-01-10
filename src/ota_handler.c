#include <zephyr/kernel.h> 
#include <zephyr/logging/log.h> 
#include <zephyr/drivers/flash.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/storage/flash_map.h>
#include "ota_handler.h"

LOG_MODULE_REGISTER(ota_handler, LOG_LEVEL_INF);

static struct flash_img_context ctx;
static bool reboot_pending = false; 

void ota_start(uint32_t total_size) {
    LOG_INF("Iniciando OTA. Tamanho: %d bytes", total_size);

    flash_img_init(&ctx);
    reboot_pending = false;
}
int ota_write_chunk(uint8_t *data, size_t len) 
{
    int ret = flash_img_buffered_write(&ctx, data, len, false);
    if (ret < 0) LOG_ERR("Erro gravando flash: %d", ret);
    return ret;
}

void ota_finish(void) 
{
    flash_img_buffered_write(&ctx, NULL, 0, true);

    LOG_INF("Download completo. Verificando e Agendando Swap...");

    if (boot_request_upgrade(BOOT_UPGRADE_TEST) == 0) 
    {
        LOG_INF("Update agendado com sucesso!");
        reboot_pending = true;
    } else {
        LOG_ERR("Falha ao agendar update!");
    }
}

void ota_check_and_reboot(void) 
{
    if (reboot_pending) 
    {
        LOG_WRN("Reiniciando sistema para aplicar update...");
        k_msleep(500); 
        sys_reboot(SYS_REBOOT_COLD);
    }
}

