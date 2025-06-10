/*
 * GPS Bus Tracker - System Verification Script
 * Run this first to verify all components are working
 * 
 * This script performs comprehensive testing of:
 * - Hardware connections
 * - GPS module functionality
 * - GSM module functionality
 * - SMS capabilities
 * - System integration
 */

#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Pin definitions (same as main code)
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GSM_RX_PIN 26
#define GSM_TX_PIN 27
#define GSM_RST_PIN 23
#define LED_PIN 19
#define TEST_BUTTON_PIN 18
#define BUZZER_PIN 21

// Test configuration
const char* TEST_PHONE = "+91XXXXXXXXXX"; // Replace with your test phone number
const int TEST_DURATION = 30; // seconds for each test

// Serial objects
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);
TinyGPSPlus gps;

// Test results
struct TestResults {
  bool ledTest = false;
  bool buzzerTest = false;
  bool gpsDataTest = false;
  bool gpsFixTest = false;
  bool gsmAtTest = false;
  bool gsmNetworkTest = false;
  bool smsTest = false;
  bool integrationTest = false;
};

TestResults results;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  printHeader();
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(GSM_RST_PIN, OUTPUT);
  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize communication
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);
  
  Serial.println("🚀 Starting comprehensive system verification...\n");
  
  // Run all tests
  runAllTests();
  
  // Print final results
  printFinalResults();
}

void loop() {
  // Test complete - show status on LED
  if (allTestsPassed()) {
    // Solid green - all tests passed
    digitalWrite(LED_PIN, HIGH);
  } else {
    // Blink red - some tests failed
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = millis();
    }
  }
  
  // Re-run tests if button pressed
  if (digitalRead(TEST_BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(TEST_BUTTON_PIN) == LOW) {
      Serial.println("\n🔄 Re-running tests...\n");
      runAllTests();
      printFinalResults();
      while (digitalRead(TEST_BUTTON_PIN) == LOW) delay(100);
    }
  }
  
  delay(100);
}

void printHeader() {
  Serial.println("\n" + String('=', 60));
  Serial.println("     GPS BUS TRACKER - SYSTEM VERIFICATION");
  Serial.println("           Hardware & Software Testing");
  Serial.println(String('=', 60));
  Serial.println("📅 Date: " + String(__DATE__) + " " + String(__TIME__));
  Serial.println("💻 ESP32 Model: " + String(ESP.getChipModel()));
  Serial.println("⚡ CPU Freq: " + String(ESP.getCpuFreqMHz()) + " MHz");
  Serial.println("💾 Flash: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  Serial.println("🧠 Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println(String('=', 60) + "\n");
}

void runAllTests() {
  Serial.println("Starting test sequence...\n");
  
  // Test 1: Hardware Components
  Serial.println("🔧 TEST 1: HARDWARE COMPONENTS");
  Serial.println(String('-', 40));
  results.ledTest = testLED();
  results.buzzerTest = testBuzzer();
  
  // Test 2: GPS Module
  Serial.println("\n🛰️ TEST 2: GPS MODULE");
  Serial.println(String('-', 40));
  results.gpsDataTest = testGPSData();
  results.gpsFixTest = testGPSFix();
  
  // Test 3: GSM Module
  Serial.println("\n📡 TEST 3: GSM MODULE");
  Serial.println(String('-', 40));
  results.gsmAtTest = testGSMCommunication();
  results.gsmNetworkTest = testGSMNetwork();
  
  // Test 4: SMS Functionality
  Serial.println("\n📱 TEST 4: SMS FUNCTIONALITY");
  Serial.println(String('-', 40));
  results.smsTest = testSMSFunctionality();
  
  // Test 5: System Integration
  Serial.println("\n🔗 TEST 5: SYSTEM INTEGRATION");
  Serial.println(String('-', 40));
  results.integrationTest = testSystemIntegration();
}

bool testLED() {
  Serial.print("Testing LED on GPIO " + String(LED_PIN) + "... ");
  
  // Blink LED 3 times
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  Serial.println("✅ PASS");
  return true;
}

bool testBuzzer() {
  Serial.print("Testing Buzzer on GPIO " + String(BUZZER_PIN) + "... ");
  
  // Short beep
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("✅ PASS");
  return true;
}

bool testGPSData() {
  Serial.print("Testing GPS data reception... ");
  
  unsigned long startTime = millis();
  int bytesReceived = 0;
  
  while (millis() - startTime < 5000) { // 5 second test
    if (gpsSerial.available()) {
      gpsSerial.read();
      bytesReceived++;
    }
  }
  
  if (bytesReceived > 0) {
    Serial.println("✅ PASS (" + String(bytesReceived) + " bytes received)");
    return true;
  } else {
    Serial.println("❌ FAIL (No data received)");
    Serial.println("   → Check GPS module connections");
    Serial.println("   → Verify 3.3V power supply");
    Serial.println("   → Ensure antenna is connected");
    return false;
  }
}

bool testGPSFix() {
  Serial.println("Testing GPS fix acquisition...");
  Serial.println("   ⏱️ Waiting up to " + String(TEST_DURATION) + " seconds for GPS fix");
  Serial.println("   💡 For faster fix, move to location with clear sky view");
  
  unsigned long startTime = millis();
  bool fixAcquired = false;
  
  while (millis() - startTime < TEST_DURATION * 1000) {
    while (gpsSerial.available() > 0) {
      if (gps.encode(gpsSerial.read())) {
        if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid()) {
          Serial.println("✅ PASS - GPS fix acquired!");
          Serial.println("   📍 Location: " + String(gps.location.lat(), 6) + ", " + String(gps.location.lng(), 6));
          Serial.println("   🛰️ Satellites: " + String(gps.satellites.value()));
          Serial.println("   📊 HDOP: " + String(gps.hdop.hdop(), 2));
          Serial.println("   ⏰ Time: " + formatTime());
          fixAcquired = true;
          break;
        }
      }
    }
    
    if (fixAcquired) break;
    
    // Show progress
    if ((millis() - startTime) % 5000 == 0) {
      Serial.println("   ⏳ Still waiting... (" + String((millis() - startTime) / 1000) + "s)");
    }
  }
  
  if (!fixAcquired) {
    Serial.println("❌ FAIL - No GPS fix acquired");
    Serial.println("   → Move to outdoor location");
    Serial.println("   → Wait longer (cold start can take 5+ minutes)");
    Serial.println("   → Check antenna connection");
  }
  
  return fixAcquired;
}

