# GPS Bus Tracking System Configuration

## Basic Configuration (Edit these in the Arduino code)

### Bus Information
```cpp
const String BUS_ID = "BUS-001";                    // Change to your bus number
const String ROUTE_NAME = "Route-A-Downtown";       // Change to your route name
```

### Phone Numbers (IMPORTANT - Update these!)
```cpp
const char* ADMIN_PHONE = "+91XXXXXXXXXX";          // Primary admin number
const char* BACKUP_PHONE = "+91YYYYYYYYYY";         // Backup admin number  
const char* EMERGENCY_PHONE = "+91ZZZZZZZZZZ";      // Emergency contact
```

### Tracking Intervals
```cpp
const unsigned long NORMAL_SEND_INTERVAL = 300000;  // 5 minutes (normal mode)
const unsigned long TRACKING_SEND_INTERVAL = 60000; // 1 minute (tracking mode)
const unsigned long EMERGENCY_INTERVAL = 30000;     // 30 seconds (emergency)
```

### Speed Monitoring
```cpp
const float SPEED_LIMIT = 60.0;                     // Speed limit in km/h
```

## SMS Commands (Send these to the system)

### Administrative Commands
- **"TRACK ON"** - Enable enhanced tracking (1-minute intervals)
- **"TRACK OFF"** - Disable enhanced tracking (return to 5-minute intervals)
- **"STATUS"** - Get system status report
- **"LOCATION"** - Get current GPS location immediately
- **"EMERGENCY OFF"** - Disable emergency mode

### Example Usage:
Send SMS "TRACK ON" to the SIM card number to enable real-time tracking.

## System Modes

### Normal Mode (Default)
- GPS updates every 5 minutes
- Speed monitoring active
- LED solid when GPS fixed

### Tracking Mode
- GPS updates every 1 minute
- Enabled via SMS command "TRACK ON"
- Useful for detailed route monitoring

### Emergency Mode
- GPS updates every 30 seconds
- Triggered by panic button press
- Sends alerts to all configured numbers
- Can only be disabled via SMS command

## Hardware Pin Configuration

### If you need to change pins, update these constants:
```cpp
#define GPS_RX_PIN 16        // ESP32 pin connected to GPS TX
#define GPS_TX_PIN 17        // ESP32 pin connected to GPS RX
#define GSM_RX_PIN 26        // ESP32 pin connected to GSM TX
#define GSM_TX_PIN 27        // ESP32 pin connected to GSM RX
#define GSM_RST_PIN 23       // ESP32 pin connected to GSM RST
#define LED_PIN 19           // ESP32 pin connected to LED
#define PANIC_BUTTON_PIN 18  // ESP32 pin connected to panic button
#define BUZZER_PIN 21        // ESP32 pin connected to buzzer
```

## Alert Types

### Speed Alert
- Triggered when bus exceeds speed limit
- Maximum one alert per minute to avoid spam
- Sent to admin phone only

### Emergency Alert
- Triggered by panic button
- Sent to all configured phone numbers
- Includes GPS location and "IMMEDIATE ASSISTANCE REQUIRED"

### Startup Notification
- Sent when system boots up
- Confirms system is operational
- Lists available SMS commands

## Installation Checklist

### Before Deployment:
1. ✅ Update all phone numbers in code
2. ✅ Set correct bus ID and route name
3. ✅ Test GPS reception at installation location
4. ✅ Verify SIM card has 2G coverage
5. ✅ Test panic button functionality
6. ✅ Confirm power supply provides adequate current
7. ✅ Test SMS commands work correctly
8. ✅ Verify emergency alerts reach all contacts

### Power Requirements:
- ESP32 + GPS: 3.3V, 250mA
- SIM800L: 3.7V-4.2V, 2A peak current
- Use automotive-grade DC-DC converters for vehicles
- Add backup battery for power outages

### Mounting Considerations:
- GPS antenna: Roof-mounted with clear sky view
- GSM antenna: Away from metal obstructions
- Control unit: Inside vehicle, protected from weather
- Panic button: Easily accessible to driver
- LED indicator: Visible to driver for status

## Troubleshooting Guide

### GPS Not Fixing:
- Check antenna connection
- Ensure clear sky view (move away from buildings)
- Cold start can take 5+ minutes
- Verify 3.3V power supply

### SMS Not Sending:
- Check SIM card has credit/active plan
- Verify 2G network coverage
- Ensure SIM800L has adequate power (2A supply)
- Check antenna connections

### System Resets:
- Usually power supply issue with SIM800L
- Add capacitors (100µF + 470µF) near SIM800L VCC
- Use dedicated 2A power supply for SIM800L

### Commands Not Working:
- Ensure SMS is sent in correct format (case sensitive)
- Check serial monitor for incoming SMS notifications
- Verify phone number format includes country code

## Maintenance

### Regular Checks:
- Monitor system status via SMS weekly
- Check antenna connections monthly
- Verify power supply voltage quarterly
- Test emergency button function quarterly
- Update GPS almanac if accuracy degrades

### Firmware Updates:
- Check for TinyGPS++ library updates
- Monitor ESP32 Arduino core updates
- Backup current working configuration before updates
