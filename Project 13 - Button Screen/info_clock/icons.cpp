#include "icons.h"
#include "hal.h"
#include "theme.h"

// Ten file thong nhat voi cac du an khac: weather_icons.h / weather_icons_sm.h
#if __has_include("weather_icons.h")
  #include "weather_icons.h"
  #define HAVE_BITMAP_ICONS 1
#else
  #define HAVE_BITMAP_ICONS 0
#endif

// Bo icon nho cho hang du bao. Sinh bang:
//   python3 png_to_rgb565.py --size 30 --symbol-prefix ICONSM_ \
//       --size-name WX_ICON_SM_SIZE --out weather_icons_sm.h icon_*.png
#if __has_include("weather_icons_sm.h")
  #include "weather_icons_sm.h"
  #define HAVE_SM_ICONS 1
#else
  #define HAVE_SM_ICONS 0
#endif

int iconsBigSize() {
#if HAVE_BITMAP_ICONS
  return WX_ICON_SIZE;
#else
  return 0;
#endif
}

int iconsSmallSize() {
#if HAVE_SM_ICONS
  return WX_ICON_SM_SIZE;
#else
  return 0;
#endif
}

bool iconsUsingBitmaps() { return HAVE_BITMAP_ICONS || HAVE_SM_ICONS; }

// Bo bitmap gom 6 icon: SUNNY, SUNNY_CLOUD, CLOUDY, RAINNY, HAZE, WINDY.
// Mat trang, sam set va TUYET khong co bitmap nen luon ve bang code —
// rieng tuyet la co y, vi Ha Noi khong bao gio co tuyet.
static const uint16_t* bitmapFor(WxIcon icon) {
#if HAVE_BITMAP_ICONS
  switch (icon) {
    case WxIcon::Clear:        return ICON_SUNNY;
    case WxIcon::PartlyCloudy: return ICON_SUNNY_CLOUD;
    case WxIcon::Cloudy:       return ICON_CLOUDY;
    case WxIcon::Rain:         return ICON_RAINNY;
    case WxIcon::Wind:         return ICON_WINDY;
    case WxIcon::Fog:          return ICON_HAZE;
    default:                   return nullptr;
  }
#else
  (void)icon;
  return nullptr;
#endif
}

// grow > 0 de ve vien nen quanh may, tach may khoi mat troi phia sau
static void cloudShape(int x, int y, int w, int h, uint16_t c, int grow) {
  const int bh    = h / 3;
  const int baseY = y + h - bh;
  tft.fillRect(x + w / 8 - grow, baseY - grow,
               w - w / 4 + 2 * grow, bh + grow, c);
  tft.fillCircle(x + w / 4,     baseY,         h / 4 + grow, c);
  tft.fillCircle(x + w / 2,     y + h / 2,     h / 3 + grow, c);
  tft.fillCircle(x + 3 * w / 4, baseY - h / 8, h / 4 + grow, c);
}

static void sunShape(int cx, int cy, int r, uint16_t c) {
  tft.fillCircle(cx, cy, r, c);
  for (int i = 0; i < 8; i++) {
    const float a  = i * 0.7853981634f;              // PI/4
    const float ca = cosf(a), sa = sinf(a);
    const int x1 = cx + (int)(ca * (r + 3)), y1 = cy + (int)(sa * (r + 3));
    const int x2 = cx + (int)(ca * (r + 7)), y2 = cy + (int)(sa * (r + 7));
    tft.drawLine(x1,     y1, x2,     y2, c);         // ve 2 lan lech 1px
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, c);         // cho tia day hon
  }
}

