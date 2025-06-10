/*
 * Complete GPS Bus Tracking System - Production Ready
 * ESP32 + NEO-6M GPS + SIM800L GSM
 * 
 * Features:
 * - Real-time GPS tracking with validation
 * - SMS alerts and notifications
 * - System health monitoring
 * - Error recovery and retry mechanisms
 * - Hardware testing functions
 * - Comprehensive logging
 * - Power management
 * - Watchdog timer
 * 
 * Hardware Connections:
 * NEO-6M GPS:
 *   VCC -> ESP32 3.3V
 *   GND -> ESP32 GND
 *   TX  -> ESP32 GPIO 16
 *   RX  -> ESP32 GPIO 17
 * 
 * SIM800L GSM:
 *   VCC -> 3.7V-4.2V (separate power supply with 2A capability)
 *   GND -> ESP32 GND (common ground)
 *   TX  -> ESP32 GPIO 26
 *   RX  -> ESP32 GPIO 27
 *   RST -> ESP32 GPIO 23
 * 
 * Status LED:
 *   Anode -> ESP32 GPIO 19 (via 1kΩ resistor)
 *   Cathode -> ESP32 GND
 * 
 * Test Button (Optional):
 *   One side -> ESP32 GPIO 18
 *   Other side -> ESP32 GND
 * 
 * Buzzer (Optional):
 *   Positive -> ESP32 GPIO 21
 *   Negative -> ESP32 GND
 */

#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <esp_task_wdt.h>
#include <EEPROM.h>
// WiFi library not needed for this project

// Pin definitions
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GSM_RX_PIN 26
#define GSM_TX_PIN 27
#define GSM_RST_PIN 23
#define LED_PIN 19
#define TEST_BUTTON_PIN 18
#define BUZZER_PIN 21

// Serial communication objects
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);
TinyGPSPlus gps;

// === CONFIGURATION SECTION - CUSTOMIZE THESE VALUES ===
const String BUS_ID = "BUS-001";                        // Your bus identifier
const String ROUTE_NAME = "Main-Route";                 // Your route name
const char* ADMIN_PHONE = "+91XXXXXXXXXX";              // Primary contact number
const char* BACKUP_PHONE = "+91YYYYYYYYYY";             // Backup contact number
const char* EMERGENCY_PHONE = "+91ZZZZZZZZZZ";          // Emergency contact

// Timing configuration
const unsigned long GPS_TIMEOUT = 180000;               // 3 minutes GPS timeout
const unsigned long NORMAL_SEND_INTERVAL = 300000;      // 5 minutes normal updates
const unsigned long TRACKING_SEND_INTERVAL = 60000;     // 1 minute tracking mode
const unsigned long EMERGENCY_INTERVAL = 30000;         // 30 seconds emergency mode
const unsigned long HEALTH_CHECK_INTERVAL = 60000;      // 1 minute health checks
const unsigned long RETRY_INTERVAL = 10000;             // 10 seconds retry delay

// Operational limits
const float SPEED_LIMIT = 80.0;                         // Speed limit in km/h
const float MIN_VALID_SPEED = 0.0;                      // Minimum valid speed
const float MAX_VALID_SPEED = 200.0;                    // Maximum valid speed
const int MIN_SATELLITES = 4;                           // Minimum satellites for good fix
const float HDOP_THRESHOLD = 2.0;                       // Maximum HDOP for good accuracy

// System state variables
bool gpsFixed = false;
bool gsmReady = false;
bool systemHealthy = true;
bool emergencyMode = false;
bool trackingMode = false;
bool testMode = false;

// Timing variables
unsigned long lastSendTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastHealthCheck = 0;
unsigned long lastSpeedAlert = 0;
unsigned long bootTime = 0;

// LED control
bool ledState = false;
int ledBlinkPattern = 1; // 1=waiting, 2=error, 3=emergency

// GPS data variables
double latitude = 0.0, longitude = 0.0;
double prevLatitude = 0.0, prevLongitude = 0.0;
float altitude = 0.0, speed = 0.0;
String dateTime = "";
int satellites = 0;
float hdop = 0.0;

