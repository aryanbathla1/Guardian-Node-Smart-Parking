#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>

// =====================================================
// GUARDIAN-NODE SMART PARKING SYSTEM
// 3707ICT - Automation and IoT
// =====================================================


// -------------------- PIN DEFINITIONS --------------------

const int TRIG_PIN   = 5;
const int ECHO_PIN   = 18;

const int RED_LED    = 25;
const int GREEN_LED  = 26;

const int SERVO_PIN  = 13;
const int PIR_PIN    = 27;
const int BUZZER_PIN = 14;


// -------------------- PARKING THRESHOLDS --------------------

// Hysteresis prevents rapid switching around one threshold.
const float OCCUPIED_DISTANCE = 45.0;
const float AVAILABLE_DISTANCE = 55.0;

// Three consecutive agreeing readings are required.
const int CONFIRMATION_READINGS = 3;


// -------------------- SERVO POSITIONS --------------------

const int BARRIER_CLOSED = 0;
const int BARRIER_OPEN   = 90;


// -------------------- NON-BLOCKING TIMING --------------------

const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long CLOUD_INTERVAL  = 20000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;

unsigned long lastSensorTime = 0;
unsigned long lastCloudTime = 0;
unsigned long lastWiFiRetryTime = 0;


// -------------------- WIFI --------------------

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";


// -------------------- THINGSPEAK --------------------

// Replace this with the Write API Key from your ThingSpeak channel.
const char* THINGSPEAK_WRITE_KEY = "YOUR_WRITE_API_KEY";


// -------------------- SYSTEM STATE --------------------

enum ParkingState {
  AVAILABLE,
  OCCUPIED,
  CHECKING,
  FAILSAFE
};

ParkingState currentState = CHECKING;

Servo barrierServo;

float currentDistance = -1.0;
bool currentMotion = false;

int occupiedCount = 0;
int availableCount = 0;


// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

float readDistance();
void evaluateParkingState(float distance, bool motion);
void applyState(ParkingState state);
void printSystemStatus();
void connectWiFi();
void maintainWiFi();
void uploadToThingSpeak();
const char* getStateName(ParkingState state);
int getStateValue(ParkingState state);


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  barrierServo.attach(SERVO_PIN);

  // Safe startup condition.
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  barrierServo.write(BARRIER_CLOSED);

  Serial.println();
  Serial.println("======================================");
  Serial.println(" Guardian-Node Smart Parking System");
  Serial.println("======================================");

  connectWiFi();

  Serial.println("System initialised.");
  Serial.println();
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  unsigned long currentTime = millis();

  // Local sensing and automation.
  if (currentTime - lastSensorTime >= SENSOR_INTERVAL) {

    lastSensorTime = currentTime;

    currentDistance = readDistance();
    currentMotion = digitalRead(PIR_PIN) == HIGH;

    evaluateParkingState(currentDistance, currentMotion);
    applyState(currentState);
    printSystemStatus();
  }

  // Maintain network without stopping local automation.
  if (currentTime - lastWiFiRetryTime >= WIFI_RETRY_INTERVAL) {

    lastWiFiRetryTime = currentTime;
    maintainWiFi();
  }

  // Periodic cloud communication.
  if (currentTime - lastCloudTime >= CLOUD_INTERVAL) {

    lastCloudTime = currentTime;
    uploadToThingSpeak();
  }
}


// =====================================================
// ULTRASONIC SENSOR
// =====================================================

float readDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  // Timeout prevents the program becoming stuck.
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1.0;
  }

  return duration * 0.0343 / 2.0;
}


// =====================================================
// SENSOR FUSION AND INTELLIGENT DECISION LOGIC
// =====================================================

void evaluateParkingState(float distance, bool motion) {

  // Invalid ultrasonic reading -> safe failure state.
  if (distance < 0) {

    currentState = FAILSAFE;

    occupiedCount = 0;
    availableCount = 0;

    return;
  }

  bool objectClose = distance <= OCCUPIED_DISTANCE;
  bool areaClear = distance >= AVAILABLE_DISTANCE;


  // RULE 1:
  // Ultrasonic detects a close object AND PIR detects motion.
  if (objectClose && motion) {

    occupiedCount++;
    availableCount = 0;

    if (occupiedCount >= CONFIRMATION_READINGS) {
      currentState = OCCUPIED;
    } else {
      currentState = CHECKING;
    }

    return;
  }


  // RULE 2:
  // Ultrasonic reports clear area AND PIR reports no motion.
  if (areaClear && !motion) {

    availableCount++;
    occupiedCount = 0;

    if (availableCount >= CONFIRMATION_READINGS) {
      currentState = AVAILABLE;
    } else {
      currentState = CHECKING;
    }

    return;
  }


  // RULE 3:
  // Sensors disagree or distance is inside the hysteresis zone.
  occupiedCount = 0;
  availableCount = 0;

  currentState = CHECKING;
}


