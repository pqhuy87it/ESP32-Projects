#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <AnimatedGIF.h>

#define SD_CS 16
#define SD_MOSI 17
#define SD_MISO 18
#define SD_SCK 8

TFT_eSPI tft = TFT_eSPI();
SPIClass spiSD(FSPI);
AnimatedGIF gif;

uint16_t *canvas;
#define DW 320
#define DH 240

// Nguong toi da cho 1 file GIF nap vao PSRAM (3MB). Lon hon -> fallback SD.
#define MAX_GIF_RAM (3 * 1024 * 1024)

#define MAX_FILES 150
String gifFiles[MAX_FILES];
int fileCount = 0;
int currentIndex = 0;
int menuOffset = 0;
bool autoMode = true;
bool forceNext = false;

int frame_min_x = DW, frame_min_y = DH;
int frame_max_x = 0, frame_max_y = 0;
bool frame_drawn = false;

// Offset de canh giua GIF nho hon man hinh
int gif_offset_x = 0, gif_offset_y = 0;

// Bien do FPS
unsigned long fps_lastTime = 0;
int fps_frameCount = 0;
int fps_value = 0;

// Giam tan suat doc cam ung (doc moi 3 frame)
int touchSkip = 0;

// ---- Double buffer nap GIF vao PSRAM ----
// Hai slot: mot slot dang phat, mot slot nap truoc file ke.
// Khi chuyen file chi can doi con tro -> gan nhu tuc thoi.
struct GifBuf {
  uint8_t *data = NULL;   // buffer PSRAM chua file
  int32_t size = 0;       // kich thuoc file da nap
  int index = -1;         // index file dang chua trong slot (-1 = trong)
  bool valid = false;     // nap thanh cong & vua RAM (false -> phai fallback SD)
};
GifBuf bufA, bufB;
GifBuf *curBuf = &bufA;    // slot dang phat
GifBuf *nextBuf = &bufB;   // slot nap truoc

// Con tro doc cho callback RAM (tro vao curBuf khi phat)
int32_t gifRamPos = 0;

// File handle cho fallback SD
File *gifFileHandle = NULL;

enum State { MENU, PLAYING };
State currentState = PLAYING;

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, ix;

  iWidth = pDraw->iWidth;
  ix = pDraw->iX + gif_offset_x;
  if (iWidth + ix > DW) iWidth = DW - ix;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y + gif_offset_y;

  if (y >= DH || y < 0 || ix >= DW || iWidth < 1) return;

  if (ix < frame_min_x) frame_min_x = ix;
  if (ix + iWidth > frame_max_x) frame_max_x = ix + iWidth;
  if (y < frame_min_y) frame_min_y = y;
  if (y > frame_max_y) frame_max_y = y;
  frame_drawn = true;

  s = pDraw->pPixels;
  d = &canvas[y * DW + ix];

  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent) s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency) {
    uint8_t ucTransparent = pDraw->ucTransparent;
    for (x = 0; x < iWidth; x++) {
      if (s[x] != ucTransparent) {
        uint16_t col = usPalette[s[x]];
        d[x] = (col >> 8) | (col << 8);
      }
    }
  } else {
    for (x = 0; x < iWidth; x++) {
      uint16_t col = usPalette[s[x]];
      d[x] = (col >> 8) | (col << 8);
    }
  }
}

// ---- Callback doc tu RAM (curBuf) ----
void *GIFOpenRam(const char *fname, int32_t *pSize) {
  gifRamPos = 0;
  *pSize = curBuf->size;
  return (void *)curBuf->data;
}
void GIFCloseRam(void *pHandle) {}

int32_t GIFReadRam(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t remain = curBuf->size - gifRamPos;
  if (iLen > remain) iLen = remain;
  if (iLen <= 0) return 0;
  memcpy(pBuf, curBuf->data + gifRamPos, iLen);
  gifRamPos += iLen;
  pFile->iPos = gifRamPos;
  return iLen;
}