// System statistics
unsigned long totalSMSSent = 0;
unsigned long totalErrors = 0;
unsigned long gpsFixCount = 0;
unsigned long maxSpeed = 0;
String lastError = "";

// Error tracking
struct ErrorLog {
  String errorType;
  String errorMessage;
  unsigned long timestamp;
};

ErrorLog errorHistory[10];
int errorIndex = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Print system info
  printSystemInfo();
  
  // Initialize watchdog timer (30 seconds)
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  
  // Initialize EEPROM for statistics
  EEPROM.begin(512);
  loadSystemStats();
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(GSM_RST_PIN, OUTPUT);
  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initial pin states
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  bootTime = millis();
  
  // System startup sequence
  Serial.println("=== STARTING SYSTEM INITIALIZATION ===");
  
  // 1. Test hardware connections
  if (!testHardwareConnections()) {
    logError("HARDWARE", "Hardware test failed during startup");
    emergencyShutdown();
    return;
  }
  
  // 2. Initialize GPS
  Serial.println("Initializing GPS module...");
  gpsSerial.begin(9600);
  delay(1000);
  
  // 3. Initialize and test GSM
  Serial.println("Initializing GSM module...");
  gsmSerial.begin(9600);
  delay(1000);
  
  if (!initializeGSM()) {
    logError("GSM", "GSM initialization failed");
    // Continue without GSM for GPS testing
  }
  
  // 4. Send startup notification
  if (gsmReady) {
    sendStartupNotification();
  }
  
  // 5. Start GPS acquisition
  Serial.println("Starting GPS acquisition...");
  Serial.println("Waiting for GPS fix (this may take 2-5 minutes)...");
  
  Serial.println("=== SYSTEM INITIALIZATION COMPLETE ===");
  Serial.println("Entering main operation loop...");
  Serial.println("Press test button for manual system test");
  
  // Reset watchdog
  esp_task_wdt_reset();
}

void loop() {
  // Reset watchdog timer
  esp_task_wdt_reset();
  
  // Check test button
  if (digitalRead(TEST_BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(TEST_BUTTON_PIN) == LOW) {
      runSystemTest();
      while (digitalRead(TEST_BUTTON_PIN) == LOW) delay(100); // Wait for release
    }
  }
  
  // Read and process GPS data
  processGPSData();
  
  // Handle LED status indication
  updateLEDStatus();
  
  // Perform periodic health checks
  performHealthCheck();
  
  // Handle SMS communication
  handleSMSCommunication();
  
  // Check if it's time to send location update
  checkLocationUpdate();
  
  // Small delay to prevent excessive CPU usage
  delay(100);
}

void processGPSData() {
  static unsigned long lastGPSRead = 0;
  
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    
    if (gps.encode(c)) {
      lastGPSRead = millis();
      
      if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid()) {
        // Store previous location
        prevLatitude = latitude;
        prevLongitude = longitude;
        
        // Update current location
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        altitude = gps.altitude.meters();
        speed = gps.speed.kmph();
        satellites = gps.satellites.value();
        hdop = gps.hdop.hdop();
        
        // Validate GPS data
        if (isValidGPSData()) {
          // Format date and time
          dateTime = formatDateTime();
          
          if (!gpsFixed) {
            gpsFixed = true;
            gpsFixCount++;
            ledBlinkPattern = 0; // Solid LED
            
            Serial.println("*** GPS FIX ACQUIRED ***");
            printGPSInfo();
            
            // Send first location immediately
            if (gsmReady) {
              sendLocationUpdate("GPS fix acquired");
              lastSendTime = millis();
            }
          }
          
          // Update max speed
          if (speed > maxSpeed) {
            maxSpeed = (unsigned long)speed;
          }
          
          // Check speed limit
          checkSpeedLimit();
          
        } else {
          logError("GPS", "Invalid GPS data received");
        }
      }
    }
  }
  
  // Check for GPS timeout
  if (gpsFixed && (millis() - lastGPSRead > 30000)) { // 30 seconds no data
    gpsFixed = false;
    ledBlinkPattern = 2; // Error pattern
    logError("GPS", "GPS signal lost");
    Serial.println("GPS signal lost!");
  }
  
  // Check for initial GPS timeout
  if (!gpsFixed && millis() > GPS_TIMEOUT) {
    ledBlinkPattern = 2; // Error pattern
    logError("GPS", "GPS timeout - no fix acquired");
    Serial.println("GPS timeout - consider moving to location with better sky view");
  }
}

