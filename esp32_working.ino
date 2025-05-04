#include <ESP32Servo.h>
#include <WiFi.h>
#include <HX711.h>

// Pin Definitions
#define MOISTURE_SENSOR_PIN 34
#define TEMP_SENSOR_PIN     35
#define GAS_SENSOR_PIN      32
#define TRIG_PIN            14
#define ECHO_PIN            12
#define SERVO_PIN           13
#define ALT_SERVO_PIN       27
#define LOADCELL_DOUT_PIN   4
#define LOADCELL_SCK_PIN    5

// WiFi Credentials
const char* ssid = "mahin";
const char* password = "11111111";
WiFiClient client;

// ESP32-CAM IP
const char* esp32_cam_ip = "192.168.1.50";
const int esp32_cam_port = 80;

// Components
HX711 scale;
Servo myServo;
bool useAltServo = false;
float calibration_factor = -96650;

void setup() {
  Serial.begin(115200);
  Serial.println("🔄 ESP32 System Booting...");

  // Attach Servo
  myServo.setPeriodHertz(50); // Standard 50Hz
  myServo.attach(SERVO_PIN, 500, 2400); // Min and max pulse widths in microseconds
  myServo.write(0);
  delay(1000);

  if (myServo.read() != 0) {
    Serial.println("⚠ Switching to alternative Servo pin");
    myServo.detach();
    myServo.attach(ALT_SERVO_PIN, 500, 2400);
    useAltServo = true;
  }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);  // ⚠ Calibrate for your load cell
  scale.tare();

  // WiFi Connection
  Serial.print("📡 Connecting to WiFi");
  WiFi.begin(ssid, password);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📍 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠ WiFi Connection Failed! Restarting...");
    ESP.restart();
  }
}

void loop() {
  float weight = scale.get_units(5);
  int moisture = readMoisture();
  int distance = getDistance();
  float temperatureC = readTemperature();
  int gasLevel = analogRead(GAS_SENSOR_PIN);

  Serial.println("\n🔍 Sensor Readings:");
  Serial.printf("⚖ Weight: %.2f\n", weight);
  Serial.printf("💧 Moisture: %d\n", moisture);
  Serial.printf("📏 Distance: %d cm\n", distance);
  Serial.printf("🌡 Temperature: %.2f °C\n", temperatureC);
  Serial.printf("🛢 Gas Level: %d\n", gasLevel);

  if (weight < 0 || moisture <= 0 || temperatureC < 0 || gasLevel < 0 || distance < 0) {
    Serial.println("⚠ Sensor Error! Assigning to Dry Bin & Capturing Image...");
    captureImage();
  }

  // Default to Dry Bin
  myServo.write(0);
  delay(2000);

  if (moisture > 0 && moisture <= 1800) {
    Serial.println("🗑 Dry Bin Selected (0°)");
    myServo.write(0);
  }
  else if (moisture > 1800 && moisture < 4095) {
    Serial.println("🗑 Wet Bin Selected (180°)");
    myServo.write(180);
  }
  else {
    Serial.println("⚠ Moisture Sensor Error - Using Distance Sensor");
    if (distance > 5 && distance < 25) {
      Serial.println("🖐 Object Detected (Wet Bin - 180°)");
      myServo.write(180);
      delay(2000);
    } else {
      Serial.println("📏 No Object Detected, Assigning Dry Bin");
      myServo.write(0);
    }
  }

  // Move to Horizontal
  Serial.println("⏳ Moving to Horizontal State...");
  myServo.write(90);
  delay(2000);

  // Reset
  Serial.println("🔄 Resetting to Dry Bin...");
  myServo.write(0);
  delay(5000);

  sendData(weight, moisture, temperatureC, gasLevel);
}

// Moisture Reading Averaged
int readMoisture() {
  long total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(MOISTURE_SENSOR_PIN);
    delay(10);
  }
  return total / 10;
}

// Convert Analog Temp to Celsius
float readTemperature() {
  int raw = analogRead(TEMP_SENSOR_PIN);
  float voltage = raw * (3.3 / 4095.0);  // ADC to volts
  return voltage * 100.0; // LM35: 10mV/°C
}

// Ultrasonic Distance with Timeout
int getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

// Send Data to Server
void sendData(float weight, int moisture, float temperatureC, int gasLevel) {
  Serial.println("📡 Sending Data to Server...");
  if (WiFi.status() == WL_CONNECTED) {
    if (client.connect("192.168.1.100", 80)) {
      String url = "/update?weight=" + String(weight, 2) +
                   "&moisture=" + String(moisture) +
                   "&temperature=" + String(temperatureC, 2) +
                   "&gas=" + String(gasLevel);
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: 192.168.1.100\r\n" +
                   "Connection: close\r\n\r\n");
      Serial.println("✅ Data Sent!");
    }
    client.stop();
  } else {
    Serial.println("⚠ WiFi Not Connected! Data Not Sent.");
  }
}

// Trigger ESP32-CAM to Capture Image
void captureImage() {
  Serial.println("📷 Capturing Image...");
  if (WiFi.status() == WL_CONNECTED) {
    if (client.connect(esp32_cam_ip, esp32_cam_port)) {
      client.print("GET /capture HTTP/1.1\r\n");
      client.print("Host: " + String(esp32_cam_ip) + "\r\n");
      client.print("Connection: close\r\n\r\n");
      Serial.println("📸 Image Captured & Sent!");
    }
    client.stop();
  } else {
    Serial.println("⚠ WiFi Not Connected! Unable to Capture Image.");
  }
}
