#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include <JPEGDEC.h>
#include <DNSServer.h>

JPEGDEC jpeg;

const char* ssid = "KIT VISUAL";
const char* password = ""; // Open network
AsyncWebServer server(80);
DNSServer dnsServer;

String currentFile = "";
AnimatedGIF gif;
File gifFile;
File hqVideoFile;
bool isGifPlaying = false;
bool isHqVideoPlaying = false;
int hqFps = 20;
int loading_frame = 0;

uint16_t* back_buffer = nullptr; // PSRAM Back buffer for tearing prevention
uint16_t* hqv2_frame_buffer = nullptr; // 240x240 internal frame buffer for HQV2
volatile bool is_uploading = false;
long hqbvTimeDebt = 0;

uint8_t* upload_psram_buffer = nullptr;
size_t upload_psram_size = 0;
size_t upload_psram_capacity = 0;
volatile bool pending_file_write = false;
String pending_file_name = "";
String loadedGifFile = "";
uint8_t* gif_psram_buffer = nullptr;
size_t gif_psram_size = 0;    // Upload state to pause rendering
bool upscale_2x = false;         // Flag to scale 240x240 to 480x480

uint8_t* upload_buffer = nullptr;
size_t upload_ptr = 0;
File uploadFile;
uint8_t* hqbv_psram_buffer = nullptr;
size_t hqbv_psram_size = 0;
size_t hqbv_psram_offset = 0;

unsigned long lastActivityTime = 0;
const unsigned long WIFI_TIMEOUT_MS = 3 * 60 * 1000;
bool isWifiAwake = true;
#define WAKE_BUTTON_PIN 0

#include <Wire.h>

#define GFX_BL 6 

