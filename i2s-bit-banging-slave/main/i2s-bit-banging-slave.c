#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2S_WS 12
#define I2S_BCK 11
#define I2S_DATA 10

#define NUM_BYTES_PAYLOAD 5
#define EXAMPLE_SAMPLE_RATE 16000

static const char *TAG = "i2s_slave";

// return true if the nth bit in the word is set, false otherwise.
bool nthBit(uint16_t word, size_t bitIndex)
{
  return (word & (1 << bitIndex)) != 0;
}

bool writing = false;
size_t payloadIndex = 0;
size_t bitIndex = 0;
uint16_t payloadBytes[NUM_BYTES_PAYLOAD] = {0x01, 0x02, 0x03, 0x04, 0x05};

IRAM_ATTR void WordSelectHandler(void *args)
{
  writing = true;
}

IRAM_ATTR void ClockHandler(void *args)
{
  if (writing)
  {
    gpio_set_level(I2S_DATA, nthBit(payloadBytes[payloadIndex], 15 - bitIndex));
    bitIndex++;

    if (bitIndex == 15)
    { 
      writing = false;
      bitIndex = 0;
      payloadIndex = (payloadIndex + 1) % NUM_BYTES_PAYLOAD;
    }
  }
}

void app_main(void)
{
  // Define GPIO configuration
  gpio_config_t data_gpio;
  data_gpio.intr_type = GPIO_INTR_DISABLE;
  data_gpio.mode = GPIO_MODE_OUTPUT;
  data_gpio.pin_bit_mask = (1ULL << I2S_DATA);
  data_gpio.pull_down_en = 1;
  data_gpio.pull_up_en = 0;
  gpio_config(&data_gpio);

  // Define GPIO configuration
  gpio_config_t clk_gpio;
  clk_gpio.intr_type = GPIO_INTR_POSEDGE;
  clk_gpio.mode = GPIO_MODE_INPUT;
  clk_gpio.pin_bit_mask = (1ULL << I2S_BCK);
  clk_gpio.pull_down_en = 1;
  clk_gpio.pull_up_en = 0;
  gpio_config(&clk_gpio);

  // Define interrupt configuration
  gpio_config_t ws_gpio;
  ws_gpio.intr_type = GPIO_INTR_NEGEDGE;
  ws_gpio.mode = GPIO_MODE_INPUT;
  ws_gpio.pin_bit_mask = (1ULL << I2S_WS);
  ws_gpio.pull_down_en = 0;
  ws_gpio.pull_up_en = 1;
  gpio_config(&ws_gpio);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(I2S_WS, WordSelectHandler, NULL);
  gpio_isr_handler_add(I2S_BCK, ClockHandler, NULL);

  for(;;) {
    vTaskDelay(1000);
  }
}
