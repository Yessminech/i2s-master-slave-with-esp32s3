#include <stdio.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include <inttypes.h>

#define I2S_MASTER_WS 21
#define I2S_MASTER_CLK 47
#define I2S_DATA 48
#define DMABufferLength (2046)
#define EXAMPLE_SAMPLE_RATE (10000) // Max 15000

// static const char err_reason[][30] = {"input param is invalid", "operation timeout"};
static const char *TAG = "i2s_master";
static i2s_chan_handle_t rx_handle = NULL;

typedef enum
{
    START = 1,
    READING = 2,
    DONE = 4,
    ERROR = 8
    // todo: Add overflow Tag
} ReadStatus;

typedef struct
{
    int16_t *buffer;
    size_t totalSize;
    EventGroupHandle_t flags;
    size_t *cursor;
} Sample;

static esp_err_t i2s_master_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    chan_cfg.dma_desc_num = 8; // todo check if this is enough
    chan_cfg.dma_frame_num = DMABufferLength;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_STD_SLOT_LEFT),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_MASTER_CLK,
            .ws = I2S_MASTER_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA,
            .invert_flags = {
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

IRAM_ATTR bool i2s_rx_recv(i2s_chan_handle_t rx_handle,
                           i2s_event_data_t *event,
                           void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // xHigherPriorityTaskWoken must be initialised to pdFALSE.
    // SAMPLE CONFIGURATION
    Sample *mySample = (Sample *)arg;

    // DMA_RECV
    int16_t *buffer = (int16_t *)(event->data);
    size_t dmaBufferSize = event->size / 2;

    // Fill Sample
    while (*mySample->cursor < mySample->totalSize)
    {
        size_t read_length = (mySample->totalSize - *mySample->cursor > dmaBufferSize)
                                 ? dmaBufferSize
                                 : mySample->totalSize - *mySample->cursor;
        for (uint16_t i = 0; i < read_length; i++)
        {
            mySample->buffer[*mySample->cursor] = buffer[i];
            (*mySample->cursor)++; // todo global var?
        }
    }

    //  0x0063
    //  0x0000

    // Done, Send
    xEventGroupSetBitsFromISR(mySample->flags, DONE, &xHigherPriorityTaskWoken);
    return true;
}

IRAM_ATTR bool i2s_rx_recv_ovf(i2s_chan_handle_t rx_handle,
                               i2s_event_data_t *event,
                               void *arg)
{
    Sample *mySample = (Sample *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(mySample->flags, ERROR, &xHigherPriorityTaskWoken);
    return false;
}
void acquisitionTask(void *arg)
{

    // START I2S
    i2s_event_callbacks_t cbs = {
        .on_recv = i2s_rx_recv,
        .on_recv_q_ovf = i2s_rx_recv_ovf,
        .on_sent = NULL,
        .on_send_q_ovf = NULL,
    };
    esp_err_t ret = i2s_channel_register_event_callback(rx_handle, &cbs, arg); // todo arg or *arg ??
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

    ESP_LOGI(TAG, "Collecting %" PRId16 " samples...", EXAMPLE_SAMPLE_RATE);
    Sample *mySample = (Sample *)arg;
    for (;;)
    {
        // Wait for i2s_rx_recv to fire acquisition event
        const TickType_t acquisition_timeout = 100 / portTICK_PERIOD_MS;                                              // todo check this , (1.1) * 1000 ;
        EventBits_t bits = xEventGroupWaitBits(mySample->flags, DONE | ERROR, pdFALSE, pdFALSE, acquisition_timeout); // todo Check this timeout

        // if (!bits)
        // {
        //     ESP_LOGW(TAG, "Timeout collecting data.");
        // }
        // else
        // {
        if ((bits & DONE))
        {
            ESP_LOGI(TAG, "Task notified end of acquisition successfully.");
            for (size_t i = 0; i < 100; i++)
            {
                uint16_t value = ((uint16_t)mySample->buffer[i]);
                ESP_LOGI(TAG, "Sample %zu: 0x%04X", i, value);
            }
            xEventGroupClearBits(mySample->flags, DONE);
        }
        else if (bits & ERROR)
        {
            ESP_LOGE(TAG, "[READ] Error reading Data: Read overflow");
            xEventGroupClearBits(mySample->flags, ERROR);
        }
    }
}

void app_main(void)
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

    size_t cursor = 0;
    int16_t *samples = malloc(2 * EXAMPLE_SAMPLE_RATE);
    memset(samples, 0, EXAMPLE_SAMPLE_RATE);
    Sample mySample = {
        .buffer = samples,
        .totalSize = EXAMPLE_SAMPLE_RATE,
        .flags = xEventGroupCreate(),
        .cursor = &cursor,
    };

    xTaskCreate(acquisitionTask, "AcquisitionTask", configMINIMAL_STACK_SIZE * 80, &mySample, 5, NULL);

    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

//*mySample->cursor = 0;
// while (*mySample->cursor < mySample->totalSize)
// {
//     size_t samplesToRead = (mySample->totalSize - *mySample->cursor > DMABufferLength) ? DMABufferLength : mySample->totalSize - *mySample->cursor;
//     size_t bytesRead = 0;
//     esp_err_t ret = i2s_channel_read(rx_handle, mySample->buffer + *mySample->cursor, 2 * samplesToRead, &bytesRead, 1000); // 1 Sample = 2 Bytes

//     if (ret != ESP_OK || bytesRead != samplesToRead * 2)
//     {
//         ESP_LOGE(TAG, "[READ] i2s read failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]);
//         xEventGroupSetBitsFromISR(mySample->flags, ERROR, &xHigherPriorityTaskWoken);
//         return false;
//     }

//     *mySample->cursor += samplesToRead;
// }