// Waveshare ESP32-S3-LCD-2.8C exact init sequence (from official demo Display_ST7701.cpp)
static const uint8_t waveshare_28c_init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF,
    WRITE_DATA_8, 0x77,
    WRITE_DATA_8, 0x01,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x13,

    WRITE_COMMAND_8, 0xEF,
    WRITE_DATA_8, 0x08,

    WRITE_COMMAND_8, 0xFF,
    WRITE_DATA_8, 0x77,
    WRITE_DATA_8, 0x01,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x10,

    WRITE_COMMAND_8, 0xC0,
    WRITE_DATA_8, 0x3B,
    WRITE_DATA_8, 0x00,

    WRITE_COMMAND_8, 0xC1,
    WRITE_DATA_8, 0x10,
    WRITE_DATA_8, 0x0C,

    WRITE_COMMAND_8, 0xC2,
    WRITE_DATA_8, 0x07,
    WRITE_DATA_8, 0x0A,

    WRITE_COMMAND_8, 0xC7,
    WRITE_DATA_8, 0x00,

    WRITE_COMMAND_8, 0xCC,
    WRITE_DATA_8, 0x10,

    WRITE_COMMAND_8, 0xCD,
    WRITE_DATA_8, 0x08,

    WRITE_COMMAND_8, 0xB0,
    WRITE_DATA_8, 0x05,
    WRITE_DATA_8, 0x12,
    WRITE_DATA_8, 0x98,
    WRITE_DATA_8, 0x0E,
    WRITE_DATA_8, 0x0F,
    WRITE_DATA_8, 0x07,
    WRITE_DATA_8, 0x07,
    WRITE_DATA_8, 0x09,
    WRITE_DATA_8, 0x09,
    WRITE_DATA_8, 0x23,
    WRITE_DATA_8, 0x05,
    WRITE_DATA_8, 0x52,
    WRITE_DATA_8, 0x0F,
    WRITE_DATA_8, 0x67,
    WRITE_DATA_8, 0x2C,
    WRITE_DATA_8, 0x11,

    WRITE_COMMAND_8, 0xB1,
    WRITE_DATA_8, 0x0B,
    WRITE_DATA_8, 0x11,
    WRITE_DATA_8, 0x97,
    WRITE_DATA_8, 0x0C,
    WRITE_DATA_8, 0x12,
    WRITE_DATA_8, 0x06,
    WRITE_DATA_8, 0x06,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0x22,
    WRITE_DATA_8, 0x03,
    WRITE_DATA_8, 0x51,
    WRITE_DATA_8, 0x11,
    WRITE_DATA_8, 0x66,
    WRITE_DATA_8, 0x2B,
    WRITE_DATA_8, 0x0F,

    WRITE_COMMAND_8, 0xFF,
    WRITE_DATA_8, 0x77,
    WRITE_DATA_8, 0x01,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x11,

    WRITE_COMMAND_8, 0xB0,
    WRITE_DATA_8, 0x5D,

    WRITE_COMMAND_8, 0xB1,
    WRITE_DATA_8, 0x3E,

    WRITE_COMMAND_8, 0xB2,
    WRITE_DATA_8, 0x81,

    WRITE_COMMAND_8, 0xB3,
    WRITE_DATA_8, 0x80,

    WRITE_COMMAND_8, 0xB5,
    WRITE_DATA_8, 0x4E,

    WRITE_COMMAND_8, 0xB7,
    WRITE_DATA_8, 0x85,

    WRITE_COMMAND_8, 0xB8,
    WRITE_DATA_8, 0x20,

    WRITE_COMMAND_8, 0xC1,
    WRITE_DATA_8, 0x78,

    WRITE_COMMAND_8, 0xC2,
    WRITE_DATA_8, 0x78,

    WRITE_COMMAND_8, 0xD0,
    WRITE_DATA_8, 0x88,

    WRITE_COMMAND_8, 0xE0,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x02,

    WRITE_COMMAND_8, 0xE1,
    WRITE_DATA_8, 0x06,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0x05,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0x07,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x33,
    WRITE_DATA_8, 0x33,

    WRITE_COMMAND_8, 0xE2,
    WRITE_DATA_8, 0x11,
    WRITE_DATA_8, 0x11,
    WRITE_DATA_8, 0x33,
    WRITE_DATA_8, 0x33,
    WRITE_DATA_8, 0xF4,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0xF4,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,

    WRITE_COMMAND_8, 0xE3,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x11,
    WRITE_DATA_8, 0x11,

    WRITE_COMMAND_8, 0xE4,
    WRITE_DATA_8, 0x44,
    WRITE_DATA_8, 0x44,

    WRITE_COMMAND_8, 0xE5,
    WRITE_DATA_8, 0x0D,
    WRITE_DATA_8, 0xF5,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,
    WRITE_DATA_8, 0x0F,
    WRITE_DATA_8, 0xF7,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,
    WRITE_DATA_8, 0x09,
    WRITE_DATA_8, 0xF1,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,
    WRITE_DATA_8, 0x0B,
    WRITE_DATA_8, 0xF3,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,

    WRITE_COMMAND_8, 0xE6,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x11,
    WRITE_DATA_8, 0x11,

    WRITE_COMMAND_8, 0xE7,
    WRITE_DATA_8, 0x44,
    WRITE_DATA_8, 0x44,

    WRITE_COMMAND_8, 0xE8,
    WRITE_DATA_8, 0x0C,
    WRITE_DATA_8, 0xF4,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,
    WRITE_DATA_8, 0x0E,
    WRITE_DATA_8, 0xF6,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0xF0,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,
    WRITE_DATA_8, 0x0A,
    WRITE_DATA_8, 0xF2,
    WRITE_DATA_8, 0x30,
    WRITE_DATA_8, 0xF0,

    WRITE_COMMAND_8, 0xE9,
    WRITE_DATA_8, 0x36,
    WRITE_DATA_8, 0x01,

    WRITE_COMMAND_8, 0xEB,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x01,
    WRITE_DATA_8, 0xE4,
    WRITE_DATA_8, 0xE4,
    WRITE_DATA_8, 0x44,
    WRITE_DATA_8, 0x88,
    WRITE_DATA_8, 0x40,

    WRITE_COMMAND_8, 0xED,
    WRITE_DATA_8, 0xFF,
    WRITE_DATA_8, 0x10,
    WRITE_DATA_8, 0xAF,
    WRITE_DATA_8, 0x76,
    WRITE_DATA_8, 0x54,
    WRITE_DATA_8, 0x2B,
    WRITE_DATA_8, 0xCF,
    WRITE_DATA_8, 0xFF,
    WRITE_DATA_8, 0xFF,
    WRITE_DATA_8, 0xFC,
    WRITE_DATA_8, 0xB2,
    WRITE_DATA_8, 0x45,
    WRITE_DATA_8, 0x67,
    WRITE_DATA_8, 0xFA,
    WRITE_DATA_8, 0x01,
    WRITE_DATA_8, 0xFF,

    WRITE_COMMAND_8, 0xEF,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0x45,
    WRITE_DATA_8, 0x3F,
    WRITE_DATA_8, 0x54,

    WRITE_COMMAND_8, 0xFF,
    WRITE_DATA_8, 0x77,
    WRITE_DATA_8, 0x01,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x00,

    WRITE_COMMAND_8, 0x11,   // Sleep Out
    END_WRITE,

    DELAY, 120,               // 120ms delay after sleep out

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x3A,
    WRITE_DATA_8, 0x66,       // 18-bit/pixel (RGB666)

    WRITE_COMMAND_8, 0x36,
    WRITE_DATA_8, 0x00,

    WRITE_COMMAND_8, 0x35,
    WRITE_DATA_8, 0x00,

    WRITE_COMMAND_8, 0x29,   // Display ON
    END_WRITE,
};

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    GFX_NOT_DEFINED /* DC */, GFX_NOT_DEFINED /* CS */,
    2 /* SCK */, 1 /* MOSI */, GFX_NOT_DEFINED /* MISO */,
    SPI2_HOST /* spi_num */, false /* is_shared_interface */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    40 /* DE */, 39 /* VSYNC */, 38 /* HSYNC */, 41 /* PCLK */,
    46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
    14 /* G0 */, 13 /* G1 */, 12 /* G2 */, 11 /* G3 */, 10 /* G4 */, 9 /* G5 */,
    5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 8 /* vsync_front_porch */, 2 /* vsync_pulse_width */, 18 /* vsync_back_porch */,
    0 /* pclk_active_neg */, 10000000 /* prefer_speed */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, waveshare_28c_init_operations, sizeof(waveshare_28c_init_operations));


