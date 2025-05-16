#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/FreeRTOSConfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "board.h"
#include "driver/i2c.h"
#include "ssd1306.h"

static const char *TAG = "MINIMAL_MP3";
static sh1107_handle_t sh1107_dev = NULL;

// I2C Configuration
#define I2C_MASTER_SCL_IO           18      // GPIO number for I2C clock
#define I2C_MASTER_SDA_IO           8       // GPIO number for I2C data
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000  // I2C master clock frequency
#define SH1107_I2C_ADDRESS          0x3C    // SH1107 I2C address

// Waveform visualization settings
#define WAVEFORM_WIDTH 120  // Leave some margin on sides
#define WAVEFORM_HEIGHT 100 // Leave some margin top/bottom
#define WAVEFORM_X_OFFSET 4
#define WAVEFORM_Y_OFFSET 14
#define WAVEFORM_SAMPLE_SIZE 256  // Number of samples to collect before drawing
#define WAVEFORM_SCALE_FACTOR 2   // Scale factor for amplitude visualization

// Buffer for audio samples
static int16_t audio_buffer[WAVEFORM_SAMPLE_SIZE];
static size_t buffer_index = 0;

// Function declarations
static void init_i2c(void);
static void init_display(void);
static void draw_waveform(void);
static int audio_data_cb(audio_element_handle_t el, char *data, int len, TickType_t wait_time, void *ctx);
static int mp3_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx);

// High-rate MP3 audio (44100Hz stereo)
// This line declares an external array variable 'hr_mp3_start' that points to the start of an embedded MP3 file
// The asm directive maps it to a symbol created by the build system when embedding the MP3 file
// The symbol name indicates it's a 16-bit stereo MP3 file sampled at 44.1kHz
extern const uint8_t hr_mp3_start[] asm("_binary_music_16b_2c_44100hz_mp3_start");
extern const uint8_t hr_mp3_end[]   asm("_binary_music_16b_2c_44100hz_mp3_end");

static struct {
    int pos;
    const uint8_t *start;
    const uint8_t *end;
} file_marker = {
    .start = hr_mp3_start,
    .end = hr_mp3_end,
    .pos = 0,
};

int mp3_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int remaining = file_marker.end - file_marker.start - file_marker.pos;
    if (remaining <= 0) return AEL_IO_DONE;
    if (len > remaining) len = remaining;
    memcpy(buf, file_marker.start + file_marker.pos, len);
    file_marker.pos += len;
    return len;
}

// Function to draw the waveform
static void draw_waveform(void)
{
    // Clear the waveform area
    for (int y = WAVEFORM_Y_OFFSET; y < WAVEFORM_Y_OFFSET + WAVEFORM_HEIGHT; y++) {
        for (int x = WAVEFORM_X_OFFSET; x < WAVEFORM_X_OFFSET + WAVEFORM_WIDTH; x++) {
            sh1107_fill_point(sh1107_dev, x, y, 0);
        }
    }
    
    // Draw the waveform
    for (int i = 0; i < WAVEFORM_SAMPLE_SIZE; i++) {
        int x = WAVEFORM_X_OFFSET + (i * WAVEFORM_WIDTH) / WAVEFORM_SAMPLE_SIZE;
        int y = WAVEFORM_Y_OFFSET + (WAVEFORM_HEIGHT / 2) + 
                (audio_buffer[i] * WAVEFORM_HEIGHT / 2 / 32768 * WAVEFORM_SCALE_FACTOR);
        
        // Ensure y is within bounds
        if (y < WAVEFORM_Y_OFFSET) y = WAVEFORM_Y_OFFSET;
        if (y >= WAVEFORM_Y_OFFSET + WAVEFORM_HEIGHT) y = WAVEFORM_Y_OFFSET + WAVEFORM_HEIGHT - 1;
        
        sh1107_fill_point(sh1107_dev, x, y, 1);
    }
    
    // Refresh the display
    sh1107_refresh_gram(sh1107_dev);
}

