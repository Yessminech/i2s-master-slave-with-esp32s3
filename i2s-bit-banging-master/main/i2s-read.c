#include <stdio.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2S_MASTER_WS 21
#define I2S_MASTER_CLK 47
#define I2S_DATA 48
#define EXAMPLE_RECV_BUF_SIZE (2400)
#define EXAMPLE_SAMPLE_RATE (16000) // LRCK / WS: Left/right clock or word select clock. For non-PDM mode, its frequency is equal to the sample rate.
static const char err_reason[][30] = {"input param is invalid",
                                      "operation timeout"};
static const char *TAG = "i2s_master";
static i2s_chan_handle_t rx_handle = NULL;

static esp_err_t i2s_master_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer //todo
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_MASTER_CLK,
            .ws = I2S_MASTER_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = true,
                .ws_inv = false, // default left: ws is low
            },
        },
    };
    /* Before reading data, init the RX channel first */
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    /* Before reading data, start the RX channel first */
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    return ESP_OK;
}

static void i2s_read(void *args)
{
    int *slave_data = malloc(EXAMPLE_RECV_BUF_SIZE);
    if (!slave_data)
    {
        ESP_LOGE(TAG, "[READ] No memory for read data buffer");
        abort();
    }
    esp_err_t ret = ESP_OK;
    size_t bytes_read = 0;

    ESP_LOGI(TAG, "[READ] Read start");
    while (1)
    {
        memset(slave_data, 0, EXAMPLE_RECV_BUF_SIZE);
        /*Read sample data from slave*/
        ret = i2s_channel_read(rx_handle, slave_data, EXAMPLE_RECV_BUF_SIZE, &bytes_read, 1000);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[READ] i2s read failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]);
            abort();
        }

        // Log the received data
        ESP_LOGI(TAG, "[READ] Received Data: ");
        for (size_t i = 0; i < bytes_read / sizeof(uint16_t); i++)
        {
            ESP_LOGI(TAG, "0x%02X ", ((uint16_t *)slave_data)[i]);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    printf("i2s read start\n-----------------------------\n");
    /* Initialize i2s peripheral */
    if (i2s_master_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s driver init failed");
        abort();
    }
    else
    {
        ESP_LOGI(TAG, "i2s driver init success");
    }

    /* Read the data from the slave node */
    // xTaskCreate(i2s_read, "i2s_read", 2048, NULL, 5, NULL); // todo: Check freeRtos Stack size 2048 or 8192
}