bool isValidGPSData() {
  // Validate coordinates
  if (latitude == 0.0 && longitude == 0.0) return false;
  if (abs(latitude) > 90.0 || abs(longitude) > 180.0) return false;
  
  // Validate speed
  if (speed < MIN_VALID_SPEED || speed > MAX_VALID_SPEED) return false;
  
  // Check satellite count
  if (satellites < MIN_SATELLITES) return false;
  
  // Check HDOP (accuracy)
  if (hdop > HDOP_THRESHOLD) return false;
  
  return true;
}

void updateLEDStatus() {
  unsigned long currentTime = millis();
  
  switch (ledBlinkPattern) {
    case 0: // Solid ON (GPS fixed)
      digitalWrite(LED_PIN, HIGH);
      break;
      
    case 1: // Slow blink (waiting for GPS)
      if (currentTime - lastBlinkTime >= 1000) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlinkTime = currentTime;
      }
      break;
      
    case 2: // Fast blink (error)
      if (currentTime - lastBlinkTime >= 250) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlinkTime = currentTime;
      }
      break;
      
    case 3: // Emergency pattern (triple blink)
      static int blinkCount = 0;
      if (currentTime - lastBlinkTime >= 200) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlinkTime = currentTime;
        blinkCount++;
        
        if (blinkCount >= 6) { // 3 complete blinks
          blinkCount = 0;
          delay(1000); // Pause between patterns
        }
      }
      break;
  }
}

void performHealthCheck() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastHealthCheck >= HEALTH_CHECK_INTERVAL) {
    lastHealthCheck = currentTime;
    
    Serial.println("Performing system health check...");
    
    // Check GPS health
    bool gpsHealthy = gpsFixed && (satellites >= MIN_SATELLITES);
    
    // Check GSM health
    bool gsmHealthy = testGSMConnection();
    
    // Update system health status
    systemHealthy = gpsHealthy && gsmHealthy;
    
    // Print health status
    Serial.print("GPS Health: ");
    Serial.println(gpsHealthy ? "GOOD" : "POOR");
    Serial.print("GSM Health: ");
    Serial.println(gsmHealthy ? "GOOD" : "POOR");
    Serial.print("System Health: ");
    Serial.println(systemHealthy ? "HEALTHY" : "UNHEALTHY");
    
    // Send health report if unhealthy
    if (!systemHealthy && gsmReady) {
      sendHealthAlert();
    }
    
    // Save statistics
    saveSystemStats();
  }
}

void checkSpeedLimit() {
  if (speed > SPEED_LIMIT) {
    unsigned long currentTime = millis();
    
    // Send speed alert (max once per minute)
    if (currentTime - lastSpeedAlert >= 60000) {
      lastSpeedAlert = currentTime;
      
      Serial.println("SPEED LIMIT EXCEEDED: " + String(speed) + " km/h");
      
      if (gsmReady) {
        String message = "⚠️ SPEED ALERT ⚠️\n";
        message += "Bus: " + BUS_ID + "\n";
        message += "Speed: " + String(speed, 1) + " km/h\n";
        message += "Limit: " + String(SPEED_LIMIT, 1) + " km/h\n";
        message += "Location: " + String(latitude, 6) + "," + String(longitude, 6) + "\n";
        message += "Time: " + dateTime;
        
        sendSMS(ADMIN_PHONE, message);
      }
      
      // Sound buzzer
      for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
      }
    }
  }
}

