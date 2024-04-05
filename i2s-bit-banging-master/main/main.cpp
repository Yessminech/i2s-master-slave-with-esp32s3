#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <cstdint>

static constexpr const char *TAG = "i2s_master";

static constexpr gpio_num_t I2S_MASTER_WS = GPIO_NUM_21;
static constexpr gpio_num_t I2S_MASTER_CLK = GPIO_NUM_47;
static constexpr gpio_num_t I2S_DATA = GPIO_NUM_48;
static constexpr std::uint32_t SAMPLE_RATE = 100'000; // Max 15000
static constexpr std::size_t DMA_BUFFER_LENGTH = 1023;
static i2s_chan_handle_t rx_handle = nullptr;

typedef enum
{
  START = 1,
  READING = 2,
  DONE = 4,
  ERROR = 8
} ReadStatus;

struct Sample
{
  int16_t *buffer;
  std::size_t totalSize;
  EventGroupHandle_t flags;
  std::size_t cursor = 0;
};

static esp_err_t i2s_master_init()
{
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  // chan_cfg.auto_clear = true; //todo
  chan_cfg.dma_desc_num = 10;
  chan_cfg.dma_frame_num = DMA_BUFFER_LENGTH;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle));
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_16BIT,
          static_cast<i2s_slot_mode_t>(I2S_STD_SLOT_LEFT)),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = I2S_MASTER_CLK,
              .ws = I2S_MASTER_WS,
              .dout = I2S_GPIO_UNUSED,
              .din = I2S_DATA,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };

  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
  return ESP_OK;
}

IRAM_ATTR bool i2s_rx_recv(i2s_chan_handle_t rx_handle, i2s_event_data_t *event,
                           void *arg)
{
  // ESP_DRAM_LOGI("", "receive!");
  return false;
}

IRAM_ATTR bool i2s_rx_recv_ovf(i2s_chan_handle_t rx_handle,
                               i2s_event_data_t *event, void *arg)
{
  ESP_DRAM_LOGI("", "overflow!");
  return false;
}

/**
 * @brief Reads samples to a buffer and prints the first 150 samples from it,
 * then restarts the cycle.
 * @param arg: a pointer to Sample.
 */
void acquisitionTask(void *arg)
{
  // START I2S
  i2s_event_callbacks_t cbs = {
      .on_recv = i2s_rx_recv,
      .on_recv_q_ovf = i2s_rx_recv_ovf,
      .on_sent = nullptr,
      .on_send_q_ovf = nullptr,
  };
  esp_err_t ret = i2s_channel_register_event_callback(rx_handle, &cbs, arg);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Callbacks registration failed!");
  }

  ret = i2s_channel_enable(rx_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "I2S start failed!");
    vTaskDelay(portMAX_DELAY);
  }

  auto *sample = reinterpret_cast<Sample *>(arg);
  uint32_t timeoutMs = 100;

  for (;;)
  {
    std::size_t bytesToRead =
        std::min(sample->totalSize, sample->totalSize - sample->cursor);
    std::size_t bytesRead = 0;

    i2s_channel_read(rx_handle, sample->buffer + sample->cursor,
                     2 * bytesToRead, &bytesRead, timeoutMs);

    sample->cursor = sample->cursor + bytesRead / 2;

    if (sample->cursor >= sample->totalSize)
    {
      ESP_LOGI(TAG, "Reading complete!");
      for (std::size_t i = 0; i < 3; i++)
      {
        std::size_t offset = i * 10;
        ESP_LOGI(TAG,
                 "Sample %3d to %3d: %4d %4d %4d %4d %4d %4d "
                 "%4d %4d %4d %4d",
                 offset, offset + 10, sample->buffer[offset],
                 sample->buffer[offset + 1], sample->buffer[offset + 2],
                 sample->buffer[offset + 3], sample->buffer[offset + 4],
                 sample->buffer[offset + 5], sample->buffer[offset + 6],
                 sample->buffer[offset + 7], sample->buffer[offset + 8],
                 sample->buffer[offset + 9]);
      }

      // for (std::size_t i = 0; i < sample->totalSize; i = i + 10000)
      // {
      //   ESP_LOGI(TAG,
      //            "Sample %3d: %4d",
      //            i, sample->buffer[i] % 10);
      // }
      ESP_LOGI(TAG, "...");
      for (std::size_t i = sample->totalSize / 10 - 3;
           i < sample->totalSize / 10; i++)
      {
        std::size_t offset = i * 10;
        ESP_LOGI(TAG,
                 "Sample %3d to %3d: %4d %4d %4d %4d %4d %4d "
                 "%4d %4d %4d %4d",
                 offset, offset + 10, sample->buffer[offset],
                 sample->buffer[offset + 1], sample->buffer[offset + 2],
                 sample->buffer[offset + 3], sample->buffer[offset + 4],
                 sample->buffer[offset + 5], sample->buffer[offset + 6],
                 sample->buffer[offset + 7], sample->buffer[offset + 8],
                 sample->buffer[offset + 9]);
      }
      sample->cursor = 0;
    }
  }
}

extern "C" void app_main(void)
{
  printf("i2s read start\n-----------------------------\n");

  if (i2s_master_init() != ESP_OK)
  {
    ESP_LOGE(TAG, "i2s driver init failed");
    abort();
  }
  else
  {
    ESP_LOGI(TAG, "i2s driver init success");
  }

  auto *samples = static_cast<int16_t *>(
      heap_caps_calloc(1, 2 * SAMPLE_RATE, MALLOC_CAP_SPIRAM));
  memset(samples, 0, 2 * SAMPLE_RATE);
  Sample mySample = {
      .buffer = samples,
      .totalSize = SAMPLE_RATE,
      .flags = xEventGroupCreate(),
  };

  xTaskCreate(acquisitionTask, "AcquisitionTask",
              configMINIMAL_STACK_SIZE * 100, &mySample, 5, nullptr);

  for (;;)
  {
    vTaskDelay(portMAX_DELAY);
  }
}