static void drawByCode(WxIcon icon, int x, int y, int box, uint16_t c) {
  const int cx = x + box / 2;
  const int cy = y + box / 2;

  switch (icon) {
    case WxIcon::Clear:
      sunShape(cx, cy, box / 4, c);
      break;

    case WxIcon::NightClear:                          // luoi liem
      tft.fillCircle(cx, cy, box / 3, c);
      tft.fillCircle(cx + box / 6, cy - box / 10, box / 3, P.bg);
      break;

    case WxIcon::PartlyCloudy:
      sunShape(x + box - box / 4, y + box / 4, box / 6, c);
      cloudShape(x, y + box / 5, box - box / 6, box / 2, P.bg, 3);
      cloudShape(x, y + box / 5, box - box / 6, box / 2, c,    0);
      break;

    case WxIcon::Cloudy:
      cloudShape(x, y + box / 5, box, box / 2, c, 0);
      break;

    case WxIcon::Rain:
      cloudShape(x, y, box, box / 2, c, 0);
      for (int i = 0; i < 3; i++) {
        const int dx = x + box / 5 + i * (box / 4);
        tft.drawLine(dx,     y + box / 2 + 4, dx - 4, y + box - 2, c);
        tft.drawLine(dx + 1, y + box / 2 + 4, dx - 3, y + box - 2, c);
      }
      break;

    case WxIcon::Storm:
      cloudShape(x, y, box, box / 2, c, 0);
      tft.fillTriangle(cx - 2, y + box / 2 + 2, cx + 7, y + box / 2 + 2,
                       cx - 1, y + box - 10, c);
      tft.fillTriangle(cx - 5, y + box - 12, cx + 4, y + box - 12,
                       cx - 3, y + box - 1,  c);
      break;

    case WxIcon::Snow:
      cloudShape(x, y, box, box / 2, c, 0);
      for (int i = 0; i < 3; i++) {
        const int sx = x + box / 5 + i * (box / 4);
        const int sy = y + box - 8;
        tft.drawFastHLine(sx - 4, sy, 9, c);
        tft.drawFastVLine(sx, sy - 4, 9, c);
        tft.drawLine(sx - 3, sy - 3, sx + 3, sy + 3, c);
        tft.drawLine(sx - 3, sy + 3, sx + 3, sy - 3, c);
      }
      break;

    case WxIcon::Fog:
      for (int i = 0; i < 4; i++) {
        const int fy    = y + box / 4 + i * (box / 7);
        const int inset = (i % 2) ? 6 : 0;
        tft.fillRoundRect(x + inset, fy, box - 2 * inset, 4, 2, c);
      }
      break;

    case WxIcon::Wind: {
      // Ba luong gio dai ngan khac nhau, luong giua co moc cuon o dau
      const int t  = (box >= 30) ? 4 : 3;
      const int y0 = y + box / 4;
      const int st = box / 5;
      tft.fillRoundRect(x + 3,     y0,          box - 12, t, t / 2, c);
      tft.fillRoundRect(x + 3,     y0 + st,     box - 6,  t, t / 2, c);
      tft.fillRoundRect(x + 3,     y0 + 2 * st, box - 16, t, t / 2, c);
      // Moc cuon cuoi luong dai nhat
      const int hx = x + box - 6, hy = y0 + st;
      tft.fillRoundRect(hx - t, hy - st / 2, t, st / 2 + t, t / 2, c);
      break;
    }
  }
}

#if HAVE_BITMAP_ICONS && defined(ICON_HAS_ALPHA)
static const uint8_t* alphaFor(WxIcon icon) {
  switch (icon) {
    case WxIcon::Clear:        return ICON_SUNNY_A;
    case WxIcon::PartlyCloudy: return ICON_SUNNY_CLOUD_A;
    case WxIcon::Cloudy:       return ICON_CLOUDY_A;
    case WxIcon::Rain:         return ICON_RAINNY_A;
    case WxIcon::Wind:         return ICON_WINDY_A;
    case WxIcon::Fog:          return ICON_HAZE_A;
    default:                   return nullptr;
  }
}
#endif

#if HAVE_SM_ICONS && defined(ICONSM_HAS_ALPHA)
static const uint8_t* alphaSmallFor(WxIcon icon) {
  switch (icon) {
    case WxIcon::Clear:        return ICONSM_SUNNY_A;
    case WxIcon::PartlyCloudy: return ICONSM_SUNNY_CLOUD_A;
    case WxIcon::Cloudy:       return ICONSM_CLOUDY_A;
    case WxIcon::Rain:         return ICONSM_RAINNY_A;
    case WxIcon::Wind:         return ICONSM_WINDY_A;
    case WxIcon::Fog:          return ICONSM_HAZE_A;
    default:                   return nullptr;
  }
}
#endif

#if HAVE_SM_ICONS
static const uint16_t* bitmapSmallFor(WxIcon icon) {
  switch (icon) {
    case WxIcon::Clear:        return ICONSM_SUNNY;
    case WxIcon::PartlyCloudy: return ICONSM_SUNNY_CLOUD;
    case WxIcon::Cloudy:       return ICONSM_CLOUDY;
    case WxIcon::Rain:         return ICONSM_RAINNY;
    case WxIcon::Wind:         return ICONSM_WINDY;
    case WxIcon::Fog:          return ICONSM_HAZE;
    default:                   return nullptr;
  }
}
#endif

// ---------------------------------------------------------------------------
// Thu nho bitmap luc chay
//
// Truoc day chi dung bitmap khi o ve du rong, nen hau het man hinh (o 22..34px)
// bi roi ve ve bang code du da co file icon. Gio bitmap duoc thu nho ve dung
// kich thuoc o.
//
// Dung box filter (lay trung binh cac pixel nguon roi vao moi pixel dich) chu
// khong phai lay mau gan nhat — icon goc co khu rang cua, lay mau gan nhat se
// lam no vo hat rat ro o kich thuoc nho.
// ---------------------------------------------------------------------------
static constexpr int ICON_BUF_MAX = 48;
static uint16_t iconBuf[ICON_BUF_MAX * ICON_BUF_MAX];   // 4.6 KB tinh

