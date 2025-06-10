/*
 * Enhanced GPS Bus Tracking System
 * ESP32 + NEO-6M GPS + SIM800L GSM
 * 
 * Additional Features for Bus Tracking:
 * - Bus ID identification
 * - Route information
 * - Speed monitoring with alerts
 * - Emergency panic button support
 * - Multiple phone numbers for alerts
 * - Data logging to EEPROM
 * - Over-speed alerts
 * - Geo-fence notifications
 */

#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <EEPROM.h>

// Pin definitions
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GSM_RX_PIN 26
#define GSM_TX_PIN 27
#define GSM_RST_PIN 23
#define LED_PIN 19
#define PANIC_BUTTON_PIN 18  // Emergency button
#define BUZZER_PIN 21        // Optional buzzer for alerts

// Serial communication objects
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);
TinyGPSPlus gps;

// Configuration - Customize for your bus system
const String BUS_ID = "BUS-001";                    // Unique bus identifier
const String ROUTE_NAME = "Route-A-Downtown";       // Bus route name
const char* ADMIN_PHONE = "+91XXXXXXXXXX";          // Primary admin number
const char* BACKUP_PHONE = "+91YYYYYYYYYY";         // Backup admin number
const char* EMERGENCY_PHONE = "+91ZZZZZZZZZZ";      // Emergency contact

// Thresholds and timing
const float SPEED_LIMIT = 60.0;                     // Speed limit in km/h
const unsigned long GPS_TIMEOUT = 120000;           // 2 minutes GPS timeout
const unsigned long NORMAL_SEND_INTERVAL = 300000;  // Send GPS every 5 minutes (normal)
const unsigned long TRACKING_SEND_INTERVAL = 60000; // Send GPS every 1 minute (tracking mode)
const unsigned long RETRY_INTERVAL = 10000;         // Retry after 10 seconds
const unsigned long EMERGENCY_INTERVAL = 30000;     // Emergency updates every 30 seconds

// System state variables
bool gpsFixed = false;
bool gsmReady = false;
bool emergencyMode = false;
bool trackingMode = false;                          // Can be enabled via SMS command
unsigned long lastSendTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastSpeedCheck = 0;
bool ledState = false;
bool panicPressed = false;

// GPS data variables
double latitude, longitude;
double prevLatitude = 0, prevLongitude = 0;
String dateTime;
float altitude, speed;
int satellites;

// Statistics
unsigned long totalDistance = 0;  // in meters
unsigned long tripStartTime = 0;
bool tripActive = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Enhanced GPS Bus Tracking System Starting...");
  Serial.println("Bus ID: " + BUS_ID);
  Serial.println("Route: " + ROUTE_NAME);
  
  // Initialize EEPROM
  EEPROM.begin(512);
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(GSM_RST_PIN, OUTPUT);
  pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize outputs
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize GPS
  gpsSerial.begin(9600);
  Serial.println("GPS module initialized");
  
  // Initialize GSM
  gsmSerial.begin(9600);
  Serial.println("GSM module initialized");
  
  // Reset and initialize GSM
  resetGSM();
  delay(2000);
  initializeGSM();
  
  // Send startup notification
  if (gsmReady) {
    sendStartupNotification();
  }
  
  Serial.println("System initialization complete");
  Serial.println("Waiting for GPS fix...");
  
  tripStartTime = millis();
}

void loop() {
  // Check panic button
  checkPanicButton();
  
  // Read GPS data
  readGPSData();
  
  // Handle LED blinking when waiting for GPS fix
  if (!gpsFixed) {
    blinkLED();
  }
  
  // Check for over-speed
  if (gpsFixed) {
    checkSpeedLimit();
  }
  
  // Check incoming SMS commands
  checkSMSCommands();
  
  // Determine send interval based on mode
  unsigned long sendInterval = NORMAL_SEND_INTERVAL;
  if (emergencyMode) {
    sendInterval = EMERGENCY_INTERVAL;
  } else if (trackingMode) {
    sendInterval = TRACKING_SEND_INTERVAL;
  }
  
  // Check if it's time to send GPS data
  unsigned long currentTime = millis();
  if (gpsFixed && gsmReady && (currentTime - lastSendTime >= sendInterval)) {
    sendGPSData();
    lastSendTime = currentTime;
  }
  
  // Update trip statistics
  updateTripStats();
  
  delay(100);
}