int32_t GIFSeekRam(GIFFILE *pFile, int32_t iPosition) {
  if (iPosition < 0) iPosition = 0;
  if (iPosition > curBuf->size) iPosition = curBuf->size;
  gifRamPos = iPosition;
  pFile->iPos = gifRamPos;
  return gifRamPos;
}

// ---- Callback doc tu SD (fallback) ----
void *GIFOpenFile(const char *fname, int32_t *pSize) {
  if (gifFileHandle) {
    gifFileHandle->close();
    delete gifFileHandle;
    gifFileHandle = NULL;
  }
  File f = SD.open(fname);
  if (f) {
    *pSize = f.size();
    gifFileHandle = new File(std::move(f));
    return (void *)gifFileHandle;
  }
  return NULL;
}
void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f) {
    f->close();
    delete f;
    if (f == gifFileHandle) gifFileHandle = NULL;
  }
}
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  if ((pFile->iSize - pFile->iPos) < iLen) iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0) return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}
void closeStrayHandle() {
  if (gifFileHandle) {
    gifFileHandle->close();
    delete gifFileHandle;
    gifFileHandle = NULL;
  }
}

// ---- Quan ly buffer PSRAM ----
void freeBuf(GifBuf *b) {
  if (b->data) {
    free(b->data);
    b->data = NULL;
  }
  b->size = 0;
  b->index = -1;
  b->valid = false;
}

// Nap file index vao slot b. Set b->valid=false neu qua lon/loi (se fallback SD).
// Luon set b->index de biet slot dang giu file nao.
void loadFileToBuf(GifBuf *b, int index) {
  // Neu slot da dang giu dung file nay va hop le -> khong nap lai
  if (b->index == index && b->valid && b->data) return;

  freeBuf(b);
  b->index = index;

  File f = SD.open(gifFiles[index].c_str());
  if (!f) return;                       // valid van = false -> fallback SD

  int32_t sz = f.size();
  if (sz <= 0 || sz > MAX_GIF_RAM) {    // qua lon
    f.close();
    return;
  }

  b->data = (uint8_t *)ps_malloc(sz);
  if (!b->data) {                       // het RAM
    f.close();
    return;
  }

  int32_t readTotal = 0;
  const int32_t CHUNK = 8192;
  while (readTotal < sz) {
    int32_t want = sz - readTotal;
    if (want > CHUNK) want = CHUNK;
    int32_t got = f.read(b->data + readTotal, want);
    if (got <= 0) break;
    readTotal += got;
  }
  f.close();

  if (readTotal != sz) {                // doc thieu
    freeBuf(b);
    b->index = index;                   // van nho index de fallback
    return;
  }

  b->size = sz;
  b->valid = true;
}