void GIFDraw(GIFDRAW *pDraw) {
    uint8_t *s;
    uint16_t *usPalette;
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (upscale_2x && iWidth > 240) iWidth = 240;
    else if (!upscale_2x && iWidth > 480) iWidth = 480;
    
    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; 
    
    int16_t x_offset = (480 - (upscale_2x ? gif.getCanvasWidth() * 2 : gif.getCanvasWidth())) / 2;
    int16_t y_offset = (480 - (upscale_2x ? gif.getCanvasHeight() * 2 : gif.getCanvasHeight())) / 2;
    if (x_offset < 0) x_offset = 0;
    if (y_offset < 0) y_offset = 0;

    uint16_t* target = back_buffer;
    if (!target) return;

    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) { 
        for (x=0; x<iWidth; x++) {
            if (s[x] == pDraw->ucTransparent) {
                s[x] = pDraw->ucBackground;
            }
        }
        pDraw->ucHasTransparency = 0;
    }
    
    s = pDraw->pPixels;
    uint8_t ucTransparent = pDraw->ucTransparent;
    static uint16_t line_buf[480];
    
    if (upscale_2x) {
        int py1 = y_offset + y * 2;
        int py2 = py1 + 1;
        if (py1 < 0 || py1 >= 480) return;
        
        int start_x = x_offset + pDraw->iX * 2;
        if (start_x >= 480) return;
        
        int draw_w = iWidth * 2;
        if (start_x + draw_w > 480) draw_w = 480 - start_x;
        
        memcpy(line_buf, &target[py1 * 480 + start_x], draw_w * 2);
        bool modified = false;

        for (x = 0; x < iWidth; x++) {
            uint8_t c = *s++;
            if (!pDraw->ucHasTransparency || c != ucTransparent) {
                if (x * 2 + 1 < draw_w) {
                    uint16_t color = usPalette[c];
                    line_buf[x * 2] = color;
                    line_buf[x * 2 + 1] = color;
                    modified = true;
                }
            }
        }
        
        if (modified || !pDraw->ucHasTransparency) {
            memcpy(&target[py1 * 480 + start_x], line_buf, draw_w * 2);
            if (py2 < 480) {
                memcpy(&target[py2 * 480 + start_x], line_buf, draw_w * 2);
            }
        }
    } else {
        int py = y_offset + y;
        if (py < 0 || py >= 480) return;
        
        int start_x = x_offset + pDraw->iX;
        if (start_x >= 480) return;
        
        int draw_w = iWidth;
        if (start_x + draw_w > 480) draw_w = 480 - start_x;
        
        if (!pDraw->ucHasTransparency) {
            for (x = 0; x < draw_w; x++) {
                line_buf[x] = usPalette[s[x]];
            }
            memcpy(&target[py * 480 + start_x], line_buf, draw_w * 2);
        } else {
            memcpy(line_buf, &target[py * 480 + start_x], draw_w * 2);
            bool modified = false;
            for (x = 0; x < draw_w; x++) {
                uint8_t c = s[x];
                if (c != ucTransparent) {
                    line_buf[x] = usPalette[c];
                    modified = true;
                }
            }
            if (modified) {
                memcpy(&target[py * 480 + start_x], line_buf, draw_w * 2);
            }
        }
    }
}

void flush_buffer() {
    if (is_uploading) return;
    if (back_buffer) {
        uint16_t* fb = gfx->getFramebuffer();
        if (fb) {
            memcpy(fb, back_buffer, 480 * 480 * 2);
        } else {
            gfx->draw16bitRGBBitmap(0, 0, back_buffer, 480, 480);
        }
    }
}

// Fast Bilinear Upscaling for RGB565
inline uint16_t blend565(uint16_t c1, uint16_t c2) {
    return ((((c1 & 0xF81F) + (c2 & 0xF81F)) >> 1) & 0xF81F) | ((((c1 & 0x07E0) + (c2 & 0x07E0)) >> 1) & 0x07E0);
}
inline uint16_t blend565_4(uint16_t c1, uint16_t c2, uint16_t c3, uint16_t c4) {
    return ((((c1 & 0xF81F) + (c2 & 0xF81F) + (c3 & 0xF81F) + (c4 & 0xF81F)) >> 2) & 0xF81F) | ((((c1 & 0x07E0) + (c2 & 0x07E0) + (c3 & 0x07E0) + (c4 & 0x07E0)) >> 2) & 0x07E0);
}

// Flag: when true, JPEGDraw writes directly to DMA framebuffer (skips back_buffer + memcpy)
bool jpeg_direct_fb = false;

