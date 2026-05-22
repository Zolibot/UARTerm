#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "UartScanner.h"

class DisplayManager {
public:
    enum DispMode : uint8_t {
        DISP_UART,
        DISP_NETWORK
    };

    void init();
    void addChar(char c);
    void render(const UartScanner& scanner);
    void showMessage(const char* msg);
    void setHexMode(bool enabled) { _hexMode = enabled; }
    bool getHexMode() const { return _hexMode; }
    void toggleHexMode() { _hexMode = !_hexMode; }

    void setDisplayMode(DispMode mode) { _dispMode = mode; }
    void setNetworkInfo(const char* mode, const char* ip, uint16_t port, int clients);
    void setSDLogging(bool on) { _sdLogging = on; }
    void setSDLogFile(const char* name) { strncpy(_logFileName, name, sizeof(_logFileName) - 1); }
    void setBottomInfo(const char* str) { strncpy(_bottomText, str, sizeof(_bottomText) - 1); }
    void capture(uint16_t* buf) const;

private:
    static constexpr int MAX_LINES = 24;
    static constexpr int LINE_LEN = 42;
    static constexpr int BAR_H = 11;
    static constexpr int STATUS_BAR_Y = 0;
    static constexpr int BOTTOM_BAR_Y = 124;
    static constexpr int DATA_AREA_Y = BAR_H;
    static constexpr int DATA_AREA_H = 113;
    static constexpr int CHAR_W = 6;
    static constexpr int CHAR_H = 8;

    char _lines[MAX_LINES][LINE_LEN];
    int _head = 0;
    int _col = 0;
    bool _dirty = true;
    bool _hexMode = false;
    bool _spriteReady = false;
    int _hexBuf[14];
    int _hexCount = 0;
    LGFX_Sprite* _sprite = nullptr;
    LGFX_Sprite* _barSprite = nullptr;
    LGFX_Sprite* _bottomSprite = nullptr;

    DispMode _dispMode = DISP_UART;
    bool _sdLogging = false;
    char _logFileName[16] = "";
    char _netMode[4] = "";
    char _netIP[16] = "";
    uint16_t _netPort = 0;
    int _netClients = 0;
    char _bottomText[42] = "";

    void clearLines();
    void newline();
    void renderTopBar(const UartScanner& scanner);
    void renderDataArea();
    void renderBottomBar();
};

inline void DisplayManager::setNetworkInfo(const char* mode, const char* ip, uint16_t port, int clients) {
    strncpy(_netMode, mode, sizeof(_netMode) - 1);
    strncpy(_netIP, ip, sizeof(_netIP) - 1);
    _netPort = port;
    _netClients = clients;
}