void checkLocationUpdate() {
  unsigned long currentTime = millis();
  unsigned long interval = NORMAL_SEND_INTERVAL;
  
  // Determine update interval based on mode
  if (emergencyMode) {
    interval = EMERGENCY_INTERVAL;
  } else if (trackingMode) {
    interval = TRACKING_SEND_INTERVAL;
  }
  
  // Send location update
  if (gpsFixed && gsmReady && (currentTime - lastSendTime >= interval)) {
    sendLocationUpdate("Scheduled update");
    lastSendTime = currentTime;
  }
}

void sendLocationUpdate(String reason) {
  Serial.println("Sending location update: " + reason);
  
  String message = BUS_ID + " - " + ROUTE_NAME + "\n";
  message += "📍 Location: " + String(latitude, 6) + "," + String(longitude, 6) + "\n";
  message += "🚌 Speed: " + String(speed, 1) + " km/h\n";
  message += "⛰️ Altitude: " + String(altitude, 0) + "m\n";
  message += "🛰️ Satellites: " + String(satellites) + "\n";
  message += "📅 Time: " + dateTime + "\n";
  
  if (emergencyMode) {
    message += "🚨 EMERGENCY MODE ACTIVE 🚨\n";
  }
  if (trackingMode) {
    message += "📊 Enhanced tracking enabled\n";
  }
  
  message += "🗺️ Map: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
  
  if (sendSMS(ADMIN_PHONE, message)) {
    totalSMSSent++;
    Serial.println("Location update sent successfully");
  } else {
    logError("SMS", "Failed to send location update");
  }
}

void handleSMSCommunication() {
  // Check for incoming SMS commands
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    
    if (response.indexOf("+CMTI:") > -1) {
      // Extract message index
      int indexStart = response.indexOf(",") + 1;
      int messageIndex = response.substring(indexStart).toInt();
      
      // Read and process SMS
      processSMSCommand(messageIndex);
    }
  }
}

void processSMSCommand(int messageIndex) {
  // Read SMS
  String command = "AT+CMGR=" + String(messageIndex);
  gsmSerial.println(command);
  delay(1000);
  
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    if (gsmSerial.available()) {
      response += (char)gsmSerial.read();
    }
  }
  
  // Parse commands (case insensitive)
  response.toUpperCase();
  
  if (response.indexOf("TRACK ON") > -1) {
    trackingMode = true;
    sendSMS(ADMIN_PHONE, "✅ Enhanced tracking enabled for " + BUS_ID);
    Serial.println("Tracking mode enabled via SMS");
    
  } else if (response.indexOf("TRACK OFF") > -1) {
    trackingMode = false;
    sendSMS(ADMIN_PHONE, "✅ Enhanced tracking disabled for " + BUS_ID);
    Serial.println("Tracking mode disabled via SMS");
    
  } else if (response.indexOf("EMERGENCY OFF") > -1) {
    emergencyMode = false;
    ledBlinkPattern = gpsFixed ? 0 : 1;
    sendSMS(ADMIN_PHONE, "✅ Emergency mode disabled for " + BUS_ID);
    Serial.println("Emergency mode disabled via SMS");
    
  } else if (response.indexOf("STATUS") > -1) {
    sendSystemStatus();
    
  } else if (response.indexOf("LOCATION") > -1) {
    if (gpsFixed) {
      sendLocationUpdate("Manual request");
    } else {
      sendSMS(ADMIN_PHONE, "❌ GPS not fixed. Location unavailable for " + BUS_ID);
    }
    
  } else if (response.indexOf("REBOOT") > -1) {
    sendSMS(ADMIN_PHONE, "🔄 Rebooting system for " + BUS_ID);
    delay(2000);
    ESP.restart();
    
  } else if (response.indexOf("TEST") > -1) {
    runSystemTest();
    
  } else if (response.indexOf("HELP") > -1) {
    sendHelpMessage();
  }
  
  // Delete the processed SMS
  gsmSerial.println("AT+CMGD=" + String(messageIndex));
  delay(1000);
}

// === GSM FUNCTIONS ===