// Коллбек для отрисовки JPEG
int JPEGDraw(JPEGDRAW *pDraw) {
    int16_t x = pDraw->x;
    int16_t y = pDraw->y;
    uint16_t w = pDraw->iWidth;
    uint16_t h = pDraw->iHeight;
    uint16_t* bitmap = pDraw->pPixels;

    uint16_t* target;
    if (jpeg_direct_fb) {
        target = gfx->getFramebuffer();
    } else {
        target = back_buffer;
    }
    if (!target) return 0;
    
    int scale = 1;
    if (upscale_2x) {
        int imgWidth = jpeg.getWidth();
        if (imgWidth > 0) {
            scale = 480 / imgWidth;
            if (scale < 1) scale = 1;
        }
    }
    
    if (scale > 1) {
        int draw_x = x * scale;
        int draw_y = y * scale;
        
        for (int j = 0; j < h; j++) {
            uint16_t* src = &bitmap[j * w];
            for (int sy = 0; sy < scale; sy++) {
                int py = draw_y + j * scale + sy;
                if (py < 0 || py >= 480) continue;
                
                uint16_t* dest = &target[py * 480 + draw_x];
                int max_i = w;
                if (draw_x + max_i * scale > 480) {
                    max_i = (480 - draw_x) / scale;
                }
                
                for (int i = 0; i < max_i; i++) {
                    uint16_t c = src[i];
                    for (int sx = 0; sx < scale; sx++) {
                        *dest++ = c;
                    }
                }
            }
        }
        return 1;
    }

    if (y >= 480 || x >= 480) return 0;
    
    for (int j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= 480) continue;
        
        int px = x;
        int copy_w = w;
        int src_offset = 0;
        
        if (px < 0) {
            copy_w += px;
            src_offset = -px;
            px = 0;
        }
        if (px + copy_w > 480) {
            copy_w = 480 - px;
        }
        
        if (copy_w > 0) {
            memcpy(&target[py * 480 + px], &bitmap[j * w + src_offset], copy_w * 2);
        }
    }
    return 1;
}

