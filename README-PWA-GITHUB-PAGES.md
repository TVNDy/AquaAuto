# AquaAuto PWA – GitHub Pages (FIX6)

## Cập nhật
Giải nén và chép toàn bộ nội dung gói vào thư mục gốc repository `TVNDy/AquaAuto`, ghi đè file cũ, sau đó commit/push.

## Kiểm tra
- `https://tvndy.github.io/AquaAuto/`
- `https://tvndy.github.io/AquaAuto/manifest.json`
- `https://tvndy.github.io/AquaAuto/sw.js`
- `https://tvndy.github.io/AquaAuto/icons/icon-192.png`
- `https://tvndy.github.io/AquaAuto/icons/icon-512.png`

## Hướng dẫn ghim
- Không tự bật popup khi mở trang.
- Banner ngắn nằm dưới tiêu đề, dùng chung trên cả ba tab.
- Chạm banner để xem hướng dẫn; nhấn × để ẩn trên trình duyệt hiện tại.
- Banner tự ẩn khi ứng dụng chạy ở chế độ đã cài.
- Mục Hỗ trợ trong Cài đặt vẫn mở lại hướng dẫn.
- Nếu Chrome cấp `beforeinstallprompt`, nút Cài đặt gọi hộp thoại gốc; nếu không, hiển thị đường dẫn menu Chrome.

## Lưu ý
Website không thể tự cài ứng dụng hoặc ép Chrome mở hộp thoại gốc. Trên Android Chrome, người dùng cần xác nhận cài đặt. Trạng thái nhấn × được lưu bằng localStorage trên trình duyệt hiện tại.

Service Worker cache: `aquaauto-shell-v9`.
Đã kiểm tra cú pháp JavaScript, JSON manifest và Service Worker. Chưa triển khai GitHub Pages hoặc kiểm thử điện thoại thật.
