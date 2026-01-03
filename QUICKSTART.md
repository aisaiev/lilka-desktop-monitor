# Quick Start Guide

## Overview

This firmware receives pixel updates over WiFi and displays them on Lilka in real-time. It's designed to be loaded from KeiraOS and uses Keira's WiFi credentials automatically.

## Prerequisites

- Lilka v2 with KeiraOS flashed
- WiFi configured in KeiraOS
- Both devices on the same WiFi network

### Step 1: Upload Firmware

**Run from KeiraOS**
1. Build the firmware using PlatformIO or download the prebuilt firmware from the releases.
1. Copy firmware file to SD card
2. In KeiraOS file manager, open the firmware .bin file
3. Device will reboot and load the firmware
4. It will automatically connect using Keira's saved WiFi credentials

### Step 2: Note the IP Address

After successful WiFi connection, the screen will show:
```
IP Address:
192.168.1.100
Waiting for connection...
```

Note this IP address for the next step.

### Step 3: Install Python Dependencies

Navigate to transmitter directory and run:

```bash
# Create virtual environment
python3 -m venv venv

# Install dependencies
venv/bin/pip install -r requirements.txt
```

**Note for macOS/Linux users with Homebrew Python**: If you get an "externally-managed-environment" error, use the virtual environment approach above.

### Step 4: Stream Your Screen!

```bash
# Basic usage
venv/bin/python transmitter.py --ip 192.168.1.100

# Or activate venv first
source venv/bin/activate
python transmitter.py --ip 192.168.1.100
```

Replace `192.168.1.100` with your device's IP address from Step 2.

That's it! Your screen should now be streaming to the Lilka display.

## Common Issues

**"WiFi Error: No WiFi configured"**
- Configure WiFi in KeiraOS first
- The firmware reads credentials from Keira's storage automatically

**"Connection Failed"**
- Verify WiFi password is correct in KeiraOS
- Check that WiFi network is available
- Restart device and try again

**"Connection refused" (from transmitter)**
- Verify ESP32 IP address on the display
- Ensure both devices are on the same WiFi network
- Check firewall settings (port 8090 must be open)

**"Screen Recording permission" (macOS)**
- System Preferences → Security & Privacy → Privacy → Screen Recording
- Enable Terminal (or your Python app)

**Low frame rate**
- Try lower threshold: `python transmitter.py --ip <IP> --threshold 8 --target-fps 20`
- Reduce capture area or resolution
- Check WiFi signal strength

**Firmware doesn't start after loading from SD**
- Ensure .bin file is not corrupted
- Verify SD card is properly formatted (FAT32)
- Try reflashing with PlatformIO

## Configuration

### WiFi Settings
WiFi credentials are managed entirely in KeiraOS. No hardcoded credentials in firmware.

To change WiFi:
1. Boot into KeiraOS
2. Go to Settings
3. Select new network and enter password
4. Reboot into this firmware - it will use new credentials automatically

### Network Port
The receiver listens on **port 8090** (TCP). Configure your transmitter accordingly.

### Display
- Display dimensions detected automatically from hardware
- Supports Lilka v2 display 280x240 1.69" TFT
- RGB565 color format (16-bit)
