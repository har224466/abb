/***************
 * Baby Band - ESP32
 * Sensors: MPU-6050, Microphone, Heart Rate
 * Communication: WiFi HTTP POST to server
 ***************/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <MPU6050.h>  // MPU6050 library

// ===== WiFi Credentials =====
const char* ssid = "Ligma";
const char* password = "swayam@123";

// ===== Server URL =====
// NOTE: Replace YOUR_SERVER_IP with your PC's LAN IP
// FastAPI must run: uvicorn main:app --host 0.0.0.0 --port 8000
const char* serverIP = "10.30.109.75";
const int serverPort = 8000;
const char* serverEndpoint = "/predict";

// ===== Pins =====
#define MIC_PIN 34
#define HR_PIN 35
#define SDA_PIN 21
#define SCL_PIN 22

// ===== MPU Setup =====
MPU6050 mpu;
float ax, ay, az;
float gx, gy, gz;

// ===== Heart Rate Variables =====
unsigned long lastPulseTime = 0;
float bpm = 0;

// ===== Cry Variables =====
#define MIC_SAMPLE_WINDOW 100   // milliseconds
float cryVolume = 0.0;
float cryFrequency = 0.0;
unsigned long lastCryTime = 0;
int cryCount = 0;

// ===== Motion Variables =====
float motionIntensity = 0.0;
float restlessness = 0.0;
#define MOTION_SAMPLE_WINDOW 200  // milliseconds

// ===== Heart Rate Trend & Variability =====
#define HR_HISTORY_SIZE 20
float hrHistory[HR_HISTORY_SIZE];
int hrIndex = 0;
float hrTrend = 0.0;
float hrVar = 0.0;

// ===== Timing =====
unsigned long lastMicCheck = 0;
unsigned long lastMotionCheck = 0;
unsigned long lastServerUpdate = 0;

void setup() {
  Serial.begin(115200);

  // ===== WiFi Connect =====
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // ===== MPU Setup =====
  Wire.begin(SDA_PIN, SCL_PIN);
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (1);
  }
}

void loop() {
  unsigned long currentTime = millis();

  // ===== Microphone Cry Volume & Frequency =====
  if (currentTime - lastMicCheck >= MIC_SAMPLE_WINDOW) {
    lastMicCheck = currentTime;
    float micValue = analogRead(MIC_PIN) / 4095.0; // Normalize 0-1
    cryVolume = micValue;

    if (micValue > 0.2) { // Threshold to detect cry
      cryCount++;
      lastCryTime = currentTime;
    }

    // Cry frequency: count per minute normalized
    cryFrequency = constrain((cryCount / ((currentTime / 60000.0))), 0.0, 1.0);
  }

  // ===== MPU Motion Intensity & Restlessness =====
  if (currentTime - lastMotionCheck >= MOTION_SAMPLE_WINDOW) {
    lastMotionCheck = currentTime;

    // Temporary int16_t variables for MPU library
    int16_t raw_ax, raw_ay, raw_az;
    int16_t raw_gx, raw_gy, raw_gz;

    mpu.getMotion6(&raw_ax, &raw_ay, &raw_az, &raw_gx, &raw_gy, &raw_gz);

    // Convert to float for calculations
    ax = (float)raw_ax;
    ay = (float)raw_ay;
    az = (float)raw_az;
    gx = (float)raw_gx;
    gy = (float)raw_gy;
    gz = (float)raw_gz;

    // Motion Intensity: normalized magnitude of acceleration
    motionIntensity = constrain(sqrt(ax*ax + ay*ay + az*az) / 17000.0, 0.0, 1.0);

    // Restlessness: jerk = derivative of acceleration
    static float last_ax = 0, last_ay = 0, last_az = 0;
    float jerk = abs(ax - last_ax) + abs(ay - last_ay) + abs(az - last_az);
    restlessness = constrain(jerk / 500.0, 0.0, 1.0);
    last_ax = ax; last_ay = ay; last_az = az;
  }

  // ===== Heart Rate Calculation =====
  int hrRaw = analogRead(HR_PIN); // Raw analog input
  static bool pulseDetected = false;
  if (hrRaw > 2000 && !pulseDetected) { // pulse threshold
    unsigned long now = millis();
    bpm = 60000.0 / (now - lastPulseTime);
    lastPulseTime = now;
    pulseDetected = true;

    // Store in history for trend & variability
    hrHistory[hrIndex] = bpm;
    hrIndex = (hrIndex + 1) % HR_HISTORY_SIZE;

    // Trend: average slope
    hrTrend = (hrHistory[hrIndex] - hrHistory[(hrIndex + HR_HISTORY_SIZE - 1) % HR_HISTORY_SIZE]);

    // Variability: stddev normalized
    float sum = 0, sumSq = 0;
    for (int i = 0; i < HR_HISTORY_SIZE; i++) {
      sum += hrHistory[i];
      sumSq += hrHistory[i] * hrHistory[i];
    }
    float mean = sum / HR_HISTORY_SIZE;
    float variance = (sumSq / HR_HISTORY_SIZE - mean * mean);
    hrVar = constrain(sqrt(variance) / 90.0, 0.0, 1.0); // normalized 0-1

  } else if (hrRaw < 2000) {
    pulseDetected = false;
  }

  // ===== Send Data to Server =====
  if (currentTime - lastServerUpdate >= 1000) { // every second
    lastServerUpdate = currentTime;

    // Auto-reconnect WiFi if disconnected
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("\nWiFi Reconnected!");
    }

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      // Correct server URL using LAN IP and endpoint
      String fullURL = String("http://") + serverIP + ":" + serverPort + serverEndpoint;
      http.begin(fullURL);

      http.addHeader("Content-Type", "application/json");

      // JSON keys must match FastAPI model (snake_case)
      String jsonData = "{";
      jsonData += "\"cry_volume\":" + String(cryVolume, 3);
      jsonData += ",\"cry_frequency\":" + String(cryFrequency, 3);
      jsonData += ",\"motion_intensity\":" + String(motionIntensity, 3);
      jsonData += ",\"restlessness\":" + String(restlessness, 3);
      jsonData += ",\"heart_rate\":" + String(int(bpm));
      jsonData += ",\"heart_rate_trend\":" + String(int(hrTrend));
      jsonData += ",\"hr_variability\":" + String(hrVar, 3);
      jsonData += "}";

      int httpResponseCode = http.POST(jsonData);
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Server Response: " + response);
      } else {
        Serial.println("Error sending data, code: " + String(httpResponseCode));
      }

      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
  }
}