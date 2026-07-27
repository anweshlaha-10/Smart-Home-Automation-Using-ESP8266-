#include <Servo.h>

// ==========================================
// PIN DEFINITIONS (NodeMCU ESP-12E Mapping)
// ==========================================
#define THERMISTOR_PIN D0    // Digital input from KY-028 DO
#define FAN_LED D1           // Green LED for Fan
#define LDR_PIN A0           // LDR Analog Pin
#define HOUSE_LED D2         // Yellow LED for House Lights
#define RED_LED D3           // Shared Red LED (Intruder/Security)
#define WELCOME_LED D4       // Blue LED for Welcome Light
#define PIR_PIN D5           // Shared PIR Sensor
#define SERVO_PIN D6         // Door Servo
#define DOOR_BUTTON D7       // Doorbell Push Button
#define SECURITY_BUTTON 3    // Security Mode Push Button (RX / GPIO3)
#define BUZZER_PIN D8        // Common Buzzer

// ==========================================
// SYSTEM THRESHOLDS 
// ==========================================
// NodeMCU has a 10-bit ADC (0-1023). Threshold scaled down from 2000.
const int LIGHT_THRESHOLD = 700; 

// ==========================================
// STATE VARIABLES
// ==========================================
// Button States
bool lastDoorButtonState = HIGH;
bool lastSecButtonState = HIGH;
bool lastHouseLedState = LOW;
bool lastFanState = LOW;
// Security System
bool securityMode = false;
unsigned long lastFlashTime = 0;
bool flashState = false;
bool intruderBeepPlayed = false;
bool lastMotionState = false;
// Door System
Servo doorServo;
bool doorOpened = false;
unsigned long doorOpenTime = 0;
const int DOOR_OPEN_DURATION = 3000; // Door stays open for 3 seconds

// Temp System Buzzer Timer
bool tempBuzzerPlayed = false;
unsigned long tempBuzzerStartTime = 0;