// Thu nho + tron alpha trong mot luot.
//
// RGB trong file da duoc NHAN TRUOC voi alpha (ghep san len nen den), nen
// cong thuc ghep len nen bat ky rat gon:
//
//     out = rgb + bg * (255 - a) / 255
//
// Dang nhan truoc cung cho phep lay trung binh rgb va alpha DOC LAP khi thu
// nho ma van dung — day chinh la ly do premultiplied alpha ton tai.
static void drawScaledBitmap(const uint16_t* src, const uint8_t* srcA,
                             int ss, int x, int y, int box, uint16_t bg) {
  if (box > ICON_BUF_MAX) box = ICON_BUF_MAX;

  const uint32_t bgR = (bg >> 11) & 0x1F;
  const uint32_t bgG = (bg >>  5) & 0x3F;
  const uint32_t bgB =  bg        & 0x1F;

  for (int dy = 0; dy < box; dy++) {
    int sy0 = dy * ss / box;
    int sy1 = ((dy + 1) * ss + box - 1) / box;
    if (sy1 <= sy0) sy1 = sy0 + 1;
    if (sy1 > ss)   sy1 = ss;

    for (int dx = 0; dx < box; dx++) {
      int sx0 = dx * ss / box;
      int sx1 = ((dx + 1) * ss + box - 1) / box;
      if (sx1 <= sx0) sx1 = sx0 + 1;
      if (sx1 > ss)   sx1 = ss;

      uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
      for (int sy = sy0; sy < sy1; sy++) {
        for (int sx = sx0; sx < sx1; sx++) {
          const int      idx = sy * ss + sx;
          const uint16_t c   = src[idx];
          r += (c >> 11) & 0x1F;
          g += (c >>  5) & 0x3F;
          b +=  c        & 0x1F;
          a += srcA ? srcA[idx] : 255;
          n++;
        }
      }
      r /= n; g /= n; b /= n; a /= n;

      if (a < 255) {                       // ghep phan trong suot len nen
        const uint32_t inv = 255 - a;
        r += bgR * inv / 255;
        g += bgG * inv / 255;
        b += bgB * inv / 255;
        if (r > 0x1F) r = 0x1F;
        if (g > 0x3F) g = 0x3F;
        if (b > 0x1F) b = 0x1F;
      }
      iconBuf[dy * box + dx] = (uint16_t)((r << 11) | (g << 5) | b);
    }
  }
  tft.pushImage(x, y, box, box, iconBuf);
}

// Chon bo bitmap gan kich thuoc o nhat, uu tien bo LON HON o de thu nho
// (phong to len se mo), roi ve. Khong co bitmap nao thi ve bang code.
void iconWeatherOn(WxIcon icon, int x, int y, int box,
                   uint16_t color, uint16_t bg) {
  const uint16_t* src     = nullptr;
  const uint8_t*  srcA    = nullptr;
  int             srcSize = 0;

#if HAVE_SM_ICONS
  if (const uint16_t* sm = bitmapSmallFor(icon)) {
    src = sm; srcSize = WX_ICON_SM_SIZE;
  #ifdef ICONSM_HAS_ALPHA
    srcA = alphaSmallFor(icon);
  #endif
  }
#endif
#if HAVE_BITMAP_ICONS
  if (const uint16_t* bgm = bitmapFor(icon)) {
    if (!src || srcSize < box) {
      src = bgm; srcSize = WX_ICON_SIZE;
    #ifdef ICON_HAS_ALPHA
      srcA = alphaFor(icon);
    #else
      srcA = nullptr;
    #endif
    }
  }
#endif

  if (src && srcSize > 0) {
    drawScaledBitmap(src, srcA, srcSize, x, y, box, bg);
    return;
  }
  drawByCode(icon, x, y, box, color);
}

void iconWeather(WxIcon icon, int x, int y, int box, uint16_t color) {
  iconWeatherOn(icon, x, y, box, color, P.bg);
}

void iconDroplet(int cx, int cy, int w, uint16_t color) {
  const int r = w / 2;
  tft.fillTriangle(cx, cy - w, cx - r, cy + r / 2, cx + r, cy + r / 2, color);
  tft.fillCircle(cx, cy + r / 2, r, color);
}

// Font TFT_eSPI khong co ky tu do, nen ve bang hai vong tron long nhau
void iconDegreeRing(int cx, int cy, int r, uint16_t color) {
  tft.drawCircle(cx, cy, r,     color);
  tft.drawCircle(cx, cy, r - 1, color);
}