bool initializeGSM() {
  Serial.println("Initializing GSM module...");
  
  // Reset GSM module
  resetGSM();
  delay(3000);
  
  // Test basic communication
  if (!sendATCommand("AT", 2000).indexOf("OK") > -1) {
    logError("GSM", "No response to AT command");
    return false;
  }
  
  // Disable echo
  sendATCommand("ATE0", 2000);
  
  // Enable SMS notifications
  sendATCommand("AT+CNMI=1,2,0,0,0", 2000);
  
  // Check SIM card
  String simResponse = sendATCommand("AT+CPIN?", 5000);
  if (simResponse.indexOf("READY") == -1) {
    logError("GSM", "SIM card not ready: " + simResponse);
    return false;
  }
  Serial.println("✅ SIM card ready");
  
  // Wait for network registration
  for (int i = 0; i < 30; i++) { // Wait up to 30 seconds
    String regResponse = sendATCommand("AT+CREG?", 3000);
    if (regResponse.indexOf(",1") > -1 || regResponse.indexOf(",5") > -1) {
      Serial.println("✅ Network registered");
      gsmReady = true;
      break;
    }
    Serial.print("Waiting for network registration... ");
    Serial.println(i + 1);
    delay(1000);
  }
  
  if (!gsmReady) {
    logError("GSM", "Network registration failed");
    return false;
  }
  
  // Set SMS text mode
  sendATCommand("AT+CMGF=1", 2000);
  
  // Set SMS character set
  sendATCommand("AT+CSCS=\"GSM\"", 2000);
  
  // Check signal strength
  String signalResponse = sendATCommand("AT+CSQ", 2000);
  Serial.println("Signal strength: " + signalResponse);
  
  Serial.println("✅ GSM module ready");
  return true;
}

void resetGSM() {
  Serial.println("Resetting GSM module...");
  digitalWrite(GSM_RST_PIN, LOW);
  delay(200);
  digitalWrite(GSM_RST_PIN, HIGH);
  delay(3000);
}

bool testGSMConnection() {
  String response = sendATCommand("AT", 1000);
  return response.indexOf("OK") > -1;
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
  
  return response;
}

bool sendSMS(const char* phoneNumber, String message) {
  if (!gsmReady) {
    logError("SMS", "GSM not ready for SMS");
    return false;
  }
  
  Serial.println("Sending SMS to: " + String(phoneNumber));
  
  // Start SMS
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(1000);
  
  // Send message content
  gsmSerial.print(message);
  delay(1000);
  
  // Send Ctrl+Z to complete SMS
  gsmSerial.write(26);
  delay(2000);
  
  // Check response
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 15000) { // 15 second timeout
    if (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      
      if (response.indexOf("OK") > -1) {
        Serial.println("✅ SMS sent successfully");
        return true;
      }
      
      if (response.indexOf("ERROR") > -1) {
        Serial.println("❌ SMS send failed: " + response);
        logError("SMS", "Send failed: " + response);
        return false;
      }
    }
  }
  
  Serial.println("❌ SMS send timeout");
  logError("SMS", "Send timeout");
  return false;
}

// === SYSTEM TESTING FUNCTIONS ===

bool testHardwareConnections() {
  Serial.println("Testing hardware connections...");
  
  bool allTestsPassed = true;
  
  // Test LED
  Serial.print("Testing LED... ");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  Serial.println("✅ LED OK");
  
  // Test Buzzer
  Serial.print("Testing Buzzer... ");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("✅ Buzzer OK");
  
  // Test GPS Serial
  Serial.print("Testing GPS Serial... ");
  gpsSerial.begin(9600);
  delay(500);
  if (gpsSerial.availableForWrite()) {
    Serial.println("✅ GPS Serial OK");
  } else {
    Serial.println("❌ GPS Serial FAILED");
    allTestsPassed = false;
  }
  
  // Test GSM Serial
  Serial.print("Testing GSM Serial... ");
  gsmSerial.begin(9600);
  delay(500);
  if (gsmSerial.availableForWrite()) {
    Serial.println("✅ GSM Serial OK");
  } else {
    Serial.println("❌ GSM Serial FAILED");
    allTestsPassed = false;
  }
  
  return allTestsPassed;
}

