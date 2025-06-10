/*
 * GPS Bus Tracking System
 * ESP32 + NEO-6M GPS + SIM800L GSM
 * 
 * Hardware Connections:
 * NEO-6M GPS:
 *   VCC -> ESP32 3.3V
 *   GND -> ESP32 GND
 *   TX  -> ESP32 GPIO 16
 *   RX  -> ESP32 GPIO 17
 * 
 * SIM800L GSM:
 *   VCC -> 3.7V-4.2V (separate power supply)
 *   GND -> ESP32 GND (common ground)
 *   TX  -> ESP32 GPIO 26
 *   RX  -> ESP32 GPIO 27
 *   RST -> ESP32 GPIO 23
 * 
 * LED:
 *   Anode -> ESP32 GPIO 19 (via 1kΩ resistor)
 *   Cathode -> ESP32 GND
 */

#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Pin definitions
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GSM_RX_PIN 26
#define GSM_TX_PIN 27
#define GSM_RST_PIN 23
#define LED_PIN 19

// Serial communication objects
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);
TinyGPSPlus gps;

// Configuration
const char* PHONE_NUMBER = "+91XXXXXXXXXX"; // Replace with actual phone number
const unsigned long GPS_TIMEOUT = 120000; // 2 minutes GPS timeout
const unsigned long SEND_INTERVAL = 60000; // Send GPS data every 60 seconds
const unsigned long RETRY_INTERVAL = 10000; // Retry after 10 seconds on failure

// System state variables
bool gpsFixed = false;
bool gsmReady = false;
unsigned long lastSendTime = 0;
unsigned long lastBlinkTime = 0;
bool ledState = false;

// GPS data variables
double latitude, longitude;
String dateTime;
float altitude, speed;

void setup() {
  Serial.begin(115200);
  Serial.println("GPS Bus Tracking System Starting...");
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(GSM_RST_PIN, OUTPUT);
  
  // Initialize LED (OFF initially)
  digitalWrite(LED_PIN, LOW);
  
  // Initialize GPS
  gpsSerial.begin(9600);
  Serial.println("GPS module initialized");
  
  // Initialize GSM
  gsmSerial.begin(9600);
  Serial.println("GSM module initialized");
  
  // Reset GSM module
  resetGSM();
  delay(2000);
  
  // Initialize GSM
  initializeGSM();
  
  Serial.println("System initialization complete");
  Serial.println("Waiting for GPS fix...");
}

void loop() {
  // Read GPS data
  readGPSData();
  
  // Handle LED blinking when waiting for GPS fix
  if (!gpsFixed) {
    blinkLED();
  }
  
  // Check if it's time to send GPS data
  unsigned long currentTime = millis();
  if (gpsFixed && gsmReady && (currentTime - lastSendTime >= SEND_INTERVAL)) {
    sendGPSData();
    lastSendTime = currentTime;
  }
  
  // Small delay to prevent watchdog issues
  delay(100);
}

void readGPSData() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (gps.encode(c)) {
      if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid()) {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        altitude = gps.altitude.meters();
        speed = gps.speed.kmph();
        
        // Format date and time
        dateTime = String(gps.date.day()) + "/" + 
                  String(gps.date.month()) + "/" + 
                  String(gps.date.year()) + " " +
                  String(gps.time.hour()) + ":" + 
                  String(gps.time.minute()) + ":" + 
                  String(gps.time.second());
        
        if (!gpsFixed) {
          gpsFixed = true;
          digitalWrite(LED_PIN, HIGH); // Turn LED ON when GPS is fixed
          Serial.println("GPS Fix Acquired!");
          Serial.print("Location: ");
          Serial.print(latitude, 6);
          Serial.print(", ");
          Serial.println(longitude, 6);
          Serial.print("Date/Time: ");
          Serial.println(dateTime);
          Serial.print("Altitude: ");
          Serial.print(altitude);
          Serial.println(" meters");
          Serial.print("Speed: ");
          Serial.print(speed);
          Serial.println(" km/h");
        }
      }
    }
  }
  
  // Check for GPS timeout
  if (!gpsFixed && millis() > GPS_TIMEOUT) {
    Serial.println("GPS timeout - no fix acquired");
    digitalWrite(LED_PIN, LOW); // Turn LED OFF on error
  }
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
    // Try to register
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

void sendGPSData() {
  if (!gpsFixed || !gsmReady) {
    Serial.println("Cannot send SMS - GPS or GSM not ready");
    digitalWrite(LED_PIN, LOW); // Turn LED OFF on error
    return;
  }
  
  Serial.println("Sending GPS data via SMS...");
  
  // Prepare SMS message
  String message = "Bus Location:\n";
  message += "Lat: " + String(latitude, 6) + "\n";
  message += "Lng: " + String(longitude, 6) + "\n";
  message += "Alt: " + String(altitude, 1) + "m\n";
  message += "Speed: " + String(speed, 1) + "km/h\n";
  message += "Time: " + dateTime + "\n";
  message += "Maps: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
  
  // Send SMS
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(PHONE_NUMBER);
  gsmSerial.println("\"");
  delay(1000);
  
  gsmSerial.print(message);
  delay(1000);
  
  gsmSerial.write(26); // Ctrl+Z to send SMS
  delay(1000);
  
  // Check response
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) { // Wait up to 10 seconds for response
    if (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      if (response.indexOf("OK") > -1) {
        Serial.println("SMS sent successfully!");
        digitalWrite(LED_PIN, HIGH); // Keep LED ON for successful send
        return;
      }
      if (response.indexOf("ERROR") > -1) {
        Serial.println("SMS send failed!");
        digitalWrite(LED_PIN, LOW); // Turn LED OFF on error
        return;
      }
    }
  }
  
  Serial.println("SMS send timeout!");
  digitalWrite(LED_PIN, LOW); // Turn LED OFF on timeout
}

void printSystemStatus() {
  Serial.println("=== System Status ===");
  Serial.print("GPS Fixed: ");
  Serial.println(gpsFixed ? "YES" : "NO");
  Serial.print("GSM Ready: ");
  Serial.println(gsmReady ? "YES" : "NO");
  if (gpsFixed) {
    Serial.print("GPS Location: ");
    Serial.print(latitude, 6);
    Serial.print(", ");
    Serial.println(longitude, 6);
  }
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  Serial.println("===================");
}