void showFPS() {
  fps_frameCount++;
  unsigned long now = millis();
  if (now - fps_lastTime >= 1000) {
    fps_value = fps_frameCount;
    fps_frameCount = 0;
    fps_lastTime = now;
    tft.fillRect(255, 2, 63, 18, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(TR_DATUM);
    char buf[16];
    sprintf(buf, "%d FPS", fps_value);
    tft.drawString(buf, 316, 3, 2);
  }
}

void drawMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRoundRect(5, 5, 310, 230, 8, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("GIF SELECTOR", 160, 25, 4);

  for (int i = 0; i < 5; i++) {
    int fileIdx = i + menuOffset;
    if (fileIdx >= fileCount) break;
    int yPos = 60 + (i * 30);
    if (fileIdx == currentIndex) {
      tft.fillRoundRect(15, yPos - 12, 210, 25, 4, TFT_NAVY);
      tft.setTextColor(TFT_CYAN);
    } else {
      tft.setTextColor(TFT_WHITE);
    }
    tft.setTextDatum(ML_DATUM);
    tft.drawString(gifFiles[fileIdx].substring(1, 20), 25, yPos, 2);
  }

  tft.fillRoundRect(240, 60, 65, 40, 5, TFT_DARKGREY);
  tft.fillRoundRect(240, 110, 65, 40, 5, TFT_DARKGREY);
  tft.fillRoundRect(240, 170, 65, 45, 5, autoMode ? TFT_GREEN : TFT_MAROON);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("UP", 272, 80, 2);
  tft.drawString("DOWN", 272, 130, 2);
  tft.drawString(autoMode ? "AUTO" : "SINGL", 272, 192, 2);
}

void handleTouch() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    if (currentState == PLAYING) {
      if (x < 80) {
        currentIndex = (currentIndex - 1 + fileCount) % fileCount;
        forceNext = true;
      } else if (x > 240) {
        currentIndex = (currentIndex + 1) % fileCount;
        forceNext = true;
      } else {
        currentState = MENU;
        drawMenu();
      }
      delay(200);
    } else {
      if (x > 230) {
        if (y < 100) {
          if (menuOffset > 0) menuOffset--;
        } else if (y < 160) {
          if (menuOffset < fileCount - 5) menuOffset++;
        } else {
          autoMode = !autoMode;
        }
        drawMenu();
      } else if (x < 220 && y > 50) {
        int selection = ((y - 50) / 30) + menuOffset;
        if (selection < fileCount) {
          currentIndex = selection;
          currentState = PLAYING;
          forceNext = true;
          tft.fillScreen(TFT_BLACK);
          memset(canvas, 0, DW * DH * 2);
        }
      }
      delay(150);
    }
  }
}