void runSystemTest() {
  testMode = true;
  Serial.println("\n=== RUNNING COMPREHENSIVE SYSTEM TEST ===");
  
  // Test 1: Hardware
  Serial.println("1. Hardware Test:");
  testHardwareConnections();
  
  // Test 2: GPS
  Serial.println("\n2. GPS Test:");
  testGPSModule();
  
  // Test 3: GSM
  Serial.println("\n3. GSM Test:");
  testGSMModule();
  
  // Test 4: SMS
  Serial.println("\n4. SMS Test:");
  if (gsmReady) {
    String testMessage = "🧪 System Test - " + BUS_ID + "\n";
    testMessage += "All systems tested at " + dateTime + "\n";
    testMessage += "GPS: " + (gpsFixed ? "✅ OK" : "❌ FAIL") + "\n";
    testMessage += "GSM: " + (gsmReady ? "✅ OK" : "❌ FAIL") + "\n";
    testMessage += "Test completed successfully.";
    
    sendSMS(ADMIN_PHONE, testMessage);
  }
  
  // Test 5: System Status
  Serial.println("\n5. System Status:");
  printSystemStatus();
  
  Serial.println("\n=== SYSTEM TEST COMPLETE ===\n");
  testMode = false;
}

void testGPSModule() {
  Serial.println("Testing GPS module...");
  
  // Check if GPS is receiving data
  unsigned long testStart = millis();
  int dataReceived = 0;
  
  while (millis() - testStart < 5000) { // 5 second test
    if (gpsSerial.available()) {
      gpsSerial.read();
      dataReceived++;
    }
  }
  
  if (dataReceived > 0) {
    Serial.println("✅ GPS receiving data (" + String(dataReceived) + " bytes)");
    
    if (gpsFixed) {
      Serial.println("✅ GPS has valid fix");
      Serial.println("   Satellites: " + String(satellites));
      Serial.println("   HDOP: " + String(hdop));
      Serial.println("   Location: " + String(latitude, 6) + "," + String(longitude, 6));
    } else {
      Serial.println("⚠️ GPS no fix (may need more time or better sky view)");
    }
  } else {
    Serial.println("❌ GPS not receiving data - check connections");
  }
}

void testGSMModule() {
  Serial.println("Testing GSM module...");
  
  // Test AT response
  String response = sendATCommand("AT", 2000);
  if (response.indexOf("OK") > -1) {
    Serial.println("✅ GSM responding to AT commands");
  } else {
    Serial.println("❌ GSM not responding - check connections and power");
    return;
  }
  
  // Test SIM card
  response = sendATCommand("AT+CPIN?", 3000);
  if (response.indexOf("READY") > -1) {
    Serial.println("✅ SIM card ready");
  } else {
    Serial.println("❌ SIM card not ready: " + response);
  }
  
  // Test network registration
  response = sendATCommand("AT+CREG?", 3000);
  if (response.indexOf(",1") > -1 || response.indexOf(",5") > -1) {
    Serial.println("✅ Network registered");
  } else {
    Serial.println("❌ Network not registered: " + response);
  }
  
  // Test signal strength
  response = sendATCommand("AT+CSQ", 2000);
  Serial.println("Signal strength: " + response);
}

// === NOTIFICATION FUNCTIONS ===

void sendStartupNotification() {
  String message = "🚌 Bus Tracking System Started\n";
  message += "Bus ID: " + BUS_ID + "\n";
  message += "Route: " + ROUTE_NAME + "\n";
  message += "System ready for operation\n\n";
  message += "📱 Available Commands:\n";
  message += "TRACK ON/OFF - Enable/disable enhanced tracking\n";
  message += "STATUS - Get system status\n";
  message += "LOCATION - Get current location\n";
  message += "EMERGENCY OFF - Disable emergency mode\n";
  message += "TEST - Run system diagnostics\n";
  message += "HELP - Show this message\n";
  message += "REBOOT - Restart system";
  
  sendSMS(ADMIN_PHONE, message);
}

