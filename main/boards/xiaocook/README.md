# XiaoCook — màn hình đã kiểm tra

## Build và nạp bằng lệnh ngắn

```sh
source /Users/macos/Developer/XiaoCook/tools/xiaocook.zsh
xc main          # Build, nạp, mở log; giữ preset tiếng Việt, wake word tắt
xc main build    # Chỉ build
xc test          # Nạp bản test LCD độc lập
```

Hướng dẫn đầy đủ: `/Users/macos/Developer/XiaoCook/BUILD-AND-FLASH.md`.
Thoát log bằng Ctrl+]. Helper dùng Python cố định và tự xử lý cache cũ.


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

## Tiện ích từ cấu hình cộng đồng

- Giao diện/âm báo tiếng Việt: dùng `sh main/boards/xiaocook/build.sh`. Script
  chọn `--language vi-VN`; ngôn ngữ trả lời của AI vẫn phụ thuộc cấu hình server.
- Hội thoại nhiều dòng: dùng tuỳ chọn `USE_MULTILINE_CHAT_MESSAGE` sẵn có
  trong upstream. Tin nhắn dài có thể che một phần mắt trong lúc hiển thị.
- BOOT bấm một lần: bắt đầu/kết thúc trò chuyện như trước. Bấm đúp: tắt loa;
  bấm đúp lần nữa: khôi phục âm lượng trước đó (nếu đang 0 từ lúc khởi động,
  khôi phục 50). Chỉ tắt loa, không tắt micro; thao tác chạy qua Schedule.

Nguồn tham khảo:
- https://github.com/78/xiaozhi-esp32 (bản địa hoá, các kiểu UI)
- https://github.com/78/xiaozhi-esp32/blob/main/main/boards/bread-compact-wifi/compact_wifi_board.cc
  (điều chỉnh âm lượng bằng nút và thông báo trên màn hình; XiaoCook dùng bấm đúp).

Sau build, nạp bằng `python3 "$IDF_PATH/tools/idf.py" -p PORT flash monitor`.
Cần kiểm tra thực tế tiếng Việt/dấu, xuống dòng, bấm đơn/đúp và âm lượng khi
đang phát. Không tự bật AEC phần cứng hoặc deep sleep cho bo này.

### Đánh thức giọng nói — tạm tắt

Preset và config board hiện chọn `WAKE_WORD_DISABLED`: dùng BOOT để bắt đầu
trò chuyện. Build bằng `sh main/boards/xiaocook/build.sh`; vẫn giữ tiếng Việt,
GIF mắt xanh, hội thoại nhiều dòng và bấm đúp tắt/bật loa.

Mã preset `himax` vẫn được giữ trong scripts/build.py. Để bật lại sau này:
1. Bỏ `CONFIG_WAKE_WORD_DISABLED=y` khỏi config.json của board XiaoCook.
2. Chạy `sh main/boards/xiaocook/build.sh --wake-word himax` rồi nạp firmware.

Hi MAX dùng MultiNet7 tiếng Anh, nhãn/câu lệnh `Hi MAX`, ngưỡng 20%; thư viện
ESP-SR chuyển chữ sang âm vị qua flite_g2p. Đây là nhận dạng lệnh tùy chỉnh,
không phải WakeNet được huấn luyện riêng. Khi bật lại cần thử độ nhạy/gọi nhầm.
Model được đóng gói trong assets cùng GIF; không thêm phân vùng model.
https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html

## Phục hồi LCD khi tiếp xúc chập chờn

ST7789 240×240 được gửi lại lệnh thoát sleep, RGB565, hướng, inversion và
bật hiển thị mỗi 5 giây, rồi yêu cầu LVGL vẽ lại toàn màn hình. Không reset
ESP32 hoặc kéo chân RST định kỳ. Task riêng ưu tiên thấp giữ khóa LVGL khi
gửi lệnh; giao dịch tham số SPI chờ DMA đang chạy hoàn tất. ST7789 init có
khoảng chờ 100 ms nên hoạt ảnh có thể khựng ngắn mỗi chu kỳ.

Giữ BOOT để yêu cầu gửi lại ngay; bấm đơn trò chuyện và bấm đúp tắt/bật loa
vẫn giữ nguyên. Đổi chu kỳ tại DISPLAY_RECOVERY_INTERVAL_MS trong config.h.
Đây là phát lại cấu hình chủ động, không phải phát hiện lỗi: module không có
dây đọc phản hồi. Có thể hồi phục sau mất trạng thái/reset LCD khi kết nối
ổn định trở lại; không sửa được dây đang hở/chập, sụt nguồn hoặc phần cứng hỏng.
Cần bọc cách điện chân hở và cố định dây. Chưa kiểm chứng phục hồi trên bo.

## Mắt xanh vuông — preset main hiện tại

`xc main` đóng gói bộ `assets/eyes-square-blue`: 21 GIF xanh #00A0FF trên
nền đen, góc bo nhỏ (roundness tối đa 0.32), giữ chớp mắt/đảo mắt và biểu cảm.
Bộ Otto tròn cũ ở `assets/eyes-blue` vẫn được giữ nhưng không đóng gói cùng.
Tạo lại bộ vuông: `python3 main/boards/xiaocook/tools/generate_eyes.py --style square`.
Tạo lại bộ tròn: thay `--style square` bằng `--style round`. Cần Python có Pillow.
Renderer dựa trên mã Otto MIT đã ghi nguồn ở trên, không đưa thư viện RoboEyes
Arduino vào firmware. Không đổi cấu hình SPI hoặc cơ chế phục hồi LCD.
