# Lilka Desktop Monitor

Stream your computer screen to [Lilka](https://lilka.dev) device over WiFi. This firmware enables real-time display streaming to Lilka's 1.69" TFT display and is designed to integrate seamlessly with KeiraOS.
Based on the ESP32 Desktop Monitor [project](https://github.com/tuckershannon/ESP32-Desktop-Monitor).

## Demo
[Demo](https://github.com/user-attachments/assets/cd33a3b0-5acb-45c2-92a5-74ff82f2dcb1)

## Hardware Requirements

### Lilka Device
- **Board**: Lilka v2 (ESP32-S3 based)
- **Display**: 1.69" ST7789 TFT LCD (280x240 pixels)
- **Operating System**: KeiraOS (for WiFi configuration and firmware loading)
- **Connectivity**: WiFi

### Computer
- Any computer running Python 3.7+
- macOS, Linux, or Windows (with appropriate screen capture libraries)

## Software Requirements

### Firmware Build (PlatformIO)
- **PlatformIO** (VS Code extension or CLI)
- **Lilka SDK** (installed via PlatformIO)
- Build environment: `lilka_v2`

### Computer Side (Python)
- Python 3.7 or higher
- Required packages (install via `pip install -r requirements.txt`):
  - `opencv-python` - Image processing and scaling
  - `mss` - Cross-platform screen capture
  - `numpy` - Array operations

## Setup Instructions

### 1. Configure WiFi in KeiraOS

**Important**: WiFi must be configured in KeiraOS before using this firmware.

1. Boot Lilka into KeiraOS
2. Go to Settings
3. Select your WiFi network and enter password
4. Credentials are saved in Keira's storage

The firmware will automatically use these saved credentials.

### 2. Build and Load Firmware

**Load from KeiraOS**
```bash
# Build firmware
pio run --environment lilka_v2

# Copy firmware.bin from .pio/build/lilka_v2/ to SD card
# In KeiraOS file manager, open the .bin file to load it
```

### 3. Note the IP Address

After the firmware loads and connects to WiFi, the display will show:
```
IP Address:
192.168.1.100
Waiting for connection...
```

Note this IP address for the next step.

### 4. Install Python Dependencies

Navigate to transmitter directory and run:

```bash
# Create virtual environment (recommended)
python3 -m venv venv

# Install dependencies
venv/bin/pip install -r requirements.txt
```

**macOS users**: Grant "Screen Recording" permission to Terminal (or your Python environment) when prompted.

### 5. Run the Transmitter

```bash
# Using virtual environment
venv/bin/python transmitter.py --ip <LILKA_IP_ADDRESS>

# Or activate venv first
source venv/bin/activate
python transmitter.py --ip <LILKA_IP_ADDRESS>
```

Replace `<LILKA_IP_ADDRESS>` with the IP address from step 3.

## Usage

### Basic Usage

```bash
# Stream leftmost monitor to Lilka
python transmitter.py --ip 192.168.1.100

# Stream specific monitor (1-based index)
python transmitter.py --ip 192.168.1.100 --monitor-index 2

# Adjust frame rate (default: 15 FPS)
python transmitter.py --ip 192.168.1.100 --target-fps 20

# Adjust change detection sensitivity (default: 5)
python transmitter.py --ip 192.168.1.100 --threshold 8
```

### Command Line Options

- `--ip <IP>` - Lilka IP address (required)
- `--port <PORT>` - TCP port (default: 8090)
- `--monitor-index <N>` - Select monitor by index (1-based, default: leftmost)
- `--prefer-largest` - Use largest monitor instead of leftmost
- `--target-fps <FPS>` - Target frame rate (default: 15.0)
- `--threshold <N>` - Pixel change threshold 0-255 (default: 5, higher = less sensitive)
- `--full-frame` - Send all pixels every frame (no diffing, slower)
- `--max-updates-per-frame <N>` - Max pixels per packet (default: 3000)
- `--rotate <0|90|180|270>` - Rotate capture before scaling
- `--show-cursor` - Draw cursor on captured frame (macOS only)

### Performance Tuning

For better frame rates:

1. **Increase threshold** (reduces bandwidth):
   ```bash
   python transmitter.py --ip 192.168.1.100 --threshold 8
   ```

2. **Adjust frame rate**:
   ```bash
   python transmitter.py --ip 192.168.1.100 --target-fps 20
   ```

3. **Increase packet size** (for high-motion content):
   ```bash
   python transmitter.py --ip 192.168.1.100 --max-updates-per-frame 8000
   ```

## How It Works

### Architecture

**KeiraOS Integration**:
- Firmware reads WiFi credentials from Keira's NVS storage
- No hardcoded credentials - fully managed through KeiraOS settings
- Can be loaded directly from SD card via KeiraOS file manager

**Display Handling**:
- Dynamic display dimension detection via Lilka SDK
- Uses Canvas buffering for smooth rendering
- Supports RGB565 color format (16-bit)

### Protocol

The system automatically selects between two optimized protocols:

**PXUP v2 (Pixel Updates)**:
- Best for: Complex content with scattered changes
- Header: `'PXUP'` (4 bytes magic) + metadata
- Body: Individual pixel updates (x, y, color as uint16)
- 6 bytes per pixel update

**PXUR v1 (Run-Length Encoding)**:
- Best for: Solid areas and horizontal runs
- Header: `'PXUR'` (4 bytes magic) + metadata
- Body: Run-length encoded rows (y, x0, length, color as uint16)
- 8 bytes per run

The transmitter automatically chooses the most efficient protocol per frame.

### Optimizations

- **Frame Diffing**: Only changed pixels are transmitted (configurable threshold)
- **PSRAM Buffering**: Uses ESP32-S3 PSRAM for large frame buffers
- **Canvas Rendering**: Double-buffered rendering prevents flickering
- **TCP_NODELAY**: Low-latency network communication
- **Adaptive Protocol**: Automatic selection between PXUP and PXUR

## Troubleshooting

### WiFi Connection Issues

**"WiFi Error: No WiFi configured"**:
- Configure WiFi in KeiraOS first
- Firmware reads credentials from Keira's shared storage

**"Connection Failed"**:
- Verify password is correct in KeiraOS
- Check WiFi network availability
- Restart Lilka and try again

### Transmitter Connection Issues

**"Connection refused"**:
1. Verify Lilka's IP address on the display
2. Check firewall settings (port 8090 must be open)
3. Ensure both devices are on the same WiFi network
4. Try restarting the firmware

### Performance Issues

**Low Frame Rate**:
1. Check WiFi signal strength
2. Increase `--threshold` to reduce bandwidth
3. Lower `--target-fps` if network can't keep up
4. Reduce capture area or resolution

**Display Flickering**:
- This firmware uses Canvas buffering to prevent flickering
- If issues persist, try lowering frame rate: `--target-fps 10`

### Firmware Loading Issues

**Firmware doesn't start after loading from SD**:
1. Ensure .bin file is not corrupted
2. Verify SD card is FAT32 formatted
3. Try rebuilding with PlatformIO
4. Check serial output for error messages

## License

This project is provided as-is for educational and personal use.

## Contributing

Contributions welcome! Please feel free to submit issues or pull requests.

## Acknowledgments

- Lilka SDK
- KeiraOS
- mss library for cross-platform screen capture
- ESP32 Desktop Monitor