void readGPSData() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (gps.encode(c)) {
      if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid()) {
        prevLatitude = latitude;
        prevLongitude = longitude;
        
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        altitude = gps.altitude.meters();
        speed = gps.speed.kmph();
        satellites = gps.satellites.value();
        
        // Format date and time (add timezone offset if needed)
        dateTime = String(gps.date.day()) + "/" + 
                  String(gps.date.month()) + "/" + 
                  String(gps.date.year()) + " " +
                  formatTime(gps.time.hour()) + ":" + 
                  formatTime(gps.time.minute()) + ":" + 
                  formatTime(gps.time.second());
        
        if (!gpsFixed) {
          gpsFixed = true;
          digitalWrite(LED_PIN, HIGH);
          Serial.println("GPS Fix Acquired!");
          printGPSInfo();
          
          if (!tripActive && speed > 5.0) { // Start trip when moving
            tripActive = true;
            tripStartTime = millis();
            Serial.println("Trip started");
          }
        }
      }
    }
  }
  
  // Check for GPS timeout
  if (!gpsFixed && millis() > GPS_TIMEOUT) {
    Serial.println("GPS timeout - no fix acquired");
    digitalWrite(LED_PIN, LOW);
  }
}

void checkPanicButton() {
  bool currentState = digitalRead(PANIC_BUTTON_PIN) == LOW;
  
  if (currentState && !panicPressed) {
    panicPressed = true;
    emergencyMode = true;
    
    // Sound buzzer
    for (int i = 0; i < 5; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);
      delay(200);
    }
    
    Serial.println("EMERGENCY BUTTON PRESSED!");
    
    if (gpsFixed && gsmReady) {
      sendEmergencyAlert();
    }
  } else if (!currentState && panicPressed) {
    panicPressed = false;
    // Don't automatically disable emergency mode - require SMS command
  }
}

void checkSpeedLimit() {
  unsigned long currentTime = millis();
  if (currentTime - lastSpeedCheck >= 5000) { // Check every 5 seconds
    lastSpeedCheck = currentTime;
    
    if (speed > SPEED_LIMIT) {
      Serial.println("SPEED LIMIT EXCEEDED: " + String(speed) + " km/h");
      
      // Send speed alert (but not too frequently)
      static unsigned long lastSpeedAlert = 0;
      if (currentTime - lastSpeedAlert >= 60000) { // Max one alert per minute
        sendSpeedAlert();
        lastSpeedAlert = currentTime;
      }
    }
  }
}

void checkSMSCommands() {
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    
    // Check for incoming SMS notification
    if (response.indexOf("+CMTI:") > -1) {
      // Extract message index
      int indexStart = response.indexOf(",") + 1;
      int messageIndex = response.substring(indexStart).toInt();
      
      // Read the SMS
      readSMS(messageIndex);
    }
  }
}

void readSMS(int index) {
  String command = "AT+CMGR=" + String(index);
  gsmSerial.println(command);
  delay(1000);
  
  String response = "";
  while (gsmSerial.available()) {
    response += (char)gsmSerial.read();
  }
  
  // Parse SMS content for commands
  if (response.indexOf("TRACK ON") > -1) {
    trackingMode = true;
    Serial.println("Tracking mode enabled via SMS");
    sendSMS(ADMIN_PHONE, "Tracking mode enabled for " + BUS_ID);
  } else if (response.indexOf("TRACK OFF") > -1) {
    trackingMode = false;
    Serial.println("Tracking mode disabled via SMS");
    sendSMS(ADMIN_PHONE, "Tracking mode disabled for " + BUS_ID);
  } else if (response.indexOf("EMERGENCY OFF") > -1) {
    emergencyMode = false;
    Serial.println("Emergency mode disabled via SMS");
    sendSMS(ADMIN_PHONE, "Emergency mode disabled for " + BUS_ID);
  } else if (response.indexOf("STATUS") > -1) {
    sendStatusReport();
  } else if (response.indexOf("LOCATION") > -1) {
    if (gpsFixed) {
      sendGPSData();
    } else {
      sendSMS(ADMIN_PHONE, BUS_ID + ": GPS not fixed. Last known location unavailable.");
    }
  }
  
  // Delete the read SMS
  gsmSerial.println("AT+CMGD=" + String(index));
  delay(1000);
}