void drawCurrentImage() {
    if (back_buffer) {
        memset(back_buffer, 0, 480 * 480 * 2);
    }
    // Always clear the active screen to black to erase Wi-Fi text
    gfx->fillScreen(0x0000);


    if (currentFile.endsWith(".hqv2") || currentFile.endsWith(".HQV2") || 
        currentFile.endsWith(".hqbv") || currentFile.endsWith(".HQBV")) {
        
        bool is_hqv2 = currentFile.endsWith(".hqv2") || currentFile.endsWith(".HQV2");
        Serial.printf("Detected %s. Loading to PSRAM...\n", is_hqv2 ? "HQV2" : "HQBV");
        isHqVideoPlaying = true;
        isGifPlaying = false;
        upscale_2x = true;

        if (is_hqv2 && !hqv2_frame_buffer) {
            hqv2_frame_buffer = (uint16_t*)heap_caps_malloc(160 * 160 * 2, MALLOC_CAP_INTERNAL);
            if (!hqv2_frame_buffer) hqv2_frame_buffer = (uint16_t*)heap_caps_malloc(160 * 160 * 2, MALLOC_CAP_SPIRAM);
        }

        if (hqbv_psram_buffer) {
            free(hqbv_psram_buffer);
            hqbv_psram_buffer = nullptr;
        }
        
        if (hqVideoFile) hqVideoFile.close();
        hqVideoFile = LittleFS.open(currentFile, "r");
        if (hqVideoFile) {
            hqbv_psram_size = hqVideoFile.size();
            hqbv_psram_buffer = (uint8_t*)heap_caps_malloc(hqbv_psram_size, MALLOC_CAP_SPIRAM);
            if (hqbv_psram_buffer) {
                size_t read_ptr = 0;
                while (read_ptr < hqbv_psram_size) {
                    size_t chunk = (hqbv_psram_size - read_ptr > 65536) ? 65536 : (hqbv_psram_size - read_ptr);
                    hqVideoFile.read(hqbv_psram_buffer + read_ptr, chunk);
                    read_ptr += chunk;
                    delay(1);
                }
                hqVideoFile.close();
            }
        }
        hqbv_psram_offset = 0;
        hqbvTimeDebt = 0;

        // === AUTO-DETECT frame size from first frame JPEG header ===
        // This ensures correct rendering regardless of what video was previously uploaded
        if (hqbv_psram_buffer && hqbv_psram_size > 9) {
            uint32_t firstFrameSize = ((uint32_t)hqbv_psram_buffer[5] << 24) |
                                       ((uint32_t)hqbv_psram_buffer[6] << 16) |
                                       ((uint32_t)hqbv_psram_buffer[7] << 8)  |
                                        (uint32_t)hqbv_psram_buffer[8];
            if (firstFrameSize > 0 && 9 + firstFrameSize <= hqbv_psram_size) {
                if (jpeg.openRAM(hqbv_psram_buffer + 9, firstFrameSize, JPEGDraw)) {
                    int jw = jpeg.getWidth();
                    int jh = jpeg.getHeight();
                    jpeg.close();
                    // Use upscale only for small (240x240) frames
                    upscale_2x = (jw <= 280 && jh <= 280);
                    Serial.printf("HQBV frame: %dx%d -> upscale_2x=%d\n", jw, jh, (int)upscale_2x);
                }
            }
        } else if (hqVideoFile) {
            // Auto-detect for fallback streaming mode
            hqVideoFile.seek(0);
            char header[5];
            if (hqVideoFile.readBytes(header, 5) == 5 && strncmp(header, "HQBV", 4) == 0) {
                uint8_t sizeBytes[4];
                if (hqVideoFile.readBytes((char*)sizeBytes, 4) == 4) {
                    uint32_t firstFrameSize = (sizeBytes[0] << 24) | (sizeBytes[1] << 16) | (sizeBytes[2] << 8) | sizeBytes[3];
                    if (firstFrameSize > 0 && firstFrameSize < 200000) {
                        uint8_t* peekBuf = (uint8_t*)malloc(firstFrameSize);
                        if (peekBuf) {
                            if (hqVideoFile.read(peekBuf, firstFrameSize) == firstFrameSize) {
                                if (jpeg.openRAM(peekBuf, firstFrameSize, JPEGDraw)) {
                                    int jw = jpeg.getWidth();
                                    int jh = jpeg.getHeight();
                                    jpeg.close();
                                    upscale_2x = (jw <= 280 && jh <= 280);
                                    Serial.printf("HQBV file frame: %dx%d -> upscale_2x=%d\n", jw, jh, (int)upscale_2x);
                                }
                            }
                            free(peekBuf);
                        }
                    }
                }
            }
            hqVideoFile.seek(0);
        }
    } else if (currentFile.endsWith(".gif") || currentFile.endsWith(".GIF")) {
        Serial.println("Detected GIF. Starting decoding loop...");
        isGifPlaying = true;
        isHqVideoPlaying = false;
        upscale_2x = false;
    } else {
        Serial.println("Detected static image. Drawing...");
        isGifPlaying = false;
        upscale_2x = false;
        
        File f = LittleFS.open(currentFile, "r");
        if (f) {
            size_t size = f.size();
            uint8_t* buf = (uint8_t*)malloc(size);
            if (buf) {
                f.read(buf, size);
                if (jpeg.openRAM(buf, size, JPEGDraw)) {
                    jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                    int x = (480 - jpeg.getWidth()) / 2;
                    int y = (480 - jpeg.getHeight()) / 2;
                    jpeg.decode(x > 0 ? x : 0, y > 0 ? y : 0, 0);
                    jpeg.close();
                }
                free(buf);
            }
            f.close();
        }
        
        if (back_buffer) {
            flush_buffer();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nStarting KIT VISUAL Firmware...");
    Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free  PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Total heap:  %d bytes\n", ESP.getHeapSize());
    Serial.printf("Free  heap:  %d bytes\n", ESP.getFreeHeap());



    // Инициализация Экрана
    #ifdef GFX_BL
        pinMode(GFX_BL, OUTPUT);
        digitalWrite(GFX_BL, HIGH);
    #endif
    // Инициализация I2C для расширителя портов TCA9554PWR
    Wire.begin(15, 7);
    
    // Настраиваем все пины как выходы
    Wire.beginTransmission(0x20);
    Wire.write(0x03);
    Wire.write(0x00);
    Wire.endTransmission();

    // Сброс дисплея: RST (EXIO1) = Low, CS = High, Buzzer = Low
    Wire.beginTransmission(0x20);
    Wire.write(0x01);
    Wire.write(0x0C); // RST=0, CS=1, SD_CS=1
    Wire.endTransmission();
    delay(50);

    // RST = High, CS = High, Buzzer = Low
    Wire.beginTransmission(0x20);
    Wire.write(0x01);
    Wire.write(0x0D); // RST=1, CS=1, SD_CS=1
    Wire.endTransmission();
    delay(120);

    // Заранее настраиваем пины SPI, чтобы не было глитчей при вызове begin()
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW); // SCK idle low
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW); // MOSI idle low

    // Перед инициализацией дисплея притягиваем CS (EXIO3) = Low
    Wire.beginTransmission(0x20);
    Wire.write(0x01);
    Wire.write(0x09); // RST=1, CS=0, SD_CS=1
    Wire.endTransmission();
    delay(100);

    Serial.println("Starting gfx->begin()...");
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    } else {
        Serial.println("gfx->begin() ok!");
    }

    // Allocate back buffer in PSRAM for double buffering to prevent tearing
    back_buffer = (uint16_t*)heap_caps_malloc(480 * 480 * 2, MALLOC_CAP_SPIRAM);
    if (!back_buffer) {
        Serial.println("FATAL ERROR: Failed to allocate PSRAM back buffer!");
    } else {
        Serial.println("Successfully allocated 460KB PSRAM back buffer.");
    }

    // После инициализации отпускаем CS = High
    Wire.beginTransmission(0x20);
    Wire.write(0x01);
    Wire.write(0x0D); // RST=1, CS=1, SD_CS=1
    Wire.endTransmission();

    gfx->fillScreen(0x0000); // Чёрный
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(3);
    gfx->setCursor(50, 220);
    gfx->println("Wi-Fi: KIT VISUAL");

    // 1. Инициализация файловой системы
    if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    // Кнопка пробуждения
    pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);

    // Загрузка последнего файла
    if (LittleFS.exists("/last_image.txt")) {
        File f = LittleFS.open("/last_image.txt", "r");
        currentFile = f.readString();
        currentFile.trim();
        f.close();
        if (currentFile != "" && LittleFS.exists(currentFile)) {
            Serial.println("Loaded last image: " + currentFile);
            drawCurrentImage();
        }
    }

    // 2. Инициализация Wi-Fi AP
    WiFi.softAP(ssid, password);
    WiFi.setSleep(false); // Disable WiFi power save to prevent dropouts
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // 3. Настройка Web-сервера и Captive Portal
    dnsServer.start(53, "*", IP);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        lastActivityTime = millis();
        request->send(200, "text/plain", "KIT VISUAL Ready");
    });
    
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://192.168.4.1/");
    });
    
    server.onNotFound([](AsyncWebServerRequest *request){
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->redirect("http://192.168.4.1/");
        }
    });

    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        lastActivityTime = millis();
        request->send(200, "text/plain", "File Uploaded Successfully");
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            Serial.printf("UploadStart: %s\n", filename.c_str());
            is_uploading = true;
            digitalWrite(GFX_BL, LOW);
            loading_frame = 0;
            currentFile = "/" + filename;
            pending_file_write = false;

            isGifPlaying = false;
            isHqVideoPlaying = false;
            if (gifFile) gifFile.close();
            if (hqVideoFile) hqVideoFile.close();

            // Free all PSRAM buffers to maximize space
            if (hqbv_psram_buffer)   { free(hqbv_psram_buffer);   hqbv_psram_buffer   = nullptr; }
            if (gif_psram_buffer)    { free(gif_psram_buffer);    gif_psram_buffer    = nullptr; }
            if (upload_psram_buffer) { free(upload_psram_buffer); upload_psram_buffer = nullptr; }
            if (uploadFile) uploadFile.close();

            LittleFS.remove("/badge_image.gif");
            LittleFS.remove("/badge_image.hqv2");
            LittleFS.remove("/badge_image.hqbv");
            LittleFS.remove("/badge_image.jpg");

            uploadFile = LittleFS.open(currentFile, "w");
        }

        if (uploadFile) {
            uploadFile.write(data, len);
        }

        if (final) {
            Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
            if (uploadFile) { uploadFile.flush(); uploadFile.close(); }
            File f = LittleFS.open("/last_image.txt", "w");
            if (f) { f.print(currentFile); f.close(); }
            
            pending_file_write = true;
            is_uploading = false;
            digitalWrite(GFX_BL, HIGH);
        }
    });

    server.begin();
    gif.begin(LITTLE_ENDIAN_PIXELS); 
    lastActivityTime = millis();
}

