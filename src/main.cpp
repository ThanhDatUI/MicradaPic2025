#include "Arduino.h"
#include <60ghzbreathheart.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <QRCode.h>
#include "Adafruit_VL53L0X.h"
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "Fuvitech";
const char* password = "fuvitech.vn";

const char* mqtt_server = "mqtt.fuvitech.vn";
const int mqtt_port = 2883;
const char* mqtt_topic = "DATN/Weight";
const char* mqtt_sensor_topic = "DATN/VALUESENSOR"; // Topic cho sensor data

const char* googleSheetURL = "https://script.google.com/macros/s/AKfycbzGbXIt-b_FD_iVvTMfj-FrVv5y3el6qsaiQVEptZQ6OAKeTrArqXx_GqjMkb1pOGyJ5A/exec";

WiFiClient espClient;
PubSubClient client(espClient);

#define RELAY_PIN 33
#define SOUND_PIN 34

BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial2);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
DHT dht(32, DHT22); 

#define MAX_SAMPLES 60
int heartRateSamples[MAX_SAMPLES];
int breathRateSamples[MAX_SAMPLES];
float weightSamples[MAX_SAMPLES];
int sampleCount = 0;
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 1000; 

// Thuật toán mới: Lấy mẫu cuối cùng (thứ 60) làm kết quả thay vì trung bình
// - Thu thập 60 mẫu trong 60 giây
// - Sử dụng mẫu cuối cùng (mẫu thứ 60) làm kết quả cuối cùng
// - Điều này giúp có kết quả ổn định hơn sau quá trình đo đạc 

int latestHeartRate = 0;
int latestRespirationRate = 0;
int latestDistance = 0; 
float latestHeight = 0; 
float latestTemperature = 0;
float latestHumidity = 0; 
float latestWeight = 0; 
float lastWeight = 0; 
float latestBMI = 0;
unsigned long lastRadarUpdate = 0;
const unsigned long radarInterval = 1000;
const unsigned long displayInterval = 500;
unsigned long lastDisplayUpdate = 0;
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 22);

// Khởi tạo đối tượng QRCode
QRCode qrcode;

int dotCount = 0;
unsigned long lastDotUpdate = 0;
const unsigned long dotInterval = 500;

int readingID = 0;
#define EEPROM_SIZE 512
#define ID_ADDRESS 0
bool hasLogged = false;
bool hasSentMQTT = false; // Cờ để kiểm soát việc gửi MQTT

// Biến cho QR code display
bool showQRCode = false;
unsigned long qrStartTime = 0;
const unsigned long qrDisplayDuration = 10000; // 10 giây

// Function declarations
void sendSensorDataToMQTT(int id, int heartRate, int breathRate, float height, float weight, float bmi);

int averageInt(int* arr, int count) {
  if (count == 0) return 0;
  int sum = 0;
  for (int i = 0; i < count; i++) sum += arr[i];
  return sum / count;
}

float averageFloat(float* arr, int count) {
  if (count == 0) return 0;
  float sum = 0;
  for (int i = 0; i < count; i++) sum += arr[i];  
  return sum / count;
}

float calculateBMI(float weight_kg, float height_cm) {
  if (weight_kg <= 0 || height_cm <= 0) return 0;
  float height_m = height_cm / 100.0; 
  return weight_kg / (height_m * height_m);
}