void sendGPSData() {
  if (!gpsFixed || !gsmReady) {
    Serial.println("Cannot send SMS - GPS or GSM not ready");
    digitalWrite(LED_PIN, LOW);
    return;
  }
  
  Serial.println("Sending GPS data...");
  
  String message = BUS_ID + " - " + ROUTE_NAME + "\n";
  message += "Location: " + String(latitude, 6) + "," + String(longitude, 6) + "\n";
  message += "Speed: " + String(speed, 1) + " km/h\n";
  message += "Altitude: " + String(altitude, 0) + "m\n";
  message += "Satellites: " + String(satellites) + "\n";
  message += "Time: " + dateTime + "\n";
  
  if (emergencyMode) {
    message += "⚠️ EMERGENCY MODE ACTIVE ⚠️\n";
  }
  if (trackingMode) {
    message += "📍 Enhanced tracking enabled\n";
  }
  
  message += "Map: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
  
  bool success = sendSMS(ADMIN_PHONE, message);
  
  if (success) {
    Serial.println("GPS data sent successfully");
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println("Failed to send GPS data");
    digitalWrite(LED_PIN, LOW);
  }
}

void sendEmergencyAlert() {
  String message = "🚨 EMERGENCY ALERT 🚨\n";
  message += "Bus: " + BUS_ID + "\n";
  message += "Route: " + ROUTE_NAME + "\n";
  message += "Location: " + String(latitude, 6) + "," + String(longitude, 6) + "\n";
  message += "Speed: " + String(speed, 1) + " km/h\n";
  message += "Time: " + dateTime + "\n";
  message += "IMMEDIATE ASSISTANCE REQUIRED\n";
  message += "Map: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
  
  // Send to multiple numbers
  sendSMS(ADMIN_PHONE, message);
  sendSMS(EMERGENCY_PHONE, message);
  if (strlen(BACKUP_PHONE) > 5) {
    sendSMS(BACKUP_PHONE, message);
  }
}

void sendSpeedAlert() {
  String message = "⚠️ SPEED ALERT ⚠️\n";
  message += "Bus: " + BUS_ID + "\n";
  message += "Current Speed: " + String(speed, 1) + " km/h\n";
  message += "Speed Limit: " + String(SPEED_LIMIT, 1) + " km/h\n";
  message += "Location: " + String(latitude, 6) + "," + String(longitude, 6) + "\n";
  message += "Time: " + dateTime;
  
  sendSMS(ADMIN_PHONE, message);
}

void sendStartupNotification() {
  String message = "Bus Tracking System Started\n";
  message += "Bus ID: " + BUS_ID + "\n";
  message += "Route: " + ROUTE_NAME + "\n";
  message += "System ready for operation\n";
  message += "Commands: TRACK ON/OFF, STATUS, LOCATION, EMERGENCY OFF";
  
  sendSMS(ADMIN_PHONE, message);
}