void drawHQV2Frame() {
    if (!hqv2_frame_buffer) return;
    
    // Chunked nearest-neighbor upscaler (160x160 -> 480x480)
    // Scale factor is 3. We process 1 source line at a time, outputting 3 identical lines.
    alignas(4) static uint16_t chunk_buf[480 * 3]; 
    
    for (int y = 0; y < 160; y++) {
        uint16_t* src_row = &hqv2_frame_buffer[y * 160];
        
        for (int x = 0; x < 160; x++) {
            uint16_t c = src_row[x];
            int dx = x * 3;
            chunk_buf[dx] = c;
            chunk_buf[dx + 1] = c;
            chunk_buf[dx + 2] = c;
        }
        
        // Copy the first scaled row into the next two rows to complete 3x vertical scaling
        memcpy(&chunk_buf[480], &chunk_buf[0], 480 * 2);
        memcpy(&chunk_buf[960], &chunk_buf[0], 480 * 2);
        
        gfx->draw16bitRGBBitmap(0, y * 3, chunk_buf, 480, 3);
    }
}

void loop() {
    dnsServer.processNextRequest();
    
    if (pending_file_write) {
        pending_file_write = false;
        drawCurrentImage();
    }


    if (is_uploading) {
        yield(); // Don't block! Let WiFi stack handle health-check requests
        return;
    }

    if (isHqVideoPlaying && hqbv_psram_buffer) {
        static unsigned long hqbvStartTime = 0;
        static int currentFrameIndex = 0;
        static bool is_hqv2_format = false;

        if (hqbv_psram_offset == 0) {
            if (hqbv_psram_size < 5) {
                isHqVideoPlaying = false; return;
            }
            if (strncmp((char*)hqbv_psram_buffer, "HQV2", 4) == 0) {
                is_hqv2_format = true;
                if (hqbv_psram_size >= 6) {
                    hqFps = hqbv_psram_buffer[4];
                    if (hqFps <= 0 || hqFps > 60) hqFps = 15;
                    hqbv_psram_offset = 6;
                } else {
                    hqFps = 15;
                    hqbv_psram_offset = 4;
                }
                if (hqv2_frame_buffer) memset(hqv2_frame_buffer, 0, 160 * 160 * 2);
            } else if (strncmp((char*)hqbv_psram_buffer, "HQBV", 4) == 0) {
                is_hqv2_format = false;
                hqFps = hqbv_psram_buffer[4];
                hqbv_psram_offset = 5;
            } else {
                Serial.println("Invalid format!");
                isHqVideoPlaying = false;
                return;
            }
            if (hqFps <= 0 || hqFps > 60) hqFps = 15;
            hqbvStartTime = millis();
            currentFrameIndex = 0;
        }

        if (hqbv_psram_offset + 4 <= hqbv_psram_size) {
            uint32_t frameSize = ((uint32_t)hqbv_psram_buffer[hqbv_psram_offset] << 24) | 
                                 ((uint32_t)hqbv_psram_buffer[hqbv_psram_offset+1] << 16) | 
                                 ((uint32_t)hqbv_psram_buffer[hqbv_psram_offset+2] << 8) | 
                                  (uint32_t)hqbv_psram_buffer[hqbv_psram_offset+3];
            hqbv_psram_offset += 4;
            
            if (frameSize > 0 && hqbv_psram_offset + frameSize <= hqbv_psram_size) {
                unsigned long now = millis();
                int expectedFrameIndex = ((now - hqbvStartTime) * hqFps) / 1000;

                bool should_draw = true;
                if (currentFrameIndex < expectedFrameIndex) {
                    // We are falling behind!
                    if (is_hqv2_format) {
                        // Delta compression MUST be decoded to maintain state. Skip drawing only.
                        should_draw = false;
                    } else {
                        // JPEG frames are independent, skip safely.
                        hqbv_psram_offset += frameSize;
                        currentFrameIndex++;
                        return; // Skip decode and let loop() process next frame
                    }
                }

                if (is_hqv2_format) {
                    if (hqv2_frame_buffer) {
                        uint8_t* ptr = hqbv_psram_buffer + hqbv_psram_offset;
                        uint8_t* end = ptr + frameSize;
                        int pixel_index = 0;
                        int ops_processed = 0;
                        
                        while (ptr < end && pixel_index < 25600) {
                            uint8_t op = *ptr++;
                            ops_processed++;
                            if (op < 128) {
                                int count = op + 1;
                                if (pixel_index + count > 25600) count = 25600 - pixel_index;
                                if (ptr + count * 2 > end) count = (end - ptr) / 2;
                                if (count > 0) {
                                    memcpy(&hqv2_frame_buffer[pixel_index], ptr, count * 2);
                                    ptr += count * 2;
                                    pixel_index += count;
                                }
                            } else if (op == 255) {
                                if (ptr + 1 >= end) break;
                                int count = (*ptr << 8) | *(ptr + 1);
                                ptr += 2;
                                if (pixel_index + count > 25600) count = 25600 - pixel_index;
                                pixel_index += count;
                            } else {
                                int count = (op - 128) + 1;
                                if (pixel_index + count > 25600) count = 25600 - pixel_index;
                                pixel_index += count;
                            }
                        }
                        if (currentFrameIndex % 30 == 0) {
                            Serial.printf("Decoded HQV2 frame %d, size: %u, ops: %d, should_draw: %d\n", currentFrameIndex, frameSize, ops_processed, (int)should_draw);
                        }
                        if (should_draw) {
                            drawHQV2Frame();
                        }
                    } else {
                        Serial.println("ERROR: hqv2_frame_buffer is NULL!");
                    }
                } else {
                    if (jpeg.openRAM(hqbv_psram_buffer + hqbv_psram_offset, frameSize, JPEGDraw)) {
                        jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                        jpeg.decode(0, 0, 0);
                        jpeg.close();
                    }
                    if (should_draw) {
                        flush_buffer();
                    }
                }
                
                hqbv_psram_offset += frameSize;
                currentFrameIndex++;
                
                unsigned long nextFrameTime = hqbvStartTime + (currentFrameIndex * 1000) / hqFps;
                now = millis();
                if (nextFrameTime > now) {
                    delay(nextFrameTime - now);
                }
            } else {
                hqbv_psram_offset = 0;
            }
            delay(1); // Feed the watchdog!
        } else {
            hqbv_psram_offset = 0;
        }
        delay(1);

    } else if (isHqVideoPlaying && hqVideoFile) {
        static unsigned long hqbvFileStartTime = 0;
        static int currentFileFrameIndex = 0;
        static bool file_is_hqv2 = false;
        
        if (hqVideoFile.position() == 0) {
            char header[4];
            if (hqVideoFile.readBytes(header, 4) != 4) {
                isHqVideoPlaying = false; return;
            }
            if (strncmp(header, "HQV2", 4) == 0) {
                file_is_hqv2 = true;
                hqFps = hqVideoFile.read();
                hqVideoFile.read(); // reserved
                if (hqv2_frame_buffer) memset(hqv2_frame_buffer, 0, 160 * 160 * 2);
            } else if (strncmp(header, "HQBV", 4) == 0) {
                file_is_hqv2 = false;
                hqFps = hqVideoFile.read();
            } else {
                Serial.println("Invalid format!");
                isHqVideoPlaying = false;
                return;
            }
            if (hqFps <= 0 || hqFps > 60) hqFps = 15;
            hqbvFileStartTime = millis();
            currentFileFrameIndex = 0;
        }

        uint8_t sizeBytes[4];
        if (hqVideoFile.readBytes((char*)sizeBytes, 4) == 4) {
            uint32_t frameSize = (sizeBytes[0] << 24) | (sizeBytes[1] << 16) | (sizeBytes[2] << 8) | sizeBytes[3];
            if (frameSize > 0 && frameSize < 200000) { 
                unsigned long now = millis();
                int expectedFrameIndex = ((now - hqbvFileStartTime) * hqFps) / 1000;
                
                bool should_draw = true;
                if (currentFileFrameIndex < expectedFrameIndex) {
                    if (file_is_hqv2) {
                        should_draw = false;
                    } else {
                        hqVideoFile.seek(hqVideoFile.position() + frameSize);
                        currentFileFrameIndex++;
                        return;
                    }
                }

                uint8_t* jpegBuf = (uint8_t*)malloc(frameSize);
                if (jpegBuf) {
                    if (hqVideoFile.read(jpegBuf, frameSize) == frameSize) {
                        if (file_is_hqv2) {
                            if (hqv2_frame_buffer) {
                                uint8_t* ptr = jpegBuf;
                                uint8_t* end = ptr + frameSize;
                                int pixel_index = 0;
                                
                                while (ptr < end && pixel_index < 25600) {
                                    uint8_t op = *ptr++;
                                    if (op < 128) {
                                        int count = op + 1;
                                        if (pixel_index + count > 25600) count = 25600 - pixel_index;
                                        if (ptr + count * 2 > end) count = (end - ptr) / 2;
                                        if (count > 0) {
                                            memcpy(&hqv2_frame_buffer[pixel_index], ptr, count * 2);
                                            ptr += count * 2;
                                            pixel_index += count;
                                        }
                                    } else if (op == 255) {
                                        if (ptr + 1 >= end) break;
                                        int count = (*ptr << 8) | *(ptr + 1);
                                        ptr += 2;
                                        if (pixel_index + count > 25600) count = 25600 - pixel_index;
                                        pixel_index += count;
                                    } else {
                                        int count = (op - 128) + 1;
                                        if (pixel_index + count > 25600) count = 25600 - pixel_index;
                                        pixel_index += count;
                                    }
                                }
                                if (should_draw) {
                                    drawHQV2Frame();
                                }
                            } else {
                                Serial.println("ERROR: hqv2_frame_buffer is NULL!");
                            }
                        } else {
                            if (jpeg.openRAM(jpegBuf, frameSize, JPEGDraw)) {
                                jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                                jpeg.decode(0, 0, 0);
                                jpeg.close();
                            }
                            if (should_draw) {
                                flush_buffer();
                            }
                        }
                        
                        
                        currentFileFrameIndex++;
                        unsigned long nextFrameTime = hqbvFileStartTime + (currentFileFrameIndex * 1000) / hqFps;
                        now = millis();
                        if (nextFrameTime > now) {
                            delay(nextFrameTime - now);
                        }
                    } else {
                        currentFileFrameIndex++;
                    }
                    free(jpegBuf);
                } else {
                    Serial.println("Memory alloc failed for HQBV frame!");
                    hqVideoFile.seek(hqVideoFile.position() + frameSize);
                    currentFileFrameIndex++;
                }
            } else {
                hqVideoFile.seek(0);
            }
        } else {
            hqVideoFile.seek(0); // End of file, loop
        }
        yield();
        
    } else if (isGifPlaying && currentFile != "") {
        if (loadedGifFile != currentFile) {
            if (gif_psram_buffer) {
                free(gif_psram_buffer);
                gif_psram_buffer = nullptr;
            }
            File gifFile = LittleFS.open(currentFile, "r");
            if (gifFile) {
                gif_psram_size = gifFile.size();
                gif_psram_buffer = (uint8_t*)heap_caps_malloc(gif_psram_size, MALLOC_CAP_SPIRAM);
                if (gif_psram_buffer) {
                    gifFile.read(gif_psram_buffer, gif_psram_size);
                }
                gifFile.close();
                loadedGifFile = currentFile;
            }
        }
        
        if (gif_psram_buffer && gif_psram_size > 0) {
            if (gif.open(gif_psram_buffer, gif_psram_size, GIFDraw)) {
                upscale_2x = (gif.getCanvasWidth() <= 240 && gif.getCanvasHeight() <= 240);
                
                int delayMs = 0;
                long timeDebt = 0;
                while (isGifPlaying && !is_uploading) {
                    unsigned long startDecode = millis();
                    int result = gif.playFrame(false, &delayMs);
                    if (!result) break;
                    
                    if (timeDebt > 0) {
                        timeDebt -= delayMs;
                    } else {
                        flush_buffer();
                        unsigned long totalTime = millis() - startDecode;
                        long frameBalance = delayMs - (long)totalTime;
                        
                        if (frameBalance > 0) {
                            delay(frameBalance);
                            timeDebt = 0;
                        } else {
                            timeDebt -= frameBalance; 
                        }
                    }
                    yield();
                }
                gif.close();
            } else {
                isGifPlaying = false;
            }
        } else {
            isGifPlaying = false;
        }
    } else {
        delay(10);
    }
}