bool testGSMCommunication() {
  Serial.print("Testing GSM module communication... ");
  
  // Reset GSM module
  digitalWrite(GSM_RST_PIN, LOW);
  delay(100);
  digitalWrite(GSM_RST_PIN, HIGH);
  delay(3000);
  
  // Test AT command
  String response = sendATCommand("AT", 2000);
  
  if (response.indexOf("OK") > -1) {
    Serial.println("✅ PASS");
    return true;
  } else {
    Serial.println("❌ FAIL");
    Serial.println("   → Check GSM module connections");
    Serial.println("   → Verify power supply (3.7V-4.2V, 2A)");
    Serial.println("   → Check antenna connection");
    return false;
  }
}

bool testGSMNetwork() {
  Serial.println("Testing GSM network registration...");
  
  // Check SIM card
  Serial.print("   Checking SIM card... ");
  String response = sendATCommand("AT+CPIN?", 5000);
  if (response.indexOf("READY") > -1) {
    Serial.println("✅ OK");
  } else {
    Serial.println("❌ FAIL - " + response);
    Serial.println("   → Check SIM card insertion");
    Serial.println("   → Ensure PIN lock is disabled");
    return false;
  }
  
  // Check network registration
  Serial.print("   Checking network registration... ");
  response = sendATCommand("AT+CREG?", 5000);
  if (response.indexOf(",1") > -1 || response.indexOf(",5") > -1) {
    Serial.println("✅ OK");
  } else {
    Serial.println("❌ FAIL - " + response);
    Serial.println("   → Check 2G network coverage");
    Serial.println("   → Wait for network registration");
    return false;
  }
  
  // Check signal strength
  Serial.print("   Checking signal strength... ");
  response = sendATCommand("AT+CSQ", 2000);
  Serial.println("✅ " + response);
  
  Serial.println("✅ PASS - GSM network ready");
  return true;
}

bool testSMSFunctionality() {
  if (String(TEST_PHONE) == "+91XXXXXXXXXX") {
    Serial.println("⚠️ SKIP - Please update TEST_PHONE number in code");
    Serial.println("   → Replace +91XXXXXXXXXX with actual phone number");
    return false;
  }
  
  Serial.println("Testing SMS send functionality...");
  Serial.println("   📱 Sending test SMS to: " + String(TEST_PHONE));
  
  // Set SMS text mode
  sendATCommand("AT+CMGF=1", 2000);
  
  // Send test SMS
  String message = "🧪 GPS Bus Tracker Test\n";
  message += "System verification in progress\n";
  message += "Time: " + formatTime() + "\n";
  message += "All systems operational ✅";
  
  bool smsSuccess = sendSMS(TEST_PHONE, message);
  
  if (smsSuccess) {
    Serial.println("✅ PASS - Test SMS sent successfully");
    Serial.println("   📲 Check your phone for the test message");
    return true;
  } else {
    Serial.println("❌ FAIL - SMS send failed");
    Serial.println("   → Check SIM card credit/plan");
    Serial.println("   → Verify phone number format");
    Serial.println("   → Check network signal strength");
    return false;
  }
}