// Sort ten file tang dan (bubble sort don gian, chi chay 1 lan luc boot)
void sortFiles() {
  for (int i = 0; i < fileCount - 1; i++) {
    for (int j = 0; j < fileCount - 1 - i; j++) {
      if (gifFiles[j] > gifFiles[j + 1]) {
        String t = gifFiles[j];
        gifFiles[j] = gifFiles[j + 1];
        gifFiles[j + 1] = t;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // khong block khi buffer day / dong Serial Monitor
  delay(1000);
  Serial.println("\n===== BOOT =====");

  if (!psramInit()) {
    Serial.println("PSRAM Fail");
    while (1)
      ;
  }
  canvas = (uint16_t *)ps_malloc(DW * DH * 2);
  memset(canvas, 0, DW * DH * 2);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  delay(100);
  uint16_t calData[5] = { 351, 3506, 363, 3267, 2 };
  tft.setTouch(calData);
  delay(100);

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    tft.println("SD Error");
    Serial.println("SD Error");
    while (1)
      ;
  }

  File root = SD.open("/");
  while (File entry = root.openNextFile()) {
    String n = entry.name();
    // Bo qua file rac macOS (._xxx), file an, thu muc he thong
    if (n.startsWith("._") || n.startsWith(".") || n.startsWith("__MACOSX")) {
      entry.close();
      continue;
    }
    if (n.endsWith(".gif") || n.endsWith(".GIF")) {
      if (fileCount < MAX_FILES) gifFiles[fileCount++] = "/" + n;
    }
    entry.close();
  }
  root.close();

  sortFiles();   // sap xep theo ten cho de theo doi

  Serial.print("So file GIF tim thay: ");
  Serial.println(fileCount);

  gif.begin(LITTLE_ENDIAN_PIXELS);

  // Nap truoc file dau tien vao slot hien tai
  if (fileCount > 0) loadFileToBuf(curBuf, currentIndex);
}

// Mo GIF tu slot b: neu b hop le -> doc RAM, khong thi fallback SD.
bool openFromBuf(GifBuf *b) {
  if (b->valid && b->data) {
    curBuf = b;   // tro callback RAM vao slot nay
    return gif.open(gifFiles[b->index].c_str(),
                    GIFOpenRam, GIFCloseRam, GIFReadRam, GIFSeekRam, GIFDraw);
  } else {
    Serial.println("Fallback: doc truc tiep tu SD");
    return gif.open(gifFiles[b->index].c_str(),
                    GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw);
  }
}

void loop() {
  if (currentState == PLAYING) {
    if (fileCount > 0) {
      forceNext = false;

      // Dam bao slot hien tai dang giu dung file currentIndex
      if (curBuf->index != currentIndex) {
        // Neu file can nam o slot next (da nap truoc) -> doi slot
        if (nextBuf->index == currentIndex) {
          GifBuf *tmp = curBuf;
          curBuf = nextBuf;
          nextBuf = tmp;
        } else {
          loadFileToBuf(curBuf, currentIndex);
        }
      }

      bool opened = openFromBuf(curBuf);

      if (opened) {
        int gw = gif.getCanvasWidth();
        int gh = gif.getCanvasHeight();
        gif_offset_x = (DW - gw) / 2;
        gif_offset_y = (DH - gh) / 2;
        if (gif_offset_x < 0) gif_offset_x = 0;
        if (gif_offset_y < 0) gif_offset_y = 0;

        memset(canvas, 0, DW * DH * 2);
        tft.fillScreen(TFT_BLACK);

        frame_min_x = DW;
        frame_max_x = 0;
        frame_min_y = DH;
        frame_max_y = 0;
        frame_drawn = false;

        // Nap truoc file ke tiep vao slot next (trong luc dang phat).
        // Chi nap 1 lan ngay dau, tranh doc SD lien tuc moi frame.
        bool prefetched = false;
        int nextIndex = (currentIndex + 1) % fileCount;

        int delayMs = 0;
        while (gif.playFrame(false, &delayMs)) {

          unsigned long frameStart = millis();

          if (frame_drawn) {
            int w = frame_max_x - frame_min_x;
            int h = frame_max_y - frame_min_y + 1;
            tft.startWrite();
            if (frame_min_x == 0 && frame_max_x >= DW) {
              tft.setAddrWindow(0, frame_min_y, DW, h);
              tft.pushPixels(&canvas[frame_min_y * DW], DW * h);
            } else {
              tft.setAddrWindow(frame_min_x, frame_min_y, w, h);
              for (int yy = frame_min_y; yy <= frame_max_y; yy++) {
                tft.pushPixels(&canvas[yy * DW + frame_min_x], w);
              }
            }
            tft.endWrite();

            frame_min_x = DW;
            frame_max_x = 0;
            frame_min_y = DH;
            frame_max_y = 0;
            frame_drawn = false;
          }

          showFPS();

          if (++touchSkip >= 3) {
            handleTouch();
            touchSkip = 0;
          }
          if (forceNext || currentState != PLAYING) break;

          // Nap truoc file ke sau khi da qua vai frame dau (cho anh on dinh)
          if (!prefetched && autoMode && fps_frameCount > 3) {
            if (nextBuf->index != nextIndex) {
              loadFileToBuf(nextBuf, nextIndex);
            }
            prefetched = true;
          }

          int elapsed = (int)(millis() - frameStart);
          int remain = delayMs - elapsed;
          if (remain > 0) {
            unsigned long waitStart = millis();
            while ((int)(millis() - waitStart) < remain) {
              if (++touchSkip >= 3) {
                handleTouch();
                touchSkip = 0;
              }
              if (forceNext || currentState != PLAYING) break;
              delay(2);
            }
            if (forceNext || currentState != PLAYING) break;
          }
        }
        gif.close();

        // Dung ngan giua cac GIF (thay cho vong cho 2s cu)
        unsigned long holdStart = millis();
        while (millis() - holdStart < 300) {
          handleTouch();
          if (forceNext || currentState != PLAYING) break;
          delay(5);
        }

        if (autoMode && !forceNext && currentState == PLAYING) {
          currentIndex = (currentIndex + 1) % fileCount;
        }
      } else {
        Serial.print("Loi mo GIF: ");
        Serial.println(gifFiles[currentIndex]);
        closeStrayHandle();
        freeBuf(curBuf);
        handleTouch();
        currentIndex = (currentIndex + 1) % fileCount;
        delay(300);
      }
    } else {
      handleTouch();
      delay(50);
    }
  } else {
    handleTouch();
  }
}