void setup() {
  Serial.begin(115200);

  // --- INPUTS ---
  pinMode(THERMISTOR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(PIR_PIN, INPUT); 
  
  // Using Internal Pull-ups for both buttons
  pinMode(DOOR_BUTTON, INPUT_PULLUP); 
  pinMode(SECURITY_BUTTON, INPUT_PULLUP); 

  // --- OUTPUTS ---
  pinMode(FAN_LED, OUTPUT);
  pinMode(HOUSE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(WELCOME_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initial States -> Everything OFF
  digitalWrite(FAN_LED, LOW);
  digitalWrite(HOUSE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(WELCOME_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize Servo
  doorServo.attach(SERVO_PIN);
  doorServo.write(0); // Ensure door starts closed
  
  lastDoorButtonState = digitalRead(DOOR_BUTTON);
  lastSecButtonState = digitalRead(SECURITY_BUTTON);
  
  Serial.println("NodeMCU Smart Home Capstone Project Ready.");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // These variables help us figure out if the buzzer should be ON this frame
  bool requestBuzzerTemp = false;
  bool requestBuzzerSecurity = false;

  // Create a continuous blinking heartbeat every 200ms for flash effects
  if (currentMillis - lastFlashTime >= 200) {
    lastFlashTime = currentMillis;
    flashState = !flashState;
  }

  // ----------------------------------------
  // ----------------------------------------
// 1. THERMISTOR: TEMPERATURE & FAN
// ----------------------------------------
bool highTemp = (digitalRead(THERMISTOR_PIN) == LOW);

bool currentFanState;

if (highTemp) {

  currentFanState = HIGH;
  digitalWrite(FAN_LED, HIGH);

  if (!tempBuzzerPlayed) {
    if (tempBuzzerStartTime == 0) tempBuzzerStartTime = currentMillis;

    // Request buzzer for the first 2 seconds
    if (currentMillis - tempBuzzerStartTime < 2000) {
      requestBuzzerTemp = true;
    } else {
      tempBuzzerPlayed = true;
    }
  }

} else {

  currentFanState = LOW;
  digitalWrite(FAN_LED, LOW);

  tempBuzzerPlayed = false;
  tempBuzzerStartTime = 0;
}

// Print only when fan state changes
if (currentFanState != lastFanState) {

  if (currentFanState == HIGH) {
    Serial.println("High Temperature Detected -> Fan ON");
  } else {
    Serial.println("Temperature Normal -> Fan OFF");
  }

  lastFanState = currentFanState;
}
  // ----------------------------------------
  // ----------------------------------------
// 2. LDR: AUTOMATIC LIGHTING
// ----------------------------------------
int ldrValue = analogRead(LDR_PIN);

bool currentHouseLedState;

if (ldrValue > LIGHT_THRESHOLD) {   // Dark -> Lights ON
  currentHouseLedState = HIGH;
} else {                            // Bright -> Lights OFF
  currentHouseLedState = LOW;
}

digitalWrite(HOUSE_LED, currentHouseLedState);

// Print only when LED state changes
if (currentHouseLedState != lastHouseLedState) {

  if (currentHouseLedState == HIGH) {
    Serial.println("Darkness Detected -> House Lights ON");
  } else {
    Serial.println("Bright Light Detected -> House Lights OFF");
  }

  lastHouseLedState = currentHouseLedState;
}
  // ----------------------------------------
// 3. DOORBELL BUTTON
// ----------------------------------------

bool currentDoorState = digitalRead(DOOR_BUTTON);

// Detect button press
if (currentDoorState == LOW && lastDoorButtonState == HIGH) {

  delay(20);   // Debounce

  if (digitalRead(DOOR_BUTTON) == LOW && !doorOpened) {

    Serial.println("Doorbell Pressed -> Opening Door");

    // Doorbell Sound (2 Beeps)
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(150);
      digitalWrite(BUZZER_PIN, LOW);
      delay(150);
    }

    // Welcome Light
    digitalWrite(WELCOME_LED, HIGH);

    // Open Door
    doorServo.write(180);

    doorOpened = true;
    doorOpenTime = millis();
  }
}   // <-- THIS BRACE WAS MISSING

lastDoorButtonState = currentDoorState;

// Auto close after 3 seconds
if (doorOpened && (millis() - doorOpenTime >= DOOR_OPEN_DURATION)) {

   // Close Door
  doorServo.write(0);

  digitalWrite(WELCOME_LED, LOW);

  doorOpened = false;

  Serial.println("Door Closed");
}
  // ----------------------------------------
  // 4. SECURITY MODE TOGGLE BUTTON
  // ----------------------------------------
  bool currentSecState = digitalRead(SECURITY_BUTTON);
  if (currentSecState == LOW && lastSecButtonState == HIGH) {
    delay(50); // Debounce
    securityMode = !securityMode; 
    
    Serial.print("Security Mode: ");
    Serial.println(securityMode ? "ON" : "OFF");
    
    // Quick beep to acknowledge mode change
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }
  lastSecButtonState = currentSecState;

  // ----------------------------------------
  // 5. PIR SENSOR LOGIC
  // ----------------------------------------
  bool motionDetected = (digitalRead(PIR_PIN) == LOW);
  
  if (securityMode) {
    // -------- SECURITY MODE ON --------
// -------- SECURITY MODE ON --------
if (motionDetected) {

  if (!lastMotionState) {
    Serial.println("Motion Detected!");
    lastMotionState = true;
  }

  digitalWrite(RED_LED, flashState);
  requestBuzzerSecurity = true;

}
else {

  if (lastMotionState) {
    Serial.println("No Motion");
    lastMotionState = false;
  }

  digitalWrite(RED_LED, LOW);
  requestBuzzerSecurity = false;
}
  }
  else {
    // -------- SECURITY MODE OFF --------
    if (motionDetected) {
      // Intruder indication
      digitalWrite(RED_LED, HIGH);

      // Beep only once
      if (!intruderBeepPlayed) {

    Serial.println("Intruder Detected!");

    digitalWrite(BUZZER_PIN, HIGH);
    delay(250);
    digitalWrite(BUZZER_PIN, LOW);

    intruderBeepPlayed = true;

    requestBuzzerSecurity = false;
}
    }
    else {
      digitalWrite(RED_LED, LOW);
      // Ready for next motion
      intruderBeepPlayed = false;
    }
  }

  // ----------------------------------------
  // 6. MASTER BUZZER CONTROL
  // ----------------------------------------
  // This ensures the Temperature and Security systems don't fight over the buzzer
  if (requestBuzzerSecurity) {
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else if (requestBuzzerTemp) {
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}