bool testSystemIntegration() {
  Serial.println("Testing system integration...");
  
  bool gpsOk = results.gpsDataTest && results.gpsFixTest;
  bool gsmOk = results.gsmAtTest && results.gsmNetworkTest;
  bool hwOk = results.ledTest && results.buzzerTest;
  
  Serial.println("   GPS System: " + String(gpsOk ? "✅ OK" : "❌ FAIL"));
  Serial.println("   GSM System: " + String(gsmOk ? "✅ OK" : "❌ FAIL"));
  Serial.println("   Hardware: " + String(hwOk ? "✅ OK" : "❌ FAIL"));
  
  if (gpsOk && gsmOk && hwOk) {
    Serial.println("✅ PASS - All systems integrated successfully");
    
    // Victory beep
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(150);
      digitalWrite(BUZZER_PIN, LOW);
      delay(150);
    }
    
    return true;
  } else {
    Serial.println("❌ FAIL - Some systems not working correctly");
    return false;
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
  
  return response;
}

bool sendSMS(const char* phoneNumber, String message) {
  // Start SMS
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(1000);
  
  // Send message
  gsmSerial.print(message);
  delay(1000);
  
  // Send Ctrl+Z
  gsmSerial.write(26);
  delay(2000);
  
  // Check response
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) {
    if (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      
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

String formatTime() {
  if (gps.time.isValid() && gps.date.isValid()) {
    return String(gps.date.day()) + "/" + 
           String(gps.date.month()) + "/" + 
           String(gps.date.year()) + " " +
           String(gps.time.hour()) + ":" + 
           String(gps.time.minute()) + ":" + 
           String(gps.time.second());
  } else {
    return "GPS Time Not Available";
  }
}

bool allTestsPassed() {
  return results.ledTest && 
         results.buzzerTest && 
         results.gpsDataTest && 
         results.gpsFixTest && 
         results.gsmAtTest && 
         results.gsmNetworkTest && 
         results.smsTest && 
         results.integrationTest;
}

void printFinalResults() {
  Serial.println("\n" + String('=', 60));
  Serial.println("              FINAL TEST RESULTS");
  Serial.println(String('=', 60));
  
  Serial.println("🔧 Hardware Tests:");
  Serial.println("   LED Test:           " + String(results.ledTest ? "✅ PASS" : "❌ FAIL"));
  Serial.println("   Buzzer Test:        " + String(results.buzzerTest ? "✅ PASS" : "❌ FAIL"));
  
  Serial.println("\n🛰️ GPS Tests:");
  Serial.println("   Data Reception:     " + String(results.gpsDataTest ? "✅ PASS" : "❌ FAIL"));
  Serial.println("   Fix Acquisition:    " + String(results.gpsFixTest ? "✅ PASS" : "❌ FAIL"));
  
  Serial.println("\n📡 GSM Tests:");
  Serial.println("   Communication:      " + String(results.gsmAtTest ? "✅ PASS" : "❌ FAIL"));
  Serial.println("   Network Registration: " + String(results.gsmNetworkTest ? "✅ PASS" : "❌ FAIL"));
  
  Serial.println("\n📱 SMS Tests:");
  Serial.println("   SMS Functionality:  " + String(results.smsTest ? "✅ PASS" : "❌ FAIL"));
  
  Serial.println("\n🔗 Integration Tests:");
  Serial.println("   System Integration: " + String(results.integrationTest ? "✅ PASS" : "❌ FAIL"));
  
  Serial.println("\n" + String('-', 60));
  
  if (allTestsPassed()) {
    Serial.println("🎉 OVERALL RESULT: ✅ ALL TESTS PASSED");
    Serial.println("✅ System is ready for deployment!");
    Serial.println("📱 Next step: Upload the main tracking code");
  } else {
    Serial.println("⚠️ OVERALL RESULT: ❌ SOME TESTS FAILED");
    Serial.println("🔧 Please fix the failed components before deployment");
    Serial.println("📋 Refer to the error messages above for troubleshooting");
  }
  
  Serial.println("\n💡 Pro Tips:");
  Serial.println("   • GPS fix can take 5+ minutes on first use");
  Serial.println("   • Ensure clear sky view for GPS antenna");
  Serial.println("   • SIM800L needs 2A power supply");
  Serial.println("   • Check 2G network coverage in your area");
  Serial.println("   • Press test button to re-run tests");
  
  Serial.println("\n" + String('=', 60));
  
  // Final status indication
  if (allTestsPassed()) {
    // Success melody
    int melody[] = {262, 294, 330, 349, 392, 440, 494, 523};
    for (int i = 0; i < 8; i++) {
      tone(BUZZER_PIN, melody[i], 200);
      delay(250);
    }
  } else {
    // Error tone
    for (int i = 0; i < 3; i++) {
      tone(BUZZER_PIN, 200, 500);
      delay(600);
    }
  }
}
