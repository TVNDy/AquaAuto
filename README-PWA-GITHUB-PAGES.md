# AquaAuto PWA – GitHub Pages deployment (FIX4)

## Deploy đúng cấu trúc

Đây là gói dành cho GitHub Pages của repository `TVNDy/AquaAuto`.
Giải nén và chép **các file bên trong gói vào thư mục gốc của repository** (cùng cấp với `index.html` hiện tại), không chép nguyên thư mục ZIP lồng vào trong repository.

Cấu trúc cần có:

- `index.html`
- `manifest.json`
- `sw.js`
- `icons/icon-192.png`
- `icons/icon-512.png`
- các file firmware/tài liệu hiện có (nếu tiếp tục giữ trong repo)

Commit/push lên nhánh và thư mục mà GitHub Pages đang publish. Với cấu hình phổ biến của repo này, URL phải mở được là:

`https://tvndy.github.io/AquaAuto/`

Kiểm tra trực tiếp các URL sau phải trả về nội dung, không phải trang 404:

- `https://tvndy.github.io/AquaAuto/manifest.json`
- `https://tvndy.github.io/AquaAuto/sw.js`
- `https://tvndy.github.io/AquaAuto/icons/icon-192.png`
- `https://tvndy.github.io/AquaAuto/icons/icon-512.png`

## Cách kiểm tra trên Android Chrome

1. Mở URL GitHub Pages trực tiếp bằng ứng dụng Google Chrome, không mở trong trình duyệt nhúng của Zalo/Facebook.
2. Tải trang xong, chờ kết nối và Service Worker đăng ký; tải lại trang một lần.
3. Khi Chrome cấp `beforeinstallprompt`, popup AquaAuto hiển thị hướng dẫn và nút **Cài đặt**. Nhấn nút sẽ gọi prompt gốc của Chrome.
4. Nếu Chrome không cấp sự kiện, nhấn menu `⋮` của Chrome và tìm **Cài đặt ứng dụng** hoặc **Thêm vào màn hình chính**. Website không thể tự tạo hộp thoại gốc khi Chrome không cấp sự kiện.
5. Sau khi cài, mở AquaAuto từ biểu tượng màn hình chính. Popup đầu tiên sẽ không tự hiện lại; mục **Cài đặt ứng dụng** vẫn cho mở hướng dẫn bất cứ lúc nào.

## Ghi chú kỹ thuật

- Cache Service Worker được nâng lên `aquaauto-shell-v7`; đăng ký dùng `updateViaCache: 'none'` và gọi `registration.update()` để lấy bản SW mới.
- Trạng thái bỏ qua popup là theo trình duyệt/nguồn AquaAuto, không bị reset khi đổi Gateway hoặc Device Secret.
- Device Secret và pairing vẫn lưu theo logic cũ; thay đổi này không phải thay đổi giao thức MQTT.
- `beforeinstallprompt` do Chrome quản lý. Không có cách JavaScript hợp lệ để ép Chrome phát sự kiện hoặc tự cài không cần người dùng xác nhận.
- Đã kiểm tra cú pháp JavaScript của trang, cú pháp Service Worker và JSON manifest. Chưa xác minh deployment trên GitHub Pages hoặc kiểm thử trên điện thoại thật.
