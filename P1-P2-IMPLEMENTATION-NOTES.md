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

## Field-test follow-up (2026-10-29)
- Wi-Fi captive portal: clear previous status/error banners at the beginning of a scan and after successful retrieval of scan results, so a stale “Hãy quét lại” message does not remain after a successful scan.
- Password visibility toggle: replace emoji with a monochrome inline SVG eye / eye-off icon and update its accessible label/state.
- PWA installation: when the browser still exposes its native `beforeinstallprompt` event, selecting “Cài đặt ứng dụng” in Settings invokes the native install prompt directly. If the browser does not expose that event (notably iPhone/iPad), show platform-specific manual steps; iOS installation still requires the user to confirm “Add to Home Screen” in Safari.
- Validation: JavaScript extracted from both the PWA and captive portal passed `node --check`. Firmware compilation and physical device verification remain outstanding.

## UI follow-up: Wi-Fi scan messaging

- Removed the generic red “Hãy quét lại” message from the normal scan/results flow.
- An empty scan result now uses a neutral inline hint: “Chưa tìm thấy mạng Wi-Fi. Bạn có thể nhấn ‘Quét Wi-Fi lại’.”
- Actual scan/API failures still show an error message with a concrete diagnostic direction.
- A previously selected SSID is preserved after rescanning when that network is still present.
- JavaScript syntax checked with `node --check`; firmware compilation and hardware validation remain pending.


## Cấu hình thử nghiệm hiện tại
- `DEVICE_SECRET` trong firmware thử nghiệm hiện được đặt là `2104`. Mã QR/đường dẫn PWA phải dùng `key=2104` để tạo đúng topic. Đây chỉ là mã ghép nối thử nghiệm, không phải thông tin xác thực an toàn cho triển khai thực tế.
- Trang cấu hình Wi-Fi đã thêm thử lại các yêu cầu HTTP ngắn hạn và thay lỗi trình duyệt thô `Failed to fetch` bằng thông báo tiếng Việt nếu Gateway vẫn không phản hồi.

## UI Fix 4 — Wi-Fi scan status handling
- Không hiển thị lỗi đỏ ngay khi một request fetch đầu tiên bị gián đoạn trong lúc captive portal vừa mở.
- Yêu cầu `/scan/start` được thử lại trong một khoảng thời gian; trạng thái quét được polling trước khi kết luận Gateway mất kết nối.
- Lỗi HTTP được phân biệt với lỗi mạng tạm thời; `409 scan_not_ready` khi lấy kết quả được xem là trạng thái chưa sẵn sàng và thử lại ngắn hạn.
- Chỉ hiển thị hướng dẫn trung tính khi danh sách Wi-Fi rỗng; chỉ hiển thị cảnh báo đỏ sau khi quét báo lỗi thật, quá thời gian, hoặc Gateway không phản hồi qua nhiều lần thử.
- Việc chưa nhập mật khẩu không tham gia vào trạng thái quét và không được coi là lỗi.


## FIX5 — sửa logic quét Wi-Fi và đọc kết quả (2026-10-30)

- Phân biệt rõ quét Wi-Fi xung quanh (scan) với kết nối Gateway vào Wi-Fi nhà/vườn (STA). Việc chưa nhập mật khẩu hoặc Gateway chưa kết nối Wi-Fi nhà không phải lỗi quét.
- `/scan/results` chỉ đọc kết quả, không xóa buffer và không tự chuyển trạng thái `DONE` về `IDLE`. Nhờ đó các yêu cầu đọc trùng/đồng thời không làm yêu cầu thứ hai nhận `409 scan_not_ready` giả.
- Mỗi lần nhấn quét lại sẽ khởi chạy một lượt quét mới (trừ khi lượt quét trước vẫn đang chạy); kết quả cũ được xóa tại thời điểm bắt đầu lượt mới.
- Trình duyệt khóa các lượt poll đồng thời và chỉ gọi đọc kết quả một lần khi trạng thái chuyển sang `done`.
- Chỉ hiện hướng dẫn trung tính khi danh sách rỗng; chỉ hiện cảnh báo khi Gateway trả lỗi quét, hết thời gian quét hoặc không thể liên lạc/đọc kết quả thật sự.
- Không thay đổi `DEVICE_SECRET` (vẫn là `2104`).
