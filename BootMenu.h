#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <M5GFX.h>
#include "AppConfig.h"
#include "KnownNetworks.h"

class BootMenu {
public:
    enum Mode : uint8_t {
        MODE_UART_SCANNER = 1,
        MODE_AP_SERVER     = 2,
        MODE_STA_SERVER    = 3,
        MODE_SETTINGS      = 4,
        MODE_HELP          = 5
    };

    void begin(AppSettings& settings);
    void update();
    void render();
    void reset();
    void retryPassword();
    void setDirty() { _dirty = true; }

    void setKnownNetworks(KnownNetworks* kn) { _known = kn; }
    void setScreenshotCallback(void (*cb)()) { _ssCb = cb; }
    void capture(uint16_t* buf) const;
    LGFX_Sprite* getSprite() { return _menuSprite; }

    bool isConfirmed() const { return _confirmed; }
    int  getSelectedMode() const { return _selectedMode; }
    bool isSettingsDirty() const { return _settingsDirty; }
    const AppSettings& getSettings() const { return *_settings; }

private:
    enum State : uint8_t {
        MAIN_MENU,
        SETTINGS,
        WIFI_SCAN,
        WIFI_PASSWORD,
        HELP_SCREEN,
        CONFIRMED
    };

    enum SettingField : uint8_t {
        SF_BAUD, SF_RX, SF_TX,
        SF_AP_SSID, SF_AP_PWD,
        SF_STA_SSID, SF_STA_PWD,
        SF_PORT, SF_SD_LOG,
        SF_SAVE,
        SF_COUNT
    };

    enum FieldType : uint8_t {
        FT_ENUM, FT_TEXT, FT_PASSWORD, FT_NUM, FT_BOOL, FT_ACTION
    };

    static const unsigned long BAUD_RATES[];
    static const int BAUD_COUNT;

    void (*_ssCb)() = nullptr;
    KnownNetworks* _known = nullptr;
    AppSettings* _settings = nullptr;
    AppSettings _editSettings;

    State _state = MAIN_MENU;
    int _selectedMode = MODE_UART_SCANNER;
    int _menuIdx = 0;
    bool _confirmed = false;

    // Settings navigation
    int _sfIdx = 0;
    int _sfScroll = 0;
    static constexpr int SF_VISIBLE = 5;

    bool _settingsDirty = false;

    // Text editing
    bool _editing = false;
    String _editBuffer;

    // WiFi scan
    bool _scanDone = false;
    int _wifiCount = 0;
    int _wifiSel = 0;

    // Render control
    bool _dirty = true;
    unsigned long _lastRender = 0;
    static constexpr unsigned long RENDER_INTERVAL = 50;

    void handleKeys();
    void handleChar(char c);
    void handleEnter();
    void handleBackspace();

    void renderMainMenu();
    void renderSettings();
    void renderWifiScan();
    void renderWifiPassword();
    void renderHelp();

    void startWifiScan();
    void applyEdit();

    LGFX_Sprite* _menuSprite = nullptr;
    bool _spriteReady = false;

    static constexpr int SW = 240;
    static constexpr int SH = 135;
    static constexpr int FH = 16;
    static constexpr int FW = 12;
    static constexpr int ROW = 18;
};