void resetSamples() {
  sampleCount = 0;
  for (int i = 0; i < MAX_SAMPLES; i++) {
    heartRateSamples[i] = 0;
    breathRateSamples[i] = 0;
    weightSamples[i] = 0;
  }
  dotCount = 0;
  lastDotUpdate = millis();
  hasLogged = false;
  hasSentMQTT = false; // Reset cờ MQTT
  showQRCode = false; // Reset QR code display
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  latestWeight = message.toFloat();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void logDataToGoogleSheet(int id, int heartRate, int breathRate, float height, float weight, float bmi) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  if (heartRate <= 0 || breathRate <= 0 || height <= 0 || weight <= 0 || bmi <= 0) {
    Serial.println("Invalid sensor data, skipping Google Sheet log");
    return;
  }

  HTTPClient http;
  
  Serial.println("Logging data to Google Sheet...");
  http.begin(googleSheetURL);
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{\"ID\":" + String(id) +
                       ",\"HeartRate\":" + String(heartRate) +
                       ",\"BreathRate\":" + String(breathRate) +
                       ",\"Height\":" + String(height, 1) +
                       ",\"Weight\":" + String(weight, 2) +
                       ",\"BMI\":" + String(bmi, 1) + "}";

  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);
    
    // Google Apps Script thường trả về 200 hoặc 302 (redirect) khi thành công
    if (httpResponseCode == 200 || httpResponseCode == 302) {
      Serial.println("Data logged successfully to Google Sheet");
      
      // Gửi dữ liệu lên MQTT trước khi hiển thị QR
      sendSensorDataToMQTT(id, heartRate, breathRate, height, weight, bmi);
      
      // Sau khi gửi MQTT xong mới kích hoạt hiển thị QR code
      showQRCode = true;
      qrStartTime = millis();
    } else {
      Serial.println("Failed to log data - unexpected response code");
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void displayQRCode() {
  u8g2.clearBuffer();
  
  String qrData = "https://byvn.net/GJQA";
  
  // Tạo mã QR với phiên bản nhỏ hơn cho URL ngắn
  uint8_t qrcodeData[qrcode_getBufferSize(2)]; // Phiên bản QR 2 (25x25) cho URL ngắn
  qrcode_initText(&qrcode, qrcodeData, 2, ECC_LOW, qrData.c_str());

  
  // Tính toán để căn giữa QR code - nhỏ hơn để vừa màn hình
  uint8_t qr_pixel_size = 1; // Mỗi module QR = 1x1 pixel để vừa màn hình
  uint8_t qr_display_size = qrcode.size * qr_pixel_size; // 25x25 pixel
  uint8_t x_offset = (128 - qr_display_size) / 2; // Căn giữa theo chiều ngang
  uint8_t y_offset = 10; // Bắt đầu từ y=10 để không đè lên chữ

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        // Vẽ module QR với kích thước 1x1 pixel
        u8g2.drawPixel(x + x_offset, y + y_offset);
      }
    }
  }
  
  // Thêm text hướng dẫn bên dưới QR
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(10, 55, "Scan to view details");
  
  u8g2.sendBuffer();
}

