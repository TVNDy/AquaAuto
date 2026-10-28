# AquaAuto — bản chỉnh sửa P1/P2 (mã nguồn thử nghiệm)

## Phạm vi đã chỉnh trong gói này

### P1 — độ tin cậy kết nối và xác nhận lệnh
- PWA tạo `command_id` cho từng yêu cầu và gửi kèm `schema`, `issued_at`, `source`.
- PWA chỉ coi phản hồi là xác nhận lệnh nếu `command_id`, node, trạng thái và `command_result` khớp; retained state cũ không tự xác nhận lệnh mới.
- PWA báo trạng thái Gateway đã ngừng cập nhật nếu MQTT vẫn kết nối nhưng không có status mới trong 75 giây.
- Firmware từ chối lệnh thiếu/sai độ dài `command_id`, và phát hiện ID trùng gần nhất để giảm tác động của MQTT QoS 1 redelivery.
- Firmware dùng exponential backoff có jitter cho lần thử kết nối MQTT, giới hạn khoảng 60 giây.

### P2 — gia cố an toàn/bảo mật ở mức mã nguồn
- Thêm cảnh báo rõ ràng rằng broker công cộng + MQTT plaintext + secret trong topic chỉ dành cho thử nghiệm.
- Không tuyên bố rằng secret trong QR/topic là cơ chế xác thực an toàn.

## Những việc P2 chưa thể hoàn tất chỉ bằng sửa frontend/firmware

Hiện firmware và PWA vẫn cấu hình `broker.emqx.io:1883` (firmware) và `wss://broker.emqx.io:8084/mqtt` (PWA), dùng topic `dyauto/{gateway}_{secret}/...`. Đây là broker công cộng và topic có thể bị đoán/quan sát; TLS ở kết nối WebSocket của PWA không bảo vệ đường MQTT plaintext của Gateway, và không tạo ACL riêng cho từng Gateway. **Không triển khai bản này để điều khiển máy bơm/quạt/tải điện thật.**

Trước khi đưa ra hiện trường, cần có:
1. Broker riêng hoặc tenant riêng, TLS cho Gateway và WSS/TLS cho PWA.
2. Tài khoản/mật khẩu hoặc chứng thư riêng cho từng Gateway, ACL giới hạn topic đúng Gateway; tài khoản PWA có quyền tối thiểu. Không dùng chung `DEVICE_SECRET` làm danh tính.
3. Quy trình pairing QR có thời hạn/thu hồi. QR không nên chứa credential dài hạn; tránh lưu secret dài hạn trong URL hoặc `localStorage`.
4. Giao thức phản hồi riêng cho `command_id` và trạng thái đầu ra; nếu cần xác nhận bơm/contactor thật phải có phản hồi vật lý, không suy ra từ LED hoặc MQTT status.
5. Lưu `sequence`/chống replay bền vững nếu cần chống lệnh cũ qua reboot. Bản hiện tại mới dedupe ID gần nhất trong RAM và chưa kiểm tra TTL dựa trên đồng hồ đáng tin cậy.

## Cấu hình kiểm thử hiện tại
- Firmware: `AquaAuto_Gateway_Connectivity_Test_v1.ino`
- PWA: `index.html`
- Service worker cache version: `aquaauto-shell-v2` (nên tăng version khi phát hành cache mới).

## Kiểm thử thủ công trước khi merge
- Gửi ON/OFF và xác nhận `command_id` trùng khớp trong status.
- Mất Internet/MQTT rồi kết nối lại; UI phải không hiển thị trạng thái cũ như trạng thái mới.
- Gửi lặp cùng `command_id`; firmware không được áp dụng lần hai.
- Gửi thiếu `command_id`, JSON sai, node sai, cmd không hỗ trợ.
- Kiểm tra timeout lệnh, nhiều điện thoại cùng mở app, khởi động lại Gateway.
- Thử trên đúng bo mạch trước khi dựa vào LED, chân BOOT hoặc nguồn; firmware này chỉ điều khiển LED onboard, không điều khiển tải thật.

## Ghi chú
Đây là bản sửa dựa trên ZIP được cung cấp trong cuộc trò chuyện; chưa được biên dịch bằng Arduino toolchain, chưa chạy trên phần cứng và chưa được triển khai lên GitHub Pages.
