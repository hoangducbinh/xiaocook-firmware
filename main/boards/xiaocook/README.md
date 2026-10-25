# XiaoCook — màn hình đã kiểm tra

ESP32-S3, LCD IPS ST7789 1.54 inch 240×240. Cấu hình được đối chiếu với
project độc lập `/Users/macos/Developer/XiaoCook/screen-test` đã chạy đủ mẫu màu.

- SPI3, mode **3**, clock **40 MHz**, RGB565, RGB order.
- MOSI/SDA GPIO11; SCK/SCL GPIO12; CS GPIO8; DC GPIO9; RST GPIO10.
- Offset X/Y = 0; không mirror, không swap XY; inversion bật.
- VCC và BL cấp 3.3V, chung GND. Không có GPIO điều khiển đèn nền.
- Chờ thêm 150 ms sau reset và sau panel init, giống bản test.

Cấu hình 240×240 thường và 240×240 7PIN đều dùng mode 3. Tên 7PIN là
nhãn lựa chọn có sẵn trong firmware, không thay đổi dây nối CS GPIO8.

Build từ thư mục firmware sau khi kích hoạt ESP-IDF:

```sh
python3 scripts/build.py xiaocook --name xiaocook-st7789-240x240
```

Sau khi nạp cần xác nhận UI hiển thị đúng màu, đủ khung và không nhiễu.
Build thành công không thay thế kiểm tra trên phần cứng.

## GIF mắt robot xanh dương

Bộ 21 biểu cảm Otto, nền đen và mắt #00A0FF, khung 240×240.
Nguồn MIT: https://github.com/txp666/otto-emoji-gif-component (component 1.4.0).
Mã tạo ảnh thích nghi tại `tools/generate_eyes.py`, license tại `assets/LICENSE-Otto`.
Tạo lại bằng Python có Pillow: `python3 main/boards/xiaocook/tools/generate_eyes.py`.
GIF được đóng gói tự động vào assets partition khi build; `idf.py flash` nạp cả
firmware và assets. Không cần đổi kiểu giao diện sang Emote. Hai theme dùng
nền đen để khớp canvas GIF; thanh trạng thái và chữ hội thoại vẫn hiển thị.