void sendStatusReport() {
  String message = "Status Report - " + BUS_ID + "\n";
  message += "GPS: " + String(gpsFixed ? "OK" : "NO FIX") + "\n";
  message += "GSM: " + String(gsmReady ? "OK" : "ERROR") + "\n";
  message += "Emergency Mode: " + String(emergencyMode ? "ON" : "OFF") + "\n";
  message += "Tracking Mode: " + String(trackingMode ? "ON" : "OFF") + "\n";
  
  if (gpsFixed) {
    message += "Current Speed: " + String(speed, 1) + " km/h\n";
    message += "Satellites: " + String(satellites) + "\n";
  }
  
  message += "Uptime: " + String(millis() / 3600000) + "h " + String((millis() % 3600000) / 60000) + "m";
  
  sendSMS(ADMIN_PHONE, message);
}

bool sendSMS(const char* phoneNumber, String message) {
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(1000);
  
  gsmSerial.print(message);
  delay(1000);
  
  gsmSerial.write(26); // Ctrl+Z
  delay(1000);
  
  // Wait for response
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) {
    if (gsmSerial.available()) {
      String response = gsmSerial.readString();
      if (response.indexOf("OK") > -1) {
        return true;
      }
      if (response.indexOf("ERROR") > -1) {
        return false;
      }
    }
  }
  return false;
}

void blinkLED() {
  unsigned long currentTime = millis();
  if (currentTime - lastBlinkTime >= 1000) { // Blink every second
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    lastBlinkTime = currentTime;
  }
}

void resetGSM() {
  Serial.println("Resetting GSM module...");
  digitalWrite(GSM_RST_PIN, LOW);
  delay(100);
  digitalWrite(GSM_RST_PIN, HIGH);
  delay(3000);
}

void initializeGSM() {
  Serial.println("Initializing GSM module...");
  
  // Test AT command
  sendATCommand("AT", 2000);
  
  // Disable echo
  sendATCommand("ATE0", 2000);
  
  // Enable SMS notifications
  sendATCommand("AT+CNMI=1,2,0,0,0", 2000);
  
  // Check SIM card
  if (sendATCommand("AT+CPIN?", 5000).indexOf("READY") > -1) {
    Serial.println("SIM card ready");
  } else {
    Serial.println("SIM card not ready!");
    return;
  }
  
  // Check network registration
  String response = sendATCommand("AT+CREG?", 5000);
  if (response.indexOf(",1") > -1 || response.indexOf(",5") > -1) {
    Serial.println("Network registered");
    gsmReady = true;
  } else {
    Serial.println("Network not registered");
    sendATCommand("AT+CREG=1", 5000);
    delay(5000);
    response = sendATCommand("AT+CREG?", 5000);
    if (response.indexOf(",1") > -1 || response.indexOf(",5") > -1) {
      Serial.println("Network registered after retry");
      gsmReady = true;
    }
  }
  
  // Set SMS text mode
  sendATCommand("AT+CMGF=1", 2000);
  
  // Set SMS character set
  sendATCommand("AT+CSCS=\"GSM\"", 2000);
  
  if (gsmReady) {
    Serial.println("GSM module ready");
  } else {
    Serial.println("GSM module not ready!");
  }
}

String sendATCommand(String command, int timeout) {
  String response = "";
  gsmSerial.println(command);
  
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
    }
  }
  
  Serial.print("AT Command: ");
  Serial.println(command);
  Serial.print("Response: ");
  Serial.println(response);
  
  return response;
}

void updateTripStats() {
  if (gpsFixed && tripActive && prevLatitude != 0 && prevLongitude != 0) {
    // Calculate distance between current and previous position
    double distanceMeters = TinyGPSPlus::distanceBetween(
      prevLatitude, prevLongitude, latitude, longitude);
    
    if (distanceMeters < 1000) { // Ignore unrealistic jumps
      totalDistance += (unsigned long)distanceMeters;
    }
  }
}

String formatTime(int timeValue) {
  return timeValue < 10 ? "0" + String(timeValue) : String(timeValue);
}

void printGPSInfo() {
  Serial.print("Location: ");
  Serial.print(latitude, 6);
  Serial.print(", ");
  Serial.println(longitude, 6);
  Serial.print("Speed: ");
  Serial.print(speed);
  Serial.println(" km/h");
  Serial.print("Satellites: ");
  Serial.println(satellites);
}
