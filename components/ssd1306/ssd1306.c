#include <string.h>
#include "ssd1306.h"
#include "esp_log.h"

static const char *TAG = "SH1107";

#define SH1107_WIDTH 128
#define SH1107_HEIGHT 128
#define SH1107_PAGES (SH1107_HEIGHT / 8)

#define SH1107_COMMAND 0x00
#define SH1107_DATA 0x40

// SH1107 commands
#define SH1107_DISPLAY_OFF 0xAE
#define SH1107_DISPLAY_ON 0xAF
#define SH1107_SET_PAGE_ADDR 0xB0
#define SH1107_SET_COLUMN_LOW 0x00
#define SH1107_SET_COLUMN_HIGH 0x10
#define SH1107_SET_START_LINE 0x40
#define SH1107_SET_CONTRAST 0x81
#define SH1107_SEG_REMAP 0xA0
#define SH1107_COM_REMAP 0xC0
#define SH1107_DISPLAY_NORMAL 0xA6
#define SH1107_DISPLAY_INVERSE 0xA7
#define SH1107_DISPLAY_ALL_ON 0xA5
#define SH1107_DISPLAY_RAM 0xA4
#define SH1107_SET_VCOM_DESELECT 0xDB
#define SH1107_SET_DISPLAY_CLOCK 0xD5
#define SH1107_SET_PRECHARGE 0xD9
#define SH1107_SET_CHARGE_PUMP 0x8D

// Font size
#define FONT_WIDTH 6
#define FONT_HEIGHT 8

// Basic 6x8 font data (simplified for readability)
static const uint8_t font_6x8[] = {
    // Space
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // !
    0x00, 0x00, 0x5F, 0x00, 0x00, 0x00,
    // A-Z
    0x00, 0x7C, 0x12, 0x12, 0x7C, 0x00,  // A
    0x00, 0x7F, 0x49, 0x49, 0x36, 0x00,  // B
    0x00, 0x3E, 0x41, 0x41, 0x22, 0x00,  // C
    0x00, 0x7F, 0x41, 0x41, 0x3E, 0x00,  // D
    0x00, 0x7F, 0x49, 0x49, 0x41, 0x00,  // E
    0x00, 0x7F, 0x09, 0x09, 0x01, 0x00,  // F
    0x00, 0x3E, 0x41, 0x49, 0x7A, 0x00,  // G
    0x00, 0x7F, 0x08, 0x08, 0x7F, 0x00,  // H
    0x00, 0x41, 0x7F, 0x41, 0x00, 0x00,  // I
    0x00, 0x20, 0x40, 0x41, 0x3F, 0x00,  // J
    0x00, 0x7F, 0x08, 0x14, 0x63, 0x00,  // K
    0x00, 0x7F, 0x40, 0x40, 0x40, 0x00,  // L
    0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F,  // M
    0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F,  // N
    0x00, 0x3E, 0x41, 0x41, 0x3E, 0x00,  // O
    0x00, 0x7F, 0x09, 0x09, 0x06, 0x00,  // P
    0x00, 0x3E, 0x41, 0x51, 0x3E, 0x00,  // Q
    0x00, 0x7F, 0x09, 0x19, 0x66, 0x00,  // R
    0x00, 0x26, 0x49, 0x49, 0x32, 0x00,  // S
    0x00, 0x01, 0x7F, 0x01, 0x00, 0x00,  // T
    0x00, 0x3F, 0x40, 0x40, 0x3F, 0x00,  // U
    0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F,  // V
    0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F,  // W
    0x00, 0x63, 0x14, 0x08, 0x14, 0x63,  // X
    0x00, 0x07, 0x08, 0x70, 0x08, 0x07,  // Y
    0x00, 0x61, 0x51, 0x49, 0x45, 0x43,  // Z
    // 0-9
    0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E,  // 0
    0x00, 0x00, 0x42, 0x7F, 0x40, 0x00,  // 1
    0x00, 0x42, 0x61, 0x51, 0x49, 0x46,  // 2
    0x00, 0x21, 0x41, 0x45, 0x4B, 0x31,  // 3
    0x00, 0x18, 0x14, 0x12, 0x7F, 0x10,  // 4
    0x00, 0x27, 0x45, 0x45, 0x45, 0x39,  // 5
    0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30,  // 6
    0x00, 0x01, 0x71, 0x09, 0x05, 0x03,  // 7
    0x00, 0x36, 0x49, 0x49, 0x49, 0x36,  // 8
    0x00, 0x06, 0x49, 0x49, 0x29, 0x1E,  // 9
};

