#pragma once

#include "driver/i2c.h"
#include "esp_err.h"

typedef void* sh1107_handle_t;

/**
 * @brief Create a SH1107 device handle
 * @param i2c_num I2C port number
 * @param address I2C device address
 * @return SH1107 device handle or NULL if failed
 */
sh1107_handle_t sh1107_create(i2c_port_t i2c_num, uint8_t address);

/**
 * @brief Clear the display
 * @param dev SH1107 device handle
 * @param color 0 for black, 1 for white
 * @return ESP_OK on success
 */
esp_err_t sh1107_clear_screen(sh1107_handle_t dev, uint8_t color);

/**
 * @brief Refresh the display with the content in the buffer
 * @param dev SH1107 device handle
 * @return ESP_OK on success
 */
esp_err_t sh1107_refresh_gram(sh1107_handle_t dev);

/**
 * @brief Display text on a specific line
 * @param dev SH1107 device handle
 * @param line Line number (0-7)
 * @param text Text to display
 * @return ESP_OK on success
 */
esp_err_t sh1107_display_text(sh1107_handle_t dev, uint8_t line, const char* text);

/**
 * @brief Draw a line on the display
 * @param dev SH1107 device handle
 * @param x0 Start x coordinate
 * @param y0 Start y coordinate
 * @param x1 End x coordinate
 * @param y1 End y coordinate
 * @return ESP_OK on success
 */
esp_err_t sh1107_draw_line(sh1107_handle_t dev, int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/**
 * @brief Fill a point on the display
 * @param dev SH1107 device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param color 0 for black, 1 for white
 * @return ESP_OK on success
 */
esp_err_t sh1107_fill_point(sh1107_handle_t dev, int16_t x, int16_t y, uint8_t color); 