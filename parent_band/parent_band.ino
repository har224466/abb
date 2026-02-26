#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// =====================================
// WIFI CONFIG
// =====================================
const char* ssid = "Ligma";
const char* password = "swayam@123";

// =====================================
// SERVER CONFIG
// =====================================
WebServer server(8080);

// =====================================
// OLED CONFIG
// =====================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

#define OLED_SDA 19
#define OLED_SCL 18

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================
// VIBRATION MOTOR
// =====================================
#define VIBRATION_PIN 12

// =====================================
// VIBRATION PATTERNS
// =====================================
void vibratePattern(String state) {
  if (state == "Hungry") {
    for (int i = 0; i < 2; i++) {
      digitalWrite(VIBRATION_PIN, HIGH);
      delay(200);
      digitalWrite(VIBRATION_PIN, LOW);
      delay(200);
    }
  }
  else if (state == "Sleepy") {
    digitalWrite(VIBRATION_PIN, HIGH);
    delay(500);
    digitalWrite(VIBRATION_PIN, LOW);
  }
  else if (state == "Stress" || state == "High Stress (Alert)") {
    for (int i = 0; i < 5; i++) {
      digitalWrite(VIBRATION_PIN, HIGH);
      delay(150);
      digitalWrite(VIBRATION_PIN, LOW);
      delay(150);
    }
  }
  else {
    digitalWrite(VIBRATION_PIN, LOW);
  }
}

// =====================================
// OLED DISPLAY FUNCTION
// =====================================
void updateOLED(String state, float confidence, int bpm) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("Baby Monitor");

  display.setCursor(0, 16);
  display.print("State: ");
  display.println(state);

  display.setCursor(0, 32);
  display.print("Conf: ");
  display.print(confidence * 100, 1);
  display.println("%");

  display.setCursor(0, 48);
  display.print("BPM: ");
  display.println(bpm);

  display.display();
}

// =====================================
// HTTP HANDLER
// =====================================
void handleResult() {
  String body = server.arg("plain");

  Serial.println("\n📥 DATA FROM LAPTOP:");
  Serial.println(body);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err) {
    Serial.println("❌ JSON parse failed");
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  String state = doc["state"];
  float confidence = doc["confidence"];
  int bpm = doc["bpm"];

  Serial.println("✅ PARSED DATA:");
  Serial.println(state);
  Serial.println(confidence);
  Serial.println(bpm);

  updateOLED(state, confidence, bpm);
  vibratePattern(state);

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// =====================================
// SETUP
// =====================================
void setup() {
  Serial.begin(115200);

  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);

  // I2C INIT WITH YOUR PINS
  Wire.begin(OLED_SDA, OLED_SCL);

  // OLED INIT
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("❌ OLED init failed");
    while (true);
  }

  // Show PARENT BAND first
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("PARENT BAND");
  display.display();
  delay(1500);  // pause so it's visible

  // Then show starting message
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Starting...");
  display.display();

  // WIFI CONNECT
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Parent ESP32 connected");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());

  // SERVER ROUTE
  server.on("/result", HTTP_POST, handleResult);
  server.begin();

  Serial.println("🌐 Server listening on port 8080");
}

// =====================================
// LOOP
// =====================================
void loop() {
  server.handleClient();
}