// =====================================================
// ACTUATOR CONTROL
// =====================================================

void applyState(ParkingState state) {

  switch (state) {

    case AVAILABLE:

      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);

      barrierServo.write(BARRIER_OPEN);

      noTone(BUZZER_PIN);

      break;


    case OCCUPIED:

      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);

      barrierServo.write(BARRIER_CLOSED);

      noTone(BUZZER_PIN);

      break;


    case CHECKING:

      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, LOW);

      // Safe behaviour while occupancy is uncertain.
      barrierServo.write(BARRIER_CLOSED);

      tone(BUZZER_PIN, 1000, 150);

      break;


    case FAILSAFE:

      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, LOW);

      // Fail-safe design: never open the barrier
      // when sensor information is invalid.
      barrierServo.write(BARRIER_CLOSED);

      tone(BUZZER_PIN, 1500, 500);

      break;
  }
}


// =====================================================
// WIFI
// =====================================================

void connectWiFi() {

  Serial.print("Connecting to Wokwi WiFi");

  WiFi.mode(WIFI_STA);

  // Channel 6 avoids unnecessary network scanning in Wokwi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

  unsigned long startTime = millis();

  // Attempt connection for up to 6 seconds.
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 6000) {

    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("WiFi connected.");

  } else {

    Serial.println();
    Serial.println("WiFi unavailable.");
    Serial.println("Local automation will continue.");
  }
}


void maintainWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Attempting WiFi reconnection...");

  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
}


// =====================================================
// THINGSPEAK CLOUD UPLOAD
// =====================================================

void uploadToThingSpeak() {

  // Allows local testing before cloud credentials are entered.
  if (String(THINGSPEAK_WRITE_KEY) == "YOUR_WRITE_API_KEY") {

    Serial.println("[CLOUD] ThingSpeak API key not configured yet.");

    return;
  }

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("[CLOUD] Upload skipped - WiFi unavailable.");

    return;
  }

  WiFiClientSecure client;

  // Suitable for this simulated university prototype.
  // HTTPS still provides encrypted transport.
  client.setInsecure();

  HTTPClient https;

  String url =
      "https://api.thingspeak.com/update?api_key=" +
      String(THINGSPEAK_WRITE_KEY) +
      "&field1=" + String(currentDistance, 1) +
      "&field2=" + String(currentMotion ? 1 : 0) +
      "&field3=" + String(getStateValue(currentState));


  Serial.println("[CLOUD] Uploading data to ThingSpeak...");

  if (!https.begin(client, url)) {

    Serial.println("[CLOUD] Unable to initialise HTTPS.");

    return;
  }

  int responseCode = https.GET();

  if (responseCode > 0) {

    String response = https.getString();

    Serial.print("[CLOUD] HTTP response: ");
    Serial.println(responseCode);

    Serial.print("[CLOUD] ThingSpeak entry ID: ");
    Serial.println(response);

  } else {

    Serial.print("[CLOUD] Upload failed: ");
    Serial.println(responseCode);
  }

  https.end();
}


// =====================================================
// SERIAL MONITOR
// =====================================================

void printSystemStatus() {

  Serial.println("--------------------------------------");

  Serial.print("Distance: ");

  if (currentDistance < 0) {
    Serial.println("INVALID");
  } else {
    Serial.print(currentDistance, 1);
    Serial.println(" cm");
  }

  Serial.print("PIR Motion: ");
  Serial.println(currentMotion ? "YES" : "NO");

  Serial.print("State: ");
  Serial.println(getStateName(currentState));

  Serial.print("WiFi: ");
  Serial.println(
      WiFi.status() == WL_CONNECTED
      ? "CONNECTED"
      : "DISCONNECTED"
  );

  if (currentState == AVAILABLE) {
    Serial.println("Barrier: OPEN");
  } else {
    Serial.println("Barrier: CLOSED");
  }

  if (currentState == CHECKING) {
    Serial.println("Reason: Sensor readings uncertain/conflicting.");
  }

  if (currentState == FAILSAFE) {
    Serial.println("Warning: Invalid ultrasonic reading.");
  }

  Serial.println("--------------------------------------");
}


// =====================================================
// STATE HELPERS
// =====================================================

const char* getStateName(ParkingState state) {

  switch (state) {

    case AVAILABLE:
      return "AVAILABLE";

    case OCCUPIED:
      return "OCCUPIED";

    case CHECKING:
      return "CHECKING";

    case FAILSAFE:
      return "FAILSAFE";

    default:
      return "UNKNOWN";
  }
}


int getStateValue(ParkingState state) {

  switch (state) {

    case AVAILABLE:
      return 0;

    case OCCUPIED:
      return 1;

    case CHECKING:
      return 2;

    case FAILSAFE:
      return 3;

    default:
      return 3;
  }
}