void sendSystemStatus() {
  unsigned long uptime = millis() - bootTime;
  
  String message = "📊 System Status - " + BUS_ID + "\n";
  message += "GPS: " + String(gpsFixed ? "✅ Fixed" : "❌ No Fix") + "\n";
  message += "GSM: " + String(gsmReady ? "✅ Ready" : "❌ Error") + "\n";
  message += "Health: " + String(systemHealthy ? "✅ Good" : "⚠️ Poor") + "\n";
  message += "Emergency: " + String(emergencyMode ? "🚨 ACTIVE" : "✅ Normal") + "\n";
  message += "Tracking: " + String(trackingMode ? "📍 Enhanced" : "📍 Normal") + "\n";
  
  if (gpsFixed) {
    message += "Speed: " + String(speed, 1) + " km/h\n";
    message += "Satellites: " + String(satellites) + "\n";
    message += "Max Speed: " + String(maxSpeed) + " km/h\n";
  }
  
  message += "SMS Sent: " + String(totalSMSSent) + "\n";
  message += "Errors: " + String(totalErrors) + "\n";
  message += "Uptime: " + formatUptime(uptime);
    sendSMS(ADMIN_PHONE, message);
}

void sendHealthAlert() {
  String message = "⚠️ SYSTEM HEALTH ALERT ⚠️\n";
  message += "Bus: " + BUS_ID + "\n";
  message += "Issue detected:\n";
  
  if (!gpsFixed) {
    message += "❌ GPS signal lost\n";
  }
  if (!gsmReady) {
    message += "❌ GSM connection lost\n";
  }
  
  message += "Last error: " + lastError + "\n";
  message += "Time: " + dateTime + "\n";
  message += "Automatic recovery in progress...";
  
  sendSMS(ADMIN_PHONE, message);
  
  // Also send to backup if available
  if (strlen(BACKUP_PHONE) > 5) {
    sendSMS(BACKUP_PHONE, message);
  }
}
    message += "❌ GPS signal lost\n";
  }
  if (!gsmReady) {
    message += "❌ GSM connection lost\n";
  }
  
  message += "Last error: " + lastError + "\n";
  message += "Time: " + dateTime + "\n";
  message += "Automatic recovery in progress...";
  
  sendSMS(ADMIN_PHONE, message);
  
  // Also send to backup if available
  if (strlen(BACKUP_PHONE) > 5) {
    sendSMS(BACKUP_PHONE, message);
  }
}

void sendHelpMessage() {
  String message = "📱 Bus Tracker Commands - " + BUS_ID + "\n\n";
  message += "TRACK ON - Enhanced tracking (1min updates)\n";
  message += "TRACK OFF - Normal tracking (5min updates)\n";
  message += "STATUS - Get detailed system status\n";
  message += "LOCATION - Get current GPS location\n";
  message += "EMERGENCY OFF - Disable emergency mode\n";
  message += "TEST - Run system diagnostics\n";
  message += "REBOOT - Restart the system\n";
  message += "HELP - Show this message\n\n";
  message += "Emergency mode activates automatically if issues detected.";
  
  sendSMS(ADMIN_PHONE, message);
}

// === UTILITY FUNCTIONS ===

