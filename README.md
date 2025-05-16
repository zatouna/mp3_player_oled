# ESP32 MP3 Player with Waveform Visualization

This project implements an MP3 player on an ESP32 microcontroller with real-time waveform visualization on an SH1107 OLED display. The player supports high-quality audio playback and provides a visual representation of the audio waveform.

## Features

- MP3 audio playback (supports 44.1kHz stereo)
- Real-time waveform visualization on OLED display
- I2S audio output
- SH1107 OLED display support
- FreeRTOS-based implementation

## Hardware Requirements

- ESP32 development board
- SH1107 OLED display (I2C interface)
- Audio amplifier and speakers
- I2S DAC (if not built into the board)

## Pin Configuration

### I2C Display
- SCL: GPIO 18
- SDA: GPIO 8
- Display Address: 0x3C

### I2S Audio
- Uses default I2S pins of the ESP32 board

## Project Structure

```
.
├── main/
│   ├── play_mp3_control_example.c    # Main application code
│   ├── music-16b-2c-22050hz.mp3     # Sample audio file
│   ├── music-16b-2c-44100hz.mp3     # Sample audio file
│   └── music-16b-2c-8000hz.mp3      # Sample audio file
├── components/                       # Custom components
├── managed_components/              # ESP-IDF managed components
└── CMakeLists.txt                   # Project build configuration
```

## Building and Flashing

1. Install ESP-IDF (v4.4 or later)
2. Clone this repository
3. Configure the project:
   ```bash
   idf.py set-target esp32
   idf.py menuconfig
   ```
4. Build and flash:
   ```bash
   idf.py build
   idf.py -p (PORT) flash
   ```

## Usage

1. Power on the device
2. The OLED display will show "MP3 PLAYER" and "WAVEFORM"
3. The waveform visualization will start automatically when audio playback begins
4. The waveform display updates in real-time as the audio plays

## Waveform Visualization

The waveform visualization:
- Uses a 120x100 pixel display area
- Shows 256 samples per frame
- Updates in real-time
- Displays the left channel of stereo audio
- Includes amplitude scaling for better visualization

## Dependencies

- ESP-ADF (ESP Audio Development Framework)
- FreeRTOS
- ESP-IDF
- SH1107 OLED driver

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request

## Acknowledgments

- ESP-ADF team for the audio framework
- ESP-IDF team for the development framework
