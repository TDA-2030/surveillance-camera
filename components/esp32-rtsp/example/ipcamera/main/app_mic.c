
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_system.h"
#include "driver/i2s.h"
#include "esp_log.h"

static const char *TAG = "mic";

#define BOARD_DMIC_I2S_SCK 26
#define BOARD_DMIC_I2S_WS 32
#define BOARD_DMIC_I2S_SDO 33

typedef struct {
    i2s_port_t port;
    TaskHandle_t task_hdl;
    RingbufHandle_t ringbuf;
} mic_data_t;


void mic_get_data(void *handle, uint8_t *in_buffer, size_t size, size_t *bytes_read)
{
    mic_data_t *mic = (mic_data_t *)handle;
    i2s_read(mic->port, in_buffer, size, bytes_read, portMAX_DELAY);

    /**
     * The serial data is in slave mode I2S format, which has 24‐bit depth in a 32 bit word. In a
     * stereo frame there are 64 SCK cycles, or 32 SCK cycles per data‐word. When L/R=0, the output
     * data in the left channel, while L/R=Vdd, data in the right channel.
     *
     * The default data format is I2S, MSB‐first. In this format, the MSB of each word is delayed by
     * one SCK cycle from the start of each half‐frame.
     */
    uint32_t *buffer = (uint32_t *)in_buffer;
    int len = *bytes_read / 4 / 4;
    for (int x = 0; x < len; x++) {
        int s1 = ((buffer[x * 4] << 1) + 0) & 0xffff0000;
        int s2 = ((buffer[x * 4 + 2] << 1) + 0) >> 16;
        buffer[x] = s1 | s2;
    }

    *bytes_read /= 4;
}

static void audio_process(void *args)
{
    mic_data_t *mic = (mic_data_t *)args;
    while (1) {
        // mic_get_data(mic->port, );
        // BaseType_t done = xRingbufferSend(mic->ringbuf, (void *)data, size, (portTickType)portMAX_DELAY);
        // if (done) {
        //     return size;

        // }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void mic_get(void *handle)
{
    mic_data_t *mic = (mic_data_t *)handle;
    size_t item_size;
    uint32_t data = (uint8_t *)xRingbufferReceive(mic->ringbuf, &item_size, (portTickType)portMAX_DELAY);
    if (item_size != 0) {

    }

}

void mic_return(mic_data_t *handle)
{
    mic_data_t *mic = (mic_data_t *)handle;
    // vRingbufferReturnItem(mic->ringbuf, (void *)data);
}

void *mic_init(i2s_port_t i2s_port, int rate)
{
    mic_data_t *mic = calloc(1, sizeof(mic_data_t));
    if (NULL == mic) {
        ESP_LOGE(TAG, "malloc for mic failed");
        return NULL;
    }

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,           // the mode must be set according to DSP configuration
        .sample_rate = rate,                            // must be the same as DSP configuration
        .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,    // must be the same as DSP configuration
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,    // must be the same as DSP configuration
        .communication_format = I2S_COMM_FORMAT_I2S,
        .dma_buf_count = 3,
        .dma_buf_len = 1024,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = BOARD_DMIC_I2S_SCK,  // IIS_SCLK
        .ws_io_num = BOARD_DMIC_I2S_WS,    // IIS_LCLK
        .data_out_num = -1,                // IIS_DSIN
        .data_in_num = BOARD_DMIC_I2S_SDO, // IIS_DOUT
        .mck_io_num = -1,
    };
    i2s_driver_install(i2s_port, &i2s_config, 0, NULL);
    i2s_set_pin(i2s_port, &pin_config);
    i2s_zero_dma_buffer(i2s_port);
    mic->port = i2s_port;

    mic->ringbuf = xRingbufferCreate(8 * 1024, RINGBUF_TYPE_BYTEBUF);
    if (mic->ringbuf == NULL) {
        ESP_LOGE(TAG, "create audio ringbuf failed");
        return NULL;
    }

    BaseType_t xReturned = xTaskCreate(audio_process, "audio_proc", 1024, (void *) mic, configMAX_PRIORITIES / 2, &mic->task_hdl);
    if (pdPASS != xReturned) {
        ESP_LOGE(TAG, "create audio task failed");
    }
    return (void *)mic;
}

void mic_deinit(void *handle)
{
    mic_data_t *mic = (mic_data_t *)handle;
    if (mic->task_hdl) {
        vTaskDelete(mic->task_hdl);
    }

    if (mic->ringbuf) {
        vRingbufferDelete(mic->ringbuf);
    }

    i2s_zero_dma_buffer(mic->port);
    i2s_driver_uninstall(mic->port);
    free(mic);
}
