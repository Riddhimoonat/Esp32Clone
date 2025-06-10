# GPS Bus Tracking System - Wiring and Setup Guide

## Hardware Components Required:
- ESP32 Development Board
- NEO-6M GPS Module with antenna
- SIM800L GSM Module with antenna
- LED (any color)
- 1kΩ resistor (for LED)
- Jumper wires
- Breadboard (optional)
- 3.7V-4.2V power supply for SIM800L (LiPo battery recommended)
- 100µF tantalum capacitor + 470µF electrolytic capacitor for SIM800L power filtering

## Wiring Connections:

### ESP32 Power:
- Connect ESP32 to USB for programming and debugging
- Or use 3.3V external power supply

### NEO-6M GPS Module:
```
NEO-6M    ->    ESP32
VCC       ->    3.3V
GND       ->    GND
TX        ->    GPIO 16 (RX)
RX        ->    GPIO 17 (TX)
```

### SIM800L GSM Module:
```
SIM800L   ->    Connection
VCC       ->    3.7V-4.2V (separate power supply, NOT ESP32!)
GND       ->    ESP32 GND (common ground)
TX        ->    ESP32 GPIO 26 (RX)
RX        ->    ESP32 GPIO 27 (TX)
RST       ->    ESP32 GPIO 23
```

### LED Status Indicator:
```
LED       ->    ESP32
Anode     ->    GPIO 19 (through 1kΩ resistor)
Cathode   ->    GND
```

## Power Supply Requirements:

### ESP32 & NEO-6M:
- Voltage: 3.3V
- Current: ~250mA
- Can be powered via USB or 3.3V regulator

### SIM800L (CRITICAL):
- Voltage: 3.7V - 4.2V
- Current: 500mA normal, up to 2A during transmission
- MUST use separate power supply with adequate current capacity
- Add capacitors near VCC pin: 100µF tantalum + 470µF electrolytic
- Poor power supply will cause random resets and connection failures

## Software Setup:

### Arduino IDE Libraries Required:
1. **TinyGPS++**: For parsing GPS NMEA data
   - Install via Library Manager: Sketch -> Include Library -> Manage Libraries
   - Search for "TinyGPS++" by Mikal Hart

2. **SoftwareSerial**: Usually included with Arduino IDE
   - If not available, install ESP32 SoftwareSerial library

### ESP32 Board Setup:
1. Install ESP32 board package in Arduino IDE:
   - File -> Preferences -> Additional Board Manager URLs
   - Add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   - Tools -> Board -> Boards Manager -> Search "ESP32" -> Install

2. Select board: Tools -> Board -> ESP32 Arduino -> ESP32 Dev Module

### Configuration:
1. Update phone number in code:
   ```cpp
   const char* PHONE_NUMBER = "+91XXXXXXXXXX"; // Replace with actual number
   ```

2. Adjust timing if needed:
   ```cpp
   const unsigned long SEND_INTERVAL = 60000; // Send every 60 seconds
   const unsigned long RETRY_INTERVAL = 10000; // Retry after 10 seconds
   ```

## SIM Card Requirements:
- 2G/GSM compatible SIM card (SIM800L only works with 2G)
- Active plan with SMS capability
- Ensure 2G network coverage in your area
- Disable PIN lock on SIM card before use

## LED Status Indicators:
- **Blinking (1Hz)**: Waiting for GPS fix
- **Solid ON**: GPS fixed and SMS sent successfully
- **OFF**: Error state (no GPS fix after timeout or SMS failure)

## Antenna Placement:
- **GPS Antenna**: Must have clear view of sky, place near window or outside
- **GSM Antenna**: Position for best cellular reception, avoid metal obstructions

## Testing Steps:
1. Upload code to ESP32
2. Open Serial Monitor (115200 baud)
3. Wait for GPS fix (may take 2-5 minutes outdoors)
4. Verify GSM network registration
5. Check SMS reception on target phone

## Troubleshooting:

### GPS Issues:
- Ensure antenna is connected and has sky view
- Cold start can take several minutes
- Check serial communication (9600 baud)
- Verify 3.3V power supply

### GSM Issues:
- Check SIM card is 2G compatible and unlocked
- Verify power supply provides adequate current (2A capability)
- Ensure antennas are properly connected
- Check network coverage (2G/GSM)
- Monitor SIM800L LED: 3-second blink = network connected

### Power Issues:
- SIM800L needs dedicated power supply (3.7V-4.2V, 2A)
- Add capacitors for power filtering
- Common ground between all modules essential

## Bus Installation Considerations:
- Use weatherproof enclosure
- Secure GPS antenna with sky view (roof mounting)
- Use automotive-grade power supply (12V to 3.3V/4V converters)
- Consider backup battery for power outages
- Protect against vibration and temperature extremes
- Use shielded cables for antenna connections
