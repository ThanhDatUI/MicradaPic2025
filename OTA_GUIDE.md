# Hướng dẫn sử dụng OTA (Over-The-Air) cho ESP32 Health Monitor

## 1. Tổng quan
OTA (Over-The-Air) cho phép bạn nạp code mới cho ESP32 qua mạng WiFi mà không cần kết nối cáp USB. Điều này rất hữu ích khi thiết bị được đặt ở vị trí khó tiếp cận.

## 2. Cấu hình OTA trong code
```cpp
ArduinoOTA.setHostname("ESP32-RADAR-HEALTH");  // Tên thiết bị
ArduinoOTA.setPassword("datn2024");             // Mật khẩu bảo mật
```

## 3. Hướng dẫn nạp code qua OTA

### 3.1. Sử dụng Arduino IDE
1. **Lần đầu tiên**: Nạp code qua USB như bình thường
2. **Sau khi ESP32 khởi động và kết nối WiFi**:
   - Mở Arduino IDE
   - Vào menu `Tools` > `Port`
   - Bạn sẽ thấy xuất hiện `ESP32-RADAR-HEALTH at 192.168.x.x (ESP32 Dev Module)`
   - Chọn port network này thay vì port COM
   - Nhấn Upload như bình thường
   - Nhập mật khẩu: `datn2024` khi được yêu cầu

### 3.2. Sử dụng PlatformIO
1. **Cấu hình trong platformio.ini**:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
upload_protocol = espota
upload_port = 192.168.1.xxx  ; IP của ESP32
upload_flags = 
    --port=3232
    --auth=datn2024
```

2. **Upload qua OTA**:
```bash
pio run --target upload
```

### 3.3. Sử dụng ESP32 OTA Tool
```bash
python espota.py -i 192.168.1.xxx -p 3232 -a datn2024 -f firmware.bin
```

## 4. Thông tin kết nối

### 4.1. Thông tin hiển thị trên Serial Monitor
```
System Initialized
=== OTA Configuration ===
Hostname: ESP32-RADAR-HEALTH
Password: datn2024
IP Address: 192.168.1.xxx
Ready for OTA updates!
========================
```

### 4.2. Thông tin hiển thị trên LCD
- **System Ready!**
- **OTA Enabled**
- **IP: 192.168.1.xxx**
- **Health Monitor**

## 5. Các trạng thái OTA trên LCD

### 5.1. Khi bắt đầu OTA
```
OTA Update...
Starting
```

### 5.2. Trong quá trình OTA
```
OTA Updating...
[████████████] 75%
```

### 5.3. Khi hoàn thành
```
OTA Complete!
Restarting...
```

### 5.4. Khi có lỗi
```
OTA Error!
Auth Failed / Begin Failed / etc.
```

## 6. Lưu ý quan trọng

### 6.1. Bảo mật
- ✅ Đã có mật khẩu bảo vệ: `datn2024`
- ✅ Chỉ hoạt động trong mạng LAN
- 🔥 **Thay đổi mật khẩu** trong môi trường production

### 6.2. Mạng
- ESP32 và máy tính phải cùng mạng WiFi
- Kiểm tra firewall không chặn port 3232
- Router cần hỗ trợ mDNS để tự động discover

### 6.3. Khi nạp OTA
- ⚠️ **Không ngắt nguồn** ESP32 trong quá trình OTA
- ⚠️ **Không reset** ESP32 khi đang upload
- ⚠️ **Đảm bảo WiFi ổn định** trong quá trình upload

### 6.4. Backup và Recovery
- Luôn giữ bản backup firmware hoạt động tốt
- Nếu OTA thất bại, ESP32 sẽ tiếp tục chạy firmware cũ
- Trong trường hợp xấu nhất, có thể nạp lại qua USB

## 7. Troubleshooting

### 7.1. Không tìm thấy thiết bị OTA
- Kiểm tra ESP32 đã kết nối WiFi thành công
- Kiểm tra IP address trên LCD hoặc Serial Monitor
- Restart Arduino IDE/PlatformIO
- Kiểm tra cùng mạng WiFi

### 7.2. Upload thất bại
- Kiểm tra mật khẩu: `datn2024`
- Thử upload file nhỏ hơn trước
- Kiểm tra firewall/antivirus
- Reset ESP32 và thử lại

### 7.3. ESP32 không phản hồi sau OTA
- Chờ ESP32 tự restart (có thể mất vài giây)
- Kiểm tra code mới có lỗi compile không
- Nạp lại firmware backup qua USB

## 8. Ví dụ thực tế

### 8.1. Upload lần đầu (qua USB)
1. Kết nối ESP32 với máy tính qua USB
2. Chọn COM port phù hợp
3. Upload code chứa OTA
4. Mở Serial Monitor để xem IP address

### 8.2. Upload lần sau (qua OTA)
1. Mở Arduino IDE
2. Vào Tools > Port
3. Chọn "ESP32-RADAR-HEALTH at 192.168.1.xxx"
4. Upload code mới
5. Nhập password: datn2024
6. Chờ hoàn thành

## 9. Monitoring OTA

### 9.1. Theo dõi qua Serial Monitor
```
Start updating sketch
Progress: 25%
Progress: 50%
Progress: 75%
Progress: 100%
End
```

### 9.2. Theo dõi qua LCD
- Thanh tiến trình trực quan
- Phần trăm hoàn thành
- Thông báo lỗi nếu có

---

**Tác giả**: DATN 2024 - ESP32 Health Monitor  
**Cập nhật**: 2024  
**Version**: 1.0
