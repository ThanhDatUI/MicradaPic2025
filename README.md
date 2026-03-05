# ESP32 Health Monitor - DATN 2025

## 📊 Tổng quan
Hệ thống đo sức khỏe tự động sử dụng ESP32 với các tính năng:
- Đo nhịp tim và nhịp thở (Radar 60GHz MR60BHA1)
- Đo chiều cao (VL53L0X ToF sensor)
- Cân nặng (Load cell + HX711) 
- Tính toán BMI tự động
- Hiển thị LCD 128x64 với hiệu ứng động
- Gửi dữ liệu lên Google Sheet và MQTT
- Hiển thị QR code để truy cập dữ liệu
- **OTA (Over-The-Air) - Nạp code từ xa qua WiFi**

## 🔧 Phần cứng
- **ESP32** (DevKit v1)
- **Radar 60GHz** MR60BHA1 (Breath & Heart detection)
- **LCD 128x64** ST7920 (SPI)
- **ToF Sensor** VL53L0X (Đo chiều cao)
- **Temperature/Humidity** DHT22
- **Load Cell** + HX711 (Cân nặng)
- **Relay** điều khiển nguồn cân

## 📱 Tính năng

### ✅ Hoàn thành
- [x] Đo nhịp tim, nhịp thở từ radar 60GHz
- [x] Đo chiều cao bằng ToF sensor VL53L0X  
- [x] Tích hợp cân điện tử (Load cell)
- [x] Tính toán BMI tự động
- [x] Hiển thị LCD với hiệu ứng động
- [x] Gửi dữ liệu lên Google Sheet
- [x] Gửi dữ liệu lên MQTT (JSON format)
- [x] Hiển thị QR code sau khi gửi thành công
- [x] Auto-reset khi WiFi timeout (5s)
- [x] Giao diện đa ngôn ngữ (Việt/Anh)
- [x] **OTA (Over-The-Air) - Nạp code qua WiFi**

### 🎨 Hiệu ứng giao diện
- Progress bar động khi đang đo
- Phần trăm hoàn thành (0-100%)
- Hiển thị nhịp tim real-time
- Thời gian đếm ngược
- Dots animation
- QR code với hướng dẫn quét

## 🌐 Kết nối

### WiFi
```cpp
const char* ssid = "Fuvitech";
const char* password = "fuvitech.vn";
```

### MQTT
```cpp
const char* mqtt_server = "mqtt.fuvitech.vn";
const int mqtt_port = 1883;
const char* mqtt_topic = "DATN/VALUES";        // Dữ liệu hiển thị
const char* mqtt_sensor_topic = "DATN/VALUESENSOR"; // Dữ liệu sensor
```

### OTA Settings
```cpp
Hostname: "ESP32-RADAR-HEALTH"
Password: "datn2024"
Port: 3232
```

## 📊 Định dạng dữ liệu

### MQTT Topic: DATN/VALUESENSOR
```json
{
  "id": 123,
  "heartRate": 72,
  "breathRate": 18,
  "height": 170.5,
  "weight": 65.2,
  "bmi": 22.5
}
```

### Google Sheet
- ID, Heart Rate, Breath Rate, Height, Weight, BMI
- Auto-increment ID với EEPROM
- HTTP POST request với response 302 = success

## 🚀 Cách sử dụng OTA

### 1. Upload lần đầu (qua USB)
```bash
pio run --target upload
```

### 2. Upload qua OTA (lần sau)
1. Kiểm tra IP trên LCD hoặc Serial Monitor
2. Cập nhật IP trong `platformio.ini`:
```ini
upload_protocol = espota
upload_port = 192.168.1.xxx  ; IP của ESP32  
upload_flags = 
    --port=3232
    --auth=datn2024
```
3. Upload: `pio run --target upload`

### 3. Arduino IDE OTA
- Tools > Port > "ESP32-RADAR-HEALTH at 192.168.x.x"
- Upload như bình thường
- Password: `datn2024`

## 📋 Cài đặt

### Dependencies
```ini
lib_deps = 
    olikraus/U8g2@^2.36.5
    adafruit/DHT-sensor-library
    adafruit/Adafruit_VL53L0X@^1.2.4
    knolleary/PubSubClient@^2.8
    ricmoo/QRCode@^0.0.1
    bblanchon/ArduinoJson@^7.4.2
```

### Thư viện ESP32 Core
- WiFi
- EEPROM  
- HTTPClient
- ESPmDNS
- WiFiUdp
- ArduinoOTA

## 🔧 Cấu hình pins

```cpp
// Radar 60GHz
Serial2 (GPIO 16, 17)

// LCD ST7920 (SPI)
CLK:  GPIO 18
DATA: GPIO 23  
CS:   GPIO 5
RST:  GPIO 22

// VL53L0X (I2C)
SDA: GPIO 21
SCL: GPIO 22

// DHT22
DATA: GPIO 4

// Relay
CTRL: GPIO 2
```

## 📖 Tài liệu

### File chính
- `src/main.cpp` - Code chính
- `OTA_GUIDE.md` - Hướng dẫn chi tiết OTA
- `platformio.ini` - Cấu hình project

### Thư viện sensor
- `lib/Seeed-Studio-MR60BHA1-Sensor-main/` - Radar 60GHz library

## 🚨 Lưu ý quan trọng

### OTA Security
- ✅ Password protected: `datn2024`
- ✅ Chỉ hoạt động trong LAN
- 🔥 **Đổi password** trong production!

### Khi OTA
- ⚠️ Không ngắt nguồn ESP32
- ⚠️ Đảm bảo WiFi ổn định  
- ⚠️ Backup firmware trước khi update

### Troubleshooting
- Reset WiFi timeout: 5 giây tự động restart
- MQTT reconnect tự động
- EEPROM lưu ID, tự tăng mỗi lần đo
- LCD hiển thị lỗi và trạng thái

## 👥 Tác giả
ESP32 Health Monitor với OTA  
**Version**: 2.0 (OTA Enabled)  
**Platform**: PlatformIO + ESP32