// Callback function to process audio data for visualization
static int audio_data_cb(audio_element_handle_t el, char *data, int len, TickType_t wait_time, void *ctx)
{
    // Only process left channel (stereo data)
    int16_t *samples = (int16_t *)data;
    int num_samples = len / sizeof(int16_t) / 2;  // Divide by 2 for stereo
    
    for (int i = 0; i < num_samples && buffer_index < WAVEFORM_SAMPLE_SIZE; i++) {
        audio_buffer[buffer_index++] = samples[i * 2];  // Left channel
    }
    
    // When buffer is full, draw the waveform
    if (buffer_index >= WAVEFORM_SAMPLE_SIZE) {
        draw_waveform();
        buffer_index = 0;
    }
    
    return len;
}

static void init_i2c(void)
{
    ESP_LOGI(TAG, "Initializing I2C...");
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter configuration failed");
        return;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver installation failed");
        return;
    }
    
    ESP_LOGI(TAG, "I2C initialized successfully");
}

static void init_display(void)
{
    ESP_LOGI(TAG, "Initializing display...");
    
    // Try to create the display device
    sh1107_dev = sh1107_create(I2C_MASTER_NUM, SH1107_I2C_ADDRESS);
    if (sh1107_dev == NULL) {
        ESP_LOGE(TAG, "Failed to create display device");
        return;
    }
    
    // Clear the display with black
    esp_err_t ret = sh1107_clear_screen(sh1107_dev, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear screen");
        return;
    }
    
    // Display title
    sh1107_display_text(sh1107_dev, 0, "MP3 PLAYER");
    sh1107_display_text(sh1107_dev, 1, "WAVEFORM");
    
    // Draw waveform area border
    for (int x = WAVEFORM_X_OFFSET - 1; x <= WAVEFORM_X_OFFSET + WAVEFORM_WIDTH; x++) {
        sh1107_fill_point(sh1107_dev, x, WAVEFORM_Y_OFFSET - 1, 1);
        sh1107_fill_point(sh1107_dev, x, WAVEFORM_Y_OFFSET + WAVEFORM_HEIGHT, 1);
    }
    for (int y = WAVEFORM_Y_OFFSET - 1; y <= WAVEFORM_Y_OFFSET + WAVEFORM_HEIGHT; y++) {
        sh1107_fill_point(sh1107_dev, WAVEFORM_X_OFFSET - 1, y, 1);
        sh1107_fill_point(sh1107_dev, WAVEFORM_X_OFFSET + WAVEFORM_WIDTH, y, 1);
    }
    
    // Refresh the display
    ret = sh1107_refresh_gram(sh1107_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh display");
        return;
    }
    
    ESP_LOGI(TAG, "Display initialized successfully");
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Initialize I2C and display
    init_i2c();
    vTaskDelay(pdMS_TO_TICKS(100)); // Give I2C time to stabilize
    init_display();
    vTaskDelay(pdMS_TO_TICKS(100)); // Give display time to initialize

    ESP_LOGI(TAG, "[1] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[2] Create pipeline and elements");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    audio_element_handle_t mp3_decoder = mp3_decoder_init(&mp3_cfg);
    audio_element_set_read_cb(mp3_decoder, mp3_music_read_cb, NULL);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);
    
    // Register audio data callback for waveform visualization
    audio_element_set_write_cb(i2s_writer, audio_data_cb, NULL);

    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_writer, "i2s");

    const char *link_tag[2] = {"mp3", "i2s"};
    audio_pipeline_link(pipeline, link_tag, 2);

    ESP_LOGI(TAG, "[3] Start pipeline");
    audio_pipeline_run(pipeline);

    while (audio_element_get_state(i2s_writer) != AEL_STATE_FINISHED) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "[4] Stop pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, i2s_writer);
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(mp3_decoder);
    audio_element_deinit(i2s_writer);
}
