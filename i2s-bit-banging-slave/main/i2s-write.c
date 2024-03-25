#include <stdio.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2S_WS 12
#define I2S_BCK 11
#define I2S_DATA 10

#define NUM_BYTES_PAYLOAD 5
#define EXAMPLE_SAMPLE_RATE (100000)

static const char err_reason[][30] = {"input param is invalid",
                                      "operation timeout"};
static const char *TAG = "i2s_slave";
static i2s_chan_handle_t tx_handle = NULL;

static esp_err_t i2s_slave_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_SLAVE);
    //chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer //todo
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_STD_SLOT_LEFT), //mode not making a difference I2S_SLOT_MODE_STEREO
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, //todo is this
            .bclk = I2S_BCK,
            .ws = I2S_WS,
            .dout = I2S_DATA,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false, //todo
                .ws_inv = false, // default left: ws is low
            },
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; //this is needed
    /* Before writing data, init the TX channel first */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));

    /* Before writing data, start the TX channel first */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    return ESP_OK;
}

static void i2s_write(void *args)
{
    esp_err_t ret = ESP_OK;
    size_t payloadIndex = 0;
    uint16_t payloadBytes[NUM_BYTES_PAYLOAD] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ESP_LOGI(TAG, "[WRITE] Write start");
    while (1)
    {
        /* Write data to master */
        ret = i2s_channel_write(tx_handle, payloadBytes, sizeof(payloadBytes), &payloadIndex, portMAX_DELAY);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[WRITE] i2s write failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]);
            abort();
        }
        else
        {
            ESP_LOGI(TAG, "[WRITE] i2s write successful, %d bytes are written.", payloadIndex);
        }
        //payloadIndex = (payloadIndex + 1) % NUM_BYTES_PAYLOAD;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    //vTaskDelete(NULL);
}

void app_main(void)
{
    
    printf("i2s write start\n-----------------------------\n");
    /* Initialize i2s peripheral */
    if (i2s_slave_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s driver init failed");
        abort();
    }
    else
    {
        ESP_LOGI(TAG, "i2s driver init success");
    }

    /* Write the data to the master node */
    xTaskCreate(i2s_write, "i2s_write", 4096, NULL, 5, NULL);
}