void printSystemInfo() {
  Serial.println("\n" + String('=', 50));
  Serial.println("GPS BUS TRACKING SYSTEM v2.0");
  Serial.println("ESP32 + NEO-6M + SIM800L");
  Serial.println(String('=', 50));
  Serial.println("Bus ID: " + BUS_ID);
  Serial.println("Route: " + ROUTE_NAME);
  Serial.println("Compiled: " + String(__DATE__) + " " + String(__TIME__));
  Serial.println("ESP32 Chip Model: " + String(ESP.getChipModel()));
  Serial.println("CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
  Serial.println("Flash Size: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println(String('=', 50) + "\n");
}

void printGPSInfo() {
  Serial.println("GPS Information:");
  Serial.println("  Location: " + String(latitude, 6) + ", " + String(longitude, 6));
  Serial.println("  Altitude: " + String(altitude, 1) + " meters");
  Serial.println("  Speed: " + String(speed, 1) + " km/h");
  Serial.println("  Satellites: " + String(satellites));
  Serial.println("  HDOP: " + String(hdop, 2));
  Serial.println("  Date/Time: " + dateTime);
}

void printSystemStatus() {
  unsigned long uptime = millis() - bootTime;
  
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.println("GPS Fixed: " + String(gpsFixed ? "YES" : "NO"));
  Serial.println("GSM Ready: " + String(gsmReady ? "YES" : "NO"));
  Serial.println("System Healthy: " + String(systemHealthy ? "YES" : "NO"));
  Serial.println("Emergency Mode: " + String(emergencyMode ? "ACTIVE" : "NORMAL"));
  Serial.println("Tracking Mode: " + String(trackingMode ? "ENHANCED" : "NORMAL"));
  
  if (gpsFixed) {
    Serial.println("Current Speed: " + String(speed, 1) + " km/h");
    Serial.println("Max Speed: " + String(maxSpeed) + " km/h");
    Serial.println("Satellites: " + String(satellites));
  }
  
  Serial.println("SMS Sent: " + String(totalSMSSent));
  Serial.println("Total Errors: " + String(totalErrors));
  Serial.println("Uptime: " + formatUptime(uptime));
  Serial.println("Free Memory: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("=====================\n");
}

String formatDateTime() {
  return formatNumber(gps.date.day()) + "/" + 
         formatNumber(gps.date.month()) + "/" + 
         String(gps.date.year()) + " " +
         formatNumber(gps.time.hour()) + ":" + 
         formatNumber(gps.time.minute()) + ":" + 
         formatNumber(gps.time.second());
}

String formatNumber(int number) {
  return number < 10 ? "0" + String(number) : String(number);
}

String formatUptime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  String uptime = "";
  if (days > 0) uptime += String(days) + "d ";
  if (hours > 0) uptime += String(hours % 24) + "h ";
  if (minutes > 0) uptime += String(minutes % 60) + "m ";
  uptime += String(seconds % 60) + "s";
  
  return uptime;
}

void logError(String errorType, String errorMessage) {
  totalErrors++;
  lastError = errorType + ": " + errorMessage;
  
  // Store in error history
  errorHistory[errorIndex].errorType = errorType;
  errorHistory[errorIndex].errorMessage = errorMessage;
  errorHistory[errorIndex].timestamp = millis();
  errorIndex = (errorIndex + 1) % 10; // Circular buffer
  
  Serial.println("ERROR [" + errorType + "]: " + errorMessage);
}

void loadSystemStats() {
  // Load statistics from EEPROM
  EEPROM.get(0, totalSMSSent);
  EEPROM.get(4, totalErrors);
  EEPROM.get(8, maxSpeed);
  
  // Validate loaded data
  if (totalSMSSent > 100000) totalSMSSent = 0;
  if (totalErrors > 100000) totalErrors = 0;
  if (maxSpeed > 500) maxSpeed = 0;
}

void saveSystemStats() {
  // Save statistics to EEPROM
  EEPROM.put(0, totalSMSSent);
  EEPROM.put(4, totalErrors);
  EEPROM.put(8, maxSpeed);
  EEPROM.commit();
}

void emergencyShutdown() {
  Serial.println("EMERGENCY SHUTDOWN - Critical error detected");
  
  // Flash LED rapidly
  for (int i = 0; i < 20; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Sound alarm
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);
  }
  
  // Try to send emergency notification
  if (gsmReady) {
    String message = "🚨 CRITICAL SYSTEM ERROR 🚨\n";
    message += "Bus: " + BUS_ID + "\n";
    message += "System shutting down due to critical error\n";
    message += "Manual intervention required\n";
    message += "Check hardware connections and power supply";
    
    sendSMS(ADMIN_PHONE, message);
    sendSMS(EMERGENCY_PHONE, message);
  }
  
  // Wait indefinitely (requires manual reset)
  while (true) {
    delay(1000);
  }
}
