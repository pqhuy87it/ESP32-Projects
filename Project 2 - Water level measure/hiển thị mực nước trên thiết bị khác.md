Được, hoàn toàn làm được. Có 3 hướng, khác nhau về độ phức tạp và độ trễ:

## 1. Arduino Cloud REST API (ESP32 tự gọi HTTPS)

Đây là cách "lấy data từ dashboard" đúng nghĩa. Flow:

1. Vào Arduino Cloud → **API keys** tạo `client_id` + `client_secret`
2. ESP32 lấy OAuth2 token: POST `https://api2.arduino.cc/iot/v1/clients/token` với `grant_type=client_credentials`, `client_id`, `client_secret`, `audience=https://api2.arduino.cc/iot`
3. GET `https://api2.arduino.cc/iot/v2/things/{thing_id}/properties/{property_id}` → JSON có `last_value`

```cpp
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* CLIENT_ID     = "...";
const char* CLIENT_SECRET = "...";
const char* THING_ID      = "...";   // lấy trong URL của Thing
const char* PROPERTY_ID   = "...";   // click vào variable -> ID

String token;
uint32_t tokenExpireAt = 0;   // millis

bool refreshToken() {
  WiFiClientSecure c; c.setInsecure();          // prod: nạp root CA
  HTTPClient http;
  http.begin(c, "https://api2.arduino.cc/iot/v1/clients/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = String("grant_type=client_credentials&client_id=") + CLIENT_ID +
                "&client_secret=" + CLIENT_SECRET +
                "&audience=https%3A%2F%2Fapi2.arduino.cc%2Fiot";

  int code = http.POST(body);
  if (code != 200) { http.end(); return false; }

  JsonDocument doc;
  deserializeJson(doc, http.getString());
  http.end();

  token = doc["access_token"].as<String>();
  uint32_t ttl = doc["expires_in"] | 300;       // thường 300s
  tokenExpireAt = millis() + (ttl - 30) * 1000; // refresh sớm 30s
  return true;
}

bool readLevel(float &value) {
  if (millis() > tokenExpireAt && !refreshToken()) return false;

  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, String("https://api2.arduino.cc/iot/v2/things/") +
                THING_ID + "/properties/" + PROPERTY_ID);
  http.addHeader("Authorization", "Bearer " + token);

  if (http.GET() != 200) { http.end(); return false; }
  JsonDocument doc;
  deserializeJson(doc, http.getString());
  http.end();

  value = doc["last_value"].as<float>();
  return true;
}
```

Lưu ý thực tế:
- Token sống rất ngắn (~5 phút) → phải cache + refresh, đừng xin token mỗi lần đọc.
- API rate limit 10 req/s, nhưng poll 15–60s là hợp lý cho mực nước.
- Đây là **polling**, không realtime. Muốn thấy ngay khi mực nước đổi thì dùng cách 2/3.
- Nên kiểm tra plan của bạn: REST API được liệt kê trong bảng feature của Arduino Cloud và một số tier miễn phí bị giới hạn, nếu gọi bị 403 thì lý do là chỗ này.

## 2. Bỏ qua cloud — MQTT broker riêng (khuyên dùng nếu bạn kiểm soát được board nguồn)

Sửa firmware board đo mực nước để **publish song song**: vừa lên Arduino Cloud, vừa lên broker riêng (Mosquitto trên Pi, hoặc HiveMQ public/cloud). Board hiển thị chỉ cần `subscribe`. Realtime, không token, không rate limit, không phụ thuộc plan. Với kinh nghiệm MQTT của bạn thì cái này mất 20 phút.

Nếu hai board cùng nhà và trong tầm sóng → **ESP-NOW** còn gọn hơn nữa, không cần WiFi/broker.

## 3. Webhook từ Arduino Cloud

Thing có mục **Webhooks**: mỗi lần variable đổi, Cloud POST JSON tới URL của bạn. Trỏ về một server nhỏ (Pi / VPS) rồi bridge sang MQTT nội bộ. Được realtime mà không phải sửa firmware board nguồn — hợp khi board đo đã đóng hộp, gắn ngoài trời rồi.

---

Tóm lại: nếu vẫn flash lại được board đo → **cách 2**. Nếu không đụng được vào nó nữa → **cách 1** (đơn giản nhất) hoặc **cách 3** (realtime hơn).

Bạn định làm board hiển thị trên phần cứng nào? Nếu là T-Display S3 hay con ideaspark 1.9" thì mình viết luôn sketch hoàn chỉnh kèm UI hiển thị mực nước dạng gauge/bar cho bạn.
