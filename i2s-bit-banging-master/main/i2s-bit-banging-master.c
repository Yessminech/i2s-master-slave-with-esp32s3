#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define I2S_MASTER_WS 21
#define I2S_MASTER_CLK 47
#define I2S_DATA 48

const TickType_t DelayI2S = pdMS_TO_TICKS(1);
static const char *TAG = "i2s_master";

void i2s_master_gen(void *arg)
{
    bool ws = false;
    while (1)
    {
        gpio_set_level(I2S_MASTER_CLK, 1);
        gpio_set_level(I2S_MASTER_WS, ws);
        vTaskDelay(DelayI2S);
        for (int i = 0; i < 15; i++)
        {
            gpio_set_level(I2S_MASTER_CLK, 0);
            vTaskDelay(DelayI2S);
            gpio_set_level(I2S_MASTER_CLK, 1);
            vTaskDelay(DelayI2S);
        }
        gpio_set_level(I2S_MASTER_CLK, 0);
        vTaskDelay(DelayI2S);
        ws = !ws;
    }
}

void app_main(void)
{
    gpio_config_t master_conf;
    master_conf.intr_type = GPIO_INTR_DISABLE;
    master_conf.mode = GPIO_MODE_OUTPUT;
    master_conf.pin_bit_mask = (1ULL << I2S_MASTER_WS) | (1ULL << I2S_MASTER_CLK);
    master_conf.pull_down_en = 0;
    master_conf.pull_up_en = 0;
    gpio_config(&master_conf);

    // Start tasks
    xTaskCreate(i2s_master_gen, "bit_bang_i2s", 2048, NULL, 5, NULL);
}
