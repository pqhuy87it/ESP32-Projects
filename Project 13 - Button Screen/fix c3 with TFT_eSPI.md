Config này rất giá trị — và nó giúp tôi chốt được chẩn đoán.

## Con số 0x10 nói lên điều gì

Trên ESP32-C3, ngoại vi GPSPI2 có bản đồ thanh ghi: offset `0x00` là `SPI_CMD_REG`, `0x0C` là `SPI_CLOCK_REG`, và **`0x10` chính là `SPI_USER_REG`**.

TFT_eSPI tính con trỏ thanh ghi bằng `_spi_user = (volatile uint32_t*)(SPI_USER_REG(SPI_PORT))`, tức là `REG_SPI_BASE(2) + 0x10`. Bạn bị store fault đúng tại `0x10` — nghĩa là **`REG_SPI_BASE(2)` đang trả về 0**. Thư viện ghi vào thanh ghi SPI_USER với địa chỉ nền bằng không.

Đó chính xác là thứ đoạn vá trong thread kia xử lý:
```cpp
#ifndef REG_SPI_BASE
#define REG_SPI_BASE(i) DR_REG_SPI2_BASE
#endif
```
Guard `#ifndef` này thất bại khi ESP-IDF mới đã định nghĩa sẵn `REG_SPI_BASE` — nên bản vá không được áp dụng, và giá trị của IDF cho index 2 lại sai trên C3.

## So sánh Setup 424 với Setup900 của tôi

| Mục | Setup 424 (chạy được) | Setup900 |
|---|---|---|
| `TFT_MISO` | 5 — chân thật, không nối | 10 (tôi đã sửa) |
| `TFT_CS` | **7 — chân thật** | **-1** |
| `TFT_BL` | **không định nghĩa** | 3 |
| `TFT_SCLK` / `TFT_MOSI` | 4 / 6 | 6 / 7 |
| `TFT_RGB_ORDER` | không định nghĩa | `TFT_BGR` |
| `SPI_READ_FREQUENCY` | không định nghĩa | 20000000 |

Hai khác biệt cấu trúc đáng ngờ nhất là **`TFT_CS = -1`** và **`TFT_BL` được định nghĩa**. Mọi nhánh `-1` trong TFT_eSPI đều là code hiếm được test, và người kia né sạch chúng bằng cách gán chân thật cho mọi thứ.

Cách đúng bây giờ là **sao chép nguyên xi cấu hình đã chạy**, chỉ đổi độ phân giải. Sau khi màn lên rồi mới dịch từng chân một nếu muốn.## Đấu lại dây theo cấu hình mới

| Màn ST7789 | ESP32-C3 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | **GPIO4** |
| SDA | **GPIO6** |
| RES | **GPIO3** |
| DC | **GPIO2** |
| BLK | **3V3** (không qua GPIO nữa) |
| — | GPIO5 và GPIO7 khai báo nhưng để trống |

Switch vẫn giữ nguyên GPIO0 → GND.

**Về GPIO2 làm DC:** đúng là strapping pin, nhưng ràng buộc chỉ áp dụng ở *thời điểm reset*. Lúc đó chân đang ở chế độ input thả nổi, và đầu DC của màn hình là trở kháng cao nên không kéo nó xuống. Sau khi boot xong nó mới bị drive. An toàn — và đã được xác nhận chạy trong cấu hình 424.

## Nếu vẫn crash

Lúc đó không còn cách nào ngoài vá thư viện. Mở `TFT_eSPI/Processors/TFT_eSPI_ESP32_C3.h`, tìm khối `REG_SPI_BASE` và đổi từ `#ifndef` sang ép định nghĩa lại:

```cpp
#undef  REG_SPI_BASE
#define REG_SPI_BASE(i) DR_REG_SPI2_BASE
```

Nếu chẩn đoán của tôi đúng, đây mới là bản vá tận gốc — hai thứ kia chỉ là né tránh. Sau khi vá nhớ chép file vào `tft-setups/` cùng script sync, vì update thư viện sẽ ghi đè.

Nạp lại và gửi tôi log nhé. Điều tôi cần biết là bạn có thấy dòng `<<< tft.init() da tra ve OK` không — nếu có thì phần còn lại chỉ là chỉnh màu với offset.