typedef struct {
    i2c_port_t i2c_num;
    uint8_t address;
    uint8_t buffer[SH1107_WIDTH * SH1107_PAGES];
} sh1107_dev_t;

static esp_err_t sh1107_write_cmd(sh1107_handle_t dev, uint8_t cmd)
{
    sh1107_dev_t* sh1107 = (sh1107_dev_t*)dev;
    uint8_t write_buf[2] = {SH1107_COMMAND, cmd};
    
    i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
    i2c_master_start(i2c_cmd);
    i2c_master_write_byte(i2c_cmd, (sh1107->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(i2c_cmd, write_buf, 2, true);
    i2c_master_stop(i2c_cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(sh1107->i2c_num, i2c_cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(i2c_cmd);
    
    return ret;
}

static esp_err_t sh1107_write_data(sh1107_handle_t dev, const uint8_t* data, size_t size)
{
    sh1107_dev_t* sh1107 = (sh1107_dev_t*)dev;
    uint8_t* write_buf = malloc(size + 1);
    if (write_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    write_buf[0] = SH1107_DATA;
    memcpy(write_buf + 1, data, size);
    
    i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
    i2c_master_start(i2c_cmd);
    i2c_master_write_byte(i2c_cmd, (sh1107->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(i2c_cmd, write_buf, size + 1, true);
    i2c_master_stop(i2c_cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(sh1107->i2c_num, i2c_cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(i2c_cmd);
    free(write_buf);
    
    return ret;
}

sh1107_handle_t sh1107_create(i2c_port_t i2c_num, uint8_t address)
{
    ESP_LOGI(TAG, "Creating SH1107 device...");
    
    sh1107_dev_t* dev = calloc(1, sizeof(sh1107_dev_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for device");
        return NULL;
    }
    
    dev->i2c_num = i2c_num;
    dev->address = address;
    
    // Initialize display with proper sequence
    ESP_LOGI(TAG, "Sending initialization commands...");
    
    // Turn off display
    sh1107_write_cmd(dev, SH1107_DISPLAY_OFF);
    
    // Set display clock
    sh1107_write_cmd(dev, SH1107_SET_DISPLAY_CLOCK);
    sh1107_write_cmd(dev, 0x80);  // Default value
    
    // Set multiplex ratio
    sh1107_write_cmd(dev, 0xA8);
    sh1107_write_cmd(dev, 0x7F);  // 1/128 duty for 128x128 display
    
    // Set display offset
    sh1107_write_cmd(dev, SH1107_SET_COLUMN_LOW);
    sh1107_write_cmd(dev, SH1107_SET_COLUMN_HIGH);
    
    // Set display start line
    sh1107_write_cmd(dev, 0x40);  // Start line 0
    
    // Set charge pump
    sh1107_write_cmd(dev, SH1107_SET_CHARGE_PUMP);
    sh1107_write_cmd(dev, 0x14);  // Enable charge pump
    
    // Set memory addressing mode
    sh1107_write_cmd(dev, 0x20);
    sh1107_write_cmd(dev, 0x02);  // Page addressing mode
    
    // Set segment re-map
    sh1107_write_cmd(dev, 0xA0 | 0x01);  // Column address 127 is mapped to SEG0
    
    // Set COM output direction
    sh1107_write_cmd(dev, 0xC0 | 0x08);  // COM127 to COM0
    
    // Set contrast
    sh1107_write_cmd(dev, SH1107_SET_CONTRAST);
    sh1107_write_cmd(dev, 0x80);  // Default value
    
    // Set pre-charge period
    sh1107_write_cmd(dev, SH1107_SET_PRECHARGE);
    sh1107_write_cmd(dev, 0x22);  // Default value
    
    // Set VCOMH deselect level
    sh1107_write_cmd(dev, SH1107_SET_VCOM_DESELECT);
    sh1107_write_cmd(dev, 0x20);  // 0.77 * Vcc
    
    // Set display mode
    sh1107_write_cmd(dev, SH1107_DISPLAY_RAM);  // Display follows RAM content
    sh1107_write_cmd(dev, SH1107_DISPLAY_NORMAL);  // Normal display (not inverted)
    
    // Turn on display
    sh1107_write_cmd(dev, SH1107_DISPLAY_ON);
    
    ESP_LOGI(TAG, "SH1107 device created successfully");
    return dev;
}

esp_err_t sh1107_clear_screen(sh1107_handle_t dev, uint8_t color)
{
    sh1107_dev_t* sh1107 = (sh1107_dev_t*)dev;
    memset(sh1107->buffer, color ? 0xFF : 0x00, sizeof(sh1107->buffer));
    return ESP_OK;
}

esp_err_t sh1107_refresh_gram(sh1107_handle_t dev)
{
    sh1107_dev_t* sh1107 = (sh1107_dev_t*)dev;
    
    for (uint8_t i = 0; i < SH1107_PAGES; i++) {
        sh1107_write_cmd(dev, SH1107_SET_PAGE_ADDR | i);
        sh1107_write_cmd(dev, SH1107_SET_COLUMN_LOW | 0x02);  // Add 2 to column address for SH1107
        sh1107_write_cmd(dev, SH1107_SET_COLUMN_HIGH | 0x00);
        
        sh1107_write_data(dev, &sh1107->buffer[i * SH1107_WIDTH], SH1107_WIDTH);
    }
    
    return ESP_OK;
}

esp_err_t sh1107_display_text(sh1107_handle_t dev, uint8_t line, const char* text)
{
    if (line >= SH1107_PAGES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sh1107_dev_t* sh1107 = (sh1107_dev_t*)dev;
    uint8_t* buffer = &sh1107->buffer[line * SH1107_WIDTH];
    
    // Clear the line first
    memset(buffer, 0, SH1107_WIDTH);
    
    int x = 0;
    for (int i = 0; text[i] != '\0' && x < SH1107_WIDTH - FONT_WIDTH; i++) {
        char c = text[i];
        int font_index;
        
        if (c >= 'A' && c <= 'Z') {
            font_index = (c - 'A' + 2) * FONT_WIDTH;  // +2 because of space and ! characters
        } else if (c >= '0' && c <= '9') {
            font_index = (c - '0' + 28) * FONT_WIDTH;  // +28 because of A-Z characters
        } else if (c == ' ') {
            font_index = 0;  // Space character
        } else if (c == '!') {
            font_index = FONT_WIDTH;  // ! character
        } else {
            continue;  // Skip unsupported characters
        }
        
        // Copy font data to buffer
        for (int j = 0; j < FONT_WIDTH; j++) {
            buffer[x + j] = font_6x8[font_index + j];
        }
        
        x += FONT_WIDTH;
    }
    
    return ESP_OK;
}

esp_err_t sh1107_draw_line(sh1107_handle_t dev, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    sh1107_dev_t* sh1107 = (sh1107_dev_t*)dev;
    
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        if (x0 >= 0 && x0 < SH1107_WIDTH && y0 >= 0 && y0 < SH1107_HEIGHT) {
            uint8_t page = y0 / 8;
            uint8_t bit = y0 % 8;
            sh1107->buffer[page * SH1107_WIDTH + x0] |= (1 << bit);
        }
        
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
    
    return ESP_OK;
}

esp_err_t sh1107_fill_point(sh1107_handle_t dev, int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= SH1107_WIDTH || y < 0 || y >= SH1107_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    sh1107_dev_t* sh1107 = (sh1107_dev_t*)dev;
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    
    if (color) {
        sh1107->buffer[page * SH1107_WIDTH + x] |= (1 << bit);
    } else {
        sh1107->buffer[page * SH1107_WIDTH + x] &= ~(1 << bit);
    }
    
    return ESP_OK;
} 