void updateDisplay() {
  // Kiểm tra xem có cần hiển thị QR code không
  if (showQRCode) {
    if (millis() - qrStartTime < qrDisplayDuration) {
      displayQRCode();
      return;
    } else {
      showQRCode = false; // Tắt QR code sau 10 giây
    }
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(5, 10, "FUVI CAFE");
  
  if (latestWeight > 0) {
    char idStr[20];
    sprintf(idStr, "ID: %d", readingID);
    u8g2.drawStr(80, 10, idStr);
  }
  
  u8g2.drawHLine(0, 12, 128);

  if (latestWeight <= 0) {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 35, "Temperature:");
    char tempStr[10];
    sprintf(tempStr, "%.1f C", latestTemperature);
    u8g2.drawStr(80, 35, tempStr);

    u8g2.drawStr(5, 50, "Humidity:");
    char humStr[10];
    sprintf(humStr, "%.1f %%", latestHumidity);
    u8g2.drawStr(80, 50, humStr);
  } else {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    if (sampleCount < MAX_SAMPLES) {
      // Hiệu ứng progress bar
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(5, 25, "Health Scanning...");
      
      // Progress bar
      int progress = (sampleCount * 100) / MAX_SAMPLES;
      u8g2.drawFrame(5, 30, 118, 8); // Khung progress bar
      u8g2.drawBox(7, 32, (progress * 114) / 100, 4); // Thanh tiến trình
      
      // Hiển thị phần trăm và số mẫu
      char progressStr[20];
      sprintf(progressStr, "%d%% (%d/60)", progress, sampleCount);
      u8g2.drawStr(5, 50, progressStr);
      
      // Hiển thị thông tin về thuật toán
      u8g2.drawStr(5, 60, "Final sample used");
      
      // Dấu chấm động theo thời gian

    } else {
      // Hiển thị kết quả cuối cùng (mẫu thứ 60) thay vì trung bình
      u8g2.drawStr(5, 22, "Heart Rate:");
      char heartRateStr[10];
      sprintf(heartRateStr, "%d bpm", heartRateSamples[MAX_SAMPLES-1]);
      u8g2.drawStr(75, 22, heartRateStr);
      
      u8g2.drawStr(5, 32, "Breath Rate:");
      char respRateStr[10];
      sprintf(respRateStr, "%d rpm", breathRateSamples[MAX_SAMPLES-1]);
      u8g2.drawStr(75, 32, respRateStr);
      
      u8g2.drawStr(5, 42, "Height:");
      char heightStr[10];
      sprintf(heightStr, "%.1f cm", latestHeight);
      u8g2.drawStr(75, 42, heightStr);

      u8g2.drawStr(5, 52, "Weight:");
      char weightStr[10];
      sprintf(weightStr, "%.2f kg", weightSamples[MAX_SAMPLES-1]);
      u8g2.drawStr(75, 52, weightStr);

      u8g2.drawStr(5, 62, "BMI:");
      char bmiStr[10];
      sprintf(bmiStr, "%.1f", calculateBMI(weightSamples[MAX_SAMPLES-1], latestHeight));
      u8g2.drawStr(75, 62, bmiStr);
    }
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  while (!Serial);

  EEPROM.begin(EEPROM_SIZE);
  readingID = EEPROM.readInt(ID_ADDRESS);
  
  if (readingID < 0 || readingID > 99999) {
    readingID = 0;
    EEPROM.writeInt(ID_ADDRESS, readingID);
    EEPROM.commit();
  }
  
  Serial.print("Current Reading ID: ");
  Serial.println(readingID);

  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  unsigned long wifiStartTime = millis();
  const unsigned long wifiTimeout = 5000; // 5 giây timeout
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // Kiểm tra timeout
    if (millis() - wifiStartTime > wifiTimeout) {
      Serial.println();
      Serial.println("WiFi connection timeout! Restarting ESP32...");
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(5, 30, "WiFi Timeout!");
      u8g2.drawStr(5, 45, "Restarting...");
      u8g2.sendBuffer();
      delay(2000);
      ESP.restart(); // Reset ESP32
    }
  }
  Serial.println(" connected");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  dht.begin(); 
  u8g2.begin();
  
  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 30, "VL53L0X Error!");
    u8g2.sendBuffer();
    while(1);
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(5, 30, "Starting...");
  u8g2.drawStr(5, 50, "FUVI CAFE");
  u8g2.sendBuffer();
  delay(2000);
  Serial.println("System Initialized");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  VL53L0X_RangingMeasurementData_t measure;
  Serial.print("Reading a measurement... ");
  lox.rangingTest(&measure, false); 
  if (measure.RangeStatus != 4) {
    Serial.print("Distance (mm): "); 
    Serial.println(measure.RangeMilliMeter);
    latestDistance = measure.RangeMilliMeter; 
    latestHeight = (2000 - latestDistance) / 10.0;
  } else {
    Serial.println(" out of range ");
    latestDistance = 0;
    latestHeight = 0;
  }

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (!isnan(temp) && !isnan(hum)) {
    latestTemperature = temp;
    latestHumidity = hum;
  } else {
    Serial.println("Lỗi đọc cảm biến DHT22!");
  }

  radar.Breath_Heart();        
  if (radar.sensor_report != 0x00) {
    switch (radar.sensor_report) {
      case HEARTRATEVAL:
        Serial.print("Sensor monitored the current heart rate value is: ");
        Serial.println(radar.heart_rate, DEC);
        if (radar.heart_rate == 0 || (radar.heart_rate >= 60 && radar.heart_rate <= 100)) {
          latestHeartRate = radar.heart_rate;
        } else {
          Serial.println("Heart rate out of range (60-100 bpm)! Keeping previous value.");
        }
        Serial.println("----------------------------");
        break;
      case HEARTRATEWAVE:
        Serial.print("The heart rate waveform(Sine wave) -- point 1: ");
        Serial.print(radar.heart_point_1);
        Serial.print(", point 2 : ");
        Serial.print(radar.heart_point_2);
        Serial.print(", point 3 : ");
        Serial.print(radar.heart_point_3);
        Serial.print(", point 4 : ");
        Serial.print(radar.heart_point_4);
        Serial.print(", point 5 : ");
        Serial.println(radar.heart_point_5);
        Serial.println("----------------------------");
        break;
      case BREATHNOR:
        Serial.println("Sensor detects current breath rate is normal.");
        Serial.println("----------------------------");
        break;
      case BREATHRAPID:
        Serial.println("Sensor detects current breath rate is too fast.");
        Serial.println("----------------------------");
        break;
      case BREATHSLOW:
        Serial.println("Sensor detects current breath rate is too slow.");
        Serial.println("----------------------------");
        break;
      case BREATHNONE:
        Serial.println("There is no breathing information yet, please wait...");
        Serial.println("----------------------------");
        break;
      case BREATHVAL:
        Serial.print("Sensor monitored the current breath rate value is: ");
        Serial.println(radar.breath_rate, DEC);
        latestRespirationRate = radar.breath_rate; 
        Serial.println("----------------------------");
        break;
      case BREATHWAVE:
        Serial.print("The breath rate waveform(Sine wave) -- point 1: ");
        Serial.print(radar.breath_point_1);
        Serial.print(", point 2 : ");
        Serial.print(radar.breath_point_2);
        Serial.print(", point 3 : ");
        Serial.print(radar.breath_point_3);
        Serial.print(", point 4 : ");
        Serial.print(radar.breath_point_4);
        Serial.print(", point 5 : ");
        Serial.println(radar.breath_point_5);
        Serial.println("----------------------------");
        break;
    }
  }

  if (lastWeight <= 0 && latestWeight > 0) {
    readingID++;
    EEPROM.writeInt(ID_ADDRESS, readingID);
    EEPROM.commit();
    resetSamples();
    Serial.print("New person detected, Reading ID: ");
    Serial.println(readingID);
  } else if (lastWeight > 0 && latestWeight <= 0) {
    resetSamples();
    Serial.println("Person left, samples reset");
  }
  lastWeight = latestWeight;

  if (latestWeight > 0 && millis() - lastSampleTime >= sampleInterval) {
    // Cập nhật BMI dựa trên trọng lượng hiện tại
    latestBMI = calculateBMI(latestWeight, latestHeight);
    
    if (sampleCount < MAX_SAMPLES) {
      heartRateSamples[sampleCount] = latestHeartRate;
      breathRateSamples[sampleCount] = latestRespirationRate;
      weightSamples[sampleCount] = latestWeight;
      sampleCount++;
    } else {
      // Gửi dữ liệu lên Google Sheet và MQTT chỉ một lần khi đủ samples
      if (!hasLogged && !hasSentMQTT) {
        // Thay đổi: Lấy mẫu cuối cùng (thứ 60) thay vì trung bình
        int finalHeartRate = heartRateSamples[MAX_SAMPLES-1];
        int finalBreathRate = breathRateSamples[MAX_SAMPLES-1];
        float finalWeight = weightSamples[MAX_SAMPLES-1];
        float finalBMI = calculateBMI(finalWeight, latestHeight);
        
        Serial.println("Using final sample (60th) as result:");
        Serial.println("Final Heart Rate: " + String(finalHeartRate));
        Serial.println("Final Breath Rate: " + String(finalBreathRate));
        Serial.println("Final Weight: " + String(finalWeight));
        Serial.println("Final BMI: " + String(finalBMI));
        
        logDataToGoogleSheet(readingID, finalHeartRate, finalBreathRate, latestHeight, finalWeight, finalBMI);
        hasLogged = true; // Đánh dấu đã gửi dữ liệu
        hasSentMQTT = true; // Đánh dấu đã gửi MQTT
      }
      
      // Tiếp tục cập nhật mẫu mới nhất (sliding window)
      for (int i = 1; i < MAX_SAMPLES; i++) {
        heartRateSamples[i-1] = heartRateSamples[i];
        breathRateSamples[i-1] = breathRateSamples[i];
        weightSamples[i-1] = weightSamples[i];
      }
      heartRateSamples[MAX_SAMPLES-1] = latestHeartRate;
      breathRateSamples[MAX_SAMPLES-1] = latestRespirationRate;
      weightSamples[MAX_SAMPLES-1] = latestWeight;
    }
    lastSampleTime = millis();
  }

  if (millis() - lastDotUpdate >= dotInterval) {
    dotCount = (dotCount % 3) + 1;
    lastDotUpdate = millis();
  }

  if (millis() - lastDisplayUpdate >= displayInterval) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  delay(200);
}

void sendSensorDataToMQTT(int id, int heartRate, int breathRate, float height, float weight, float bmi) {
  if (!client.connected()) {
    Serial.println("MQTT not connected, cannot send sensor data");
    return;
  }

  // Tạo JSON object
  JsonDocument doc;
  doc["ID"] = id;
  doc["HeartRate"] = heartRate;
  doc["BreathRate"] = breathRate;
  doc["Height"] = round(height * 10) / 10.0; // Làm tròn 1 chữ số thập phân
  doc["Weight"] = round(weight * 100) / 100.0; // Làm tròn 2 chữ số thập phân
  doc["BMI"] = round(bmi * 10) / 10.0; // Làm tròn 1 chữ số thập phân

  // Serialize JSON thành string
  String jsonString;
  serializeJson(doc, jsonString);

  // Gửi lên MQTT
  Serial.println("Sending sensor data to MQTT...");
  Serial.print("Topic: ");
  Serial.println(mqtt_sensor_topic);
  Serial.print("Data: ");
  Serial.println(jsonString);

  if (client.publish(mqtt_sensor_topic, jsonString.c_str())) {
    Serial.println("Sensor data sent to MQTT successfully");
  } else {
    Serial.println("Failed to send sensor data to MQTT");
  }
}