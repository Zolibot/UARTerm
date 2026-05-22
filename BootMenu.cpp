#include "BootMenu.h"
#include <M5Cardputer.h>

const unsigned long BootMenu::BAUD_RATES[] = {
    300, 1200, 2400, 4800, 9600, 19200, 38400, 57600,
    115200, 230400, 460800, 921600
};
const int BootMenu::BAUD_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

void BootMenu::begin(AppSettings& settings) {
    _settings = &settings;
    _editSettings = settings;
    _state = MAIN_MENU;
    _menuIdx = 0;
    _selectedMode = MODE_UART_SCANNER;
    _confirmed = false;
    _sfIdx = 0;
    _sfScroll = 0;
    _editing = false;
    _settingsDirty = false;
    _scanDone = false;
    _wifiCount = 0;
    _wifiSel = 0;
    _dirty = true;
    _lastRender = 0;

    if (!_menuSprite) {
        auto& gfx = M5Cardputer.Display;
        _menuSprite = new LGFX_Sprite(&gfx);
        _spriteReady = _menuSprite->createSprite(SW, SH);
        if (_spriteReady) {
            _menuSprite->setTextSize(2);
            _menuSprite->setTextWrap(false);
        }
    }
}

void BootMenu::reset() {
    if (_settings) _editSettings = *_settings;
    _state = MAIN_MENU;
    _menuIdx = 0;
    _confirmed = false;
    _sfIdx = 0;
    _sfScroll = 0;
    _editing = false;
    _scanDone = false;
    _wifiCount = 0;
    _wifiSel = 0;
    _dirty = true;
}

void BootMenu::retryPassword() {
    _state = WIFI_PASSWORD;
    _editBuffer = "";
    _editing = false;
    _dirty = true;
}

void BootMenu::update() {
    if (_state == CONFIRMED) return;
    handleKeys();
}

// ── Keyboard ──────────────────────────────────────────────────────

void BootMenu::handleKeys() {
    M5Cardputer.update();
    if (!M5Cardputer.Keyboard.isChange()) return;

    auto state = M5Cardputer.Keyboard.keysState();
    _dirty = true;

    for (auto c : state.word) {
        if ((c == 'p' || c == 'P') && _ssCb) { _ssCb(); return; }
    }

    if (_editing) {
        if (state.enter) {
            applyEdit();
            return;
        }
        if (state.del) {
            if (_editBuffer.length() > 0)
                _editBuffer.remove(_editBuffer.length() - 1);
            return;
        }
        for (auto c : state.word) {
            if (c >= 0x20 && c <= 0x7E)
                _editBuffer += c;
        }
        return;
    }

    if (_state == WIFI_PASSWORD) {
        if (state.enter) {
            handleEnter();
            return;
        }
        if (state.del) {
            if (_editBuffer.length() > 0)
                _editBuffer.remove(_editBuffer.length() - 1);
            return;
        }
        for (auto c : state.word) {
            if (c == 'c' || c == 'C') {
                _state = WIFI_SCAN;
                return;
            }
            if (c >= 0x20 && c <= 0x7E)
                _editBuffer += c;
        }
        return;
    }

    if (state.enter) {
        handleEnter();
        return;
    }

    if (state.del) {
        handleBackspace();
        return;
    }

    for (auto c : state.word) {
        handleChar(c);
    }
}

// ── State transitions ─────────────────────────────────────────────

void BootMenu::handleEnter() {
    switch (_state) {
        case MAIN_MENU: {
            int mode = _menuIdx + 1;
            if (mode == MODE_SETTINGS) {
                _editSettings = *_settings;
                _sfIdx = 0;
                _sfScroll = 0;
                _state = SETTINGS;
            } else if (mode == MODE_STA_SERVER) {
                _editSettings = *_settings;
                startWifiScan();
            } else if (mode == MODE_HELP) {
                _state = HELP_SCREEN;
            } else {
                _selectedMode = mode;
                _state = CONFIRMED;
                _confirmed = true;
            }
            break;
        }
        case SETTINGS: {
            if (_sfIdx == SF_SAVE) {
                if (_settings) *_settings = _editSettings;
                _settingsDirty = true;
                _state = MAIN_MENU;
                return;
            }
            FieldType ft;
            switch (_sfIdx) {
                case SF_BAUD: case SF_RX: case SF_TX: ft = FT_ENUM; break;
                case SF_AP_SSID: case SF_STA_SSID: ft = FT_TEXT; break;
                case SF_AP_PWD: case SF_STA_PWD: ft = FT_PASSWORD; break;
                case SF_PORT: ft = FT_NUM; break;
                case SF_SD_LOG: ft = FT_BOOL; break;
                default: ft = FT_ACTION; break;
            }
            if (ft == FT_TEXT || ft == FT_PASSWORD) {
                _editing = true;
                switch (_sfIdx) {
                    case SF_AP_SSID:  _editBuffer = _editSettings.apSsid; break;
                    case SF_AP_PWD:   _editBuffer = _editSettings.apPassword; break;
                    case SF_STA_SSID: _editBuffer = _editSettings.staSsid; break;
                    case SF_STA_PWD:  _editBuffer = _editSettings.staPassword; break;
                    default: _editing = false; break;
                }
            }
            break;
        }
        case WIFI_SCAN: {
            if (_wifiCount > 0 && _wifiSel >= 0 && _wifiSel < _wifiCount) {
                String ssid = WiFi.SSID(_wifiSel);
                strncpy(_editSettings.staSsid, ssid.c_str(),
                        sizeof(_editSettings.staSsid) - 1);

                if (_known && _known->isKnown(ssid.c_str())) {
                    const char* pass = _known->getPassword(ssid.c_str());
                    strncpy(_editSettings.staPassword, pass ? pass : "",
                            sizeof(_editSettings.staPassword) - 1);
                    if (_settings) *_settings = _editSettings;
                    _settingsDirty = true;
                    _selectedMode = MODE_STA_SERVER;
                    _confirmed = true;
                    _state = CONFIRMED;
                } else {
                    _editBuffer = "";
                    _state = WIFI_PASSWORD;
                }
            }
            break;
        }
        case WIFI_PASSWORD: {
            strncpy(_editSettings.staPassword, _editBuffer.c_str(),
                    sizeof(_editSettings.staPassword) - 1);
            _selectedMode = MODE_STA_SERVER;
            if (_settings) *_settings = _editSettings;
            _settingsDirty = true;
            _confirmed = true;
            _state = CONFIRMED;
            break;
        }
        case HELP_SCREEN:
            _state = MAIN_MENU;
            break;
        default: break;
    }
}

void BootMenu::handleBackspace() {
    switch (_state) {
        case HELP_SCREEN:
        case WIFI_SCAN:
            _state = MAIN_MENU;
            break;
        case WIFI_PASSWORD:
            _state = WIFI_SCAN;
            break;
        default: break;
    }
}

void BootMenu::handleChar(char c) {
    switch (_state) {
        case MAIN_MENU: {
            if (c == ';' && _menuIdx > 0) _menuIdx--;
            if (c == '.' && _menuIdx < 4) _menuIdx++;
            break;
        }
        case SETTINGS: {
            if (c == ';' && _sfIdx > 0) _sfIdx--;
            if (c == '.' && _sfIdx < SF_COUNT - 1) _sfIdx++;

            if (_sfIdx < _sfScroll) _sfScroll = _sfIdx;
            if (_sfIdx >= _sfScroll + SF_VISIBLE) _sfScroll = _sfIdx - SF_VISIBLE + 1;

            if (c == ',' || c == '/') {
                bool up = (c == '/');
                switch (_sfIdx) {
                    case SF_BAUD: {
                        unsigned long cur = _editSettings.baud;
                        if (cur == 0) cur = BAUD_RATES[0];
                        for (int i = 0; i < BAUD_COUNT; i++) {
                            if (BAUD_RATES[i] == cur) {
                                int ni = up ? (i + 1) % BAUD_COUNT
                                            : (i - 1 + BAUD_COUNT) % BAUD_COUNT;
                                _editSettings.baud = BAUD_RATES[ni];
                                return;
                            }
                        }
                        _editSettings.baud = BAUD_RATES[0];
                        break;
                    }
                    case SF_RX: {
                        int& v = _editSettings.rxPin;
                        int forbid = _editSettings.txPin;
                        do {
                            if (up) { v = (v == 1) ? 2 : (v == 2) ? -1 : 1; }
                            else    { v = (v == 1) ? -1 : (v == 2) ? 1 : 2; }
                        } while (v >= 0 && v == forbid);
                        break;
                    }
                    case SF_TX: {
                        int& v = _editSettings.txPin;
                        int forbid = _editSettings.rxPin;
                        do {
                            if (up) { v = (v == 1) ? 2 : (v == 2) ? -1 : 1; }
                            else    { v = (v == 1) ? -1 : (v == 2) ? 1 : 2; }
                        } while (v >= 0 && v == forbid);
                        break;
                    }
                    case SF_PORT: {
                        int& v = (int&)_editSettings.tcpPort;
                        v = constrain(up ? v + 1 : v - 1, 1, 65535);
                        break;
                    }
                    case SF_SD_LOG: {
                        _editSettings.sdLogging = !_editSettings.sdLogging;
                        break;
                    }
                    default: break;
                }
            }

            if (c == 'c' || c == 'C') {
                _state = MAIN_MENU;
            }
            break;
        }
        case WIFI_SCAN: {
            if (c == ';' && _wifiSel > 0) _wifiSel--;
            if (c == '.' && _wifiSel < _wifiCount - 1) _wifiSel++;
            if (c == 'c' || c == 'C') {
                WiFi.scanDelete();
                _state = MAIN_MENU;
            }
            break;
        }
        case WIFI_PASSWORD:
            break;
        case HELP_SCREEN:
            if (c == 'c' || c == 'C') _state = MAIN_MENU;
            break;
        default: break;
    }
}

void BootMenu::applyEdit() {
    _editing = false;
    switch (_sfIdx) {
        case SF_AP_SSID:
            strncpy(_editSettings.apSsid, _editBuffer.c_str(),
                    sizeof(_editSettings.apSsid) - 1);
            break;
        case SF_AP_PWD:
            strncpy(_editSettings.apPassword, _editBuffer.c_str(),
                    sizeof(_editSettings.apPassword) - 1);
            break;
        case SF_STA_SSID:
            strncpy(_editSettings.staSsid, _editBuffer.c_str(),
                    sizeof(_editSettings.staSsid) - 1);
            break;
        case SF_STA_PWD:
            strncpy(_editSettings.staPassword, _editBuffer.c_str(),
                    sizeof(_editSettings.staPassword) - 1);
            break;
        default: break;
    }
}

// ── WiFi scan ─────────────────────────────────────────────────────

void BootMenu::startWifiScan() {
    _state = WIFI_SCAN;
    _scanDone = false;
    _wifiCount = 0;
    _wifiSel = 0;

    WiFi.scanDelete();
    WiFi.scanNetworks(true);
}

// ── Rendering ─────────────────────────────────────────────────────

static void drawStr(LGFX_Sprite* sprite, int x, int y, const char* s, uint16_t fg, uint16_t bg) {
    if (!sprite) return;
    sprite->setCursor(x, y);
    sprite->setTextColor(fg, bg);
    sprite->print(s);
}

void BootMenu::capture(uint16_t* buf) const {
    if (_menuSprite) {
        _menuSprite->readRect(0, 0, SW, SH, buf);
    }
}

void BootMenu::render() {
    if (!_spriteReady || !_menuSprite) return;

    unsigned long now = millis();
    if (!_dirty && now - _lastRender < RENDER_INTERVAL) return;
    _lastRender = now;

    _menuSprite->fillSprite(TFT_BLACK);
    _menuSprite->setTextSize(2);

    switch (_state) {
        case MAIN_MENU:      renderMainMenu();  break;
        case SETTINGS:       renderSettings();  break;
        case WIFI_SCAN:      renderWifiScan();  break;
        case WIFI_PASSWORD:  renderWifiPassword(); break;
        case HELP_SCREEN:    renderHelp();      break;
        case CONFIRMED:      break;
    }

    _menuSprite->pushSprite(0, 0);
    _dirty = false;
}

void BootMenu::renderMainMenu() {
    auto* gfx = _menuSprite;

    drawStr(gfx, 0, 0, "=== SELECT MODE ===", TFT_CYAN, TFT_BLACK);

    const char* items[] = {
        "1. UART Scanner",
        "2. AP + TCP Server",
        "3. STA + TCP Server",
        "4. Manual Settings",
        "5. Help / About"
    };

    for (int i = 0; i < 5; i++) {
        int y = ROW + i * ROW;
        if (i == _menuIdx) {
            gfx->fillRect(4, y, SW - 8, FH, TFT_BLUE);
            drawStr(gfx, 8, y, items[i], TFT_WHITE, TFT_BLUE);
        } else {
            drawStr(gfx, 8, y, items[i], TFT_LIGHTGRAY, TFT_BLACK);
        }
    }

    drawStr(gfx, 0, SH - FH, " ;=UP  .=DOWN  Enter=OK", TFT_DARKGREY, TFT_BLACK);
}

void BootMenu::renderSettings() {
    auto* gfx = _menuSprite;

    drawStr(gfx, 0, 0, "=== MANUAL SETTINGS ===", TFT_CYAN, TFT_BLACK);

    if (_sfScroll > 0) {
        drawStr(gfx, SW - FW * 2, 0, "^", TFT_DARKGREY, TFT_BLACK);
    }

    char buf[40];
    int end = min(_sfScroll + SF_VISIBLE, (int)SF_COUNT);
    for (int vi = 0; vi < end - _sfScroll; vi++) {
        int i = vi + _sfScroll;
        int y = ROW + vi * ROW;
        bool sel = (i == _sfIdx);
        bool isEdit = sel && _editing;

        uint16_t bg = isEdit ? TFT_MAROON : (sel ? TFT_NAVY : TFT_BLACK);
        uint16_t fg = sel ? TFT_WHITE : TFT_LIGHTGRAY;

        if (sel || isEdit) gfx->fillRect(4, y, SW - 8, FH, bg);

        switch (i) {
            case SF_BAUD:
                if (_editSettings.baud == 0)
                    snprintf(buf, sizeof(buf), " Baud: Auto");
                else
                    snprintf(buf, sizeof(buf), " Baud: %lu", _editSettings.baud);
                drawStr(gfx, 0, y, buf, fg, bg);
                if (sel) drawStr(gfx, SW - FW * 4, y, "</>", TFT_CYAN, bg);
                break;
            case SF_RX: {
                const char* v = _editSettings.rxPin == 1 ? "G1" :
                                _editSettings.rxPin == 2 ? "G2" : "Auto";
                snprintf(buf, sizeof(buf), " RX: %s", v);
                drawStr(gfx, 0, y, buf, fg, bg);
                if (sel) drawStr(gfx, SW - FW * 4, y, "</>", TFT_CYAN, bg);
                break;
            }
            case SF_TX: {
                const char* v = _editSettings.txPin == 1 ? "G1" :
                                _editSettings.txPin == 2 ? "G2" : "Auto";
                snprintf(buf, sizeof(buf), " TX: %s", v);
                drawStr(gfx, 0, y, buf, fg, bg);
                if (sel) drawStr(gfx, SW - FW * 4, y, "</>", TFT_CYAN, bg);
                break;
            }
            case SF_AP_SSID:
                snprintf(buf, sizeof(buf), " AP: %s%s",
                    isEdit ? _editBuffer.c_str() : _editSettings.apSsid,
                    isEdit ? "_" : "");
                drawStr(gfx, 0, y, buf, fg, bg);
                break;
            case SF_AP_PWD:
                snprintf(buf, sizeof(buf), " AP Pwd: %s%s",
                    isEdit ? _editBuffer.c_str() : "****",
                    isEdit ? "_" : "");
                drawStr(gfx, 0, y, buf, fg, bg);
                break;
            case SF_STA_SSID:
                snprintf(buf, sizeof(buf), " STA: %s%s",
                    isEdit ? _editBuffer.c_str() : _editSettings.staSsid,
                    isEdit ? "_" : "");
                drawStr(gfx, 0, y, buf, fg, bg);
                break;
            case SF_STA_PWD:
                snprintf(buf, sizeof(buf), " STA Pwd: %s%s",
                    isEdit ? _editBuffer.c_str() : "****",
                    isEdit ? "_" : "");
                drawStr(gfx, 0, y, buf, fg, bg);
                break;
            case SF_PORT:
                snprintf(buf, sizeof(buf), " Port: %u", _editSettings.tcpPort);
                drawStr(gfx, 0, y, buf, fg, bg);
                if (sel) drawStr(gfx, SW - FW * 4, y, "</>", TFT_CYAN, bg);
                break;
            case SF_SD_LOG:
                snprintf(buf, sizeof(buf), " SD Log: %s",
                    _editSettings.sdLogging ? "On" : "Off");
                drawStr(gfx, 0, y, buf, fg, bg);
                break;
            case SF_SAVE:
                gfx->fillRect(4, y, SW - 8, FH, sel ? TFT_DARKGREEN : TFT_BLACK);
                drawStr(gfx, 8, y, "[ Save & Exit ]",
                    sel ? TFT_WHITE : TFT_GREEN,
                    sel ? TFT_DARKGREEN : TFT_BLACK);
                break;
        }
    }

    if (_sfScroll + SF_VISIBLE < SF_COUNT) {
        drawStr(gfx, SW - FW * 2, SH - FH - ROW, "v", TFT_DARKGREY, TFT_BLACK);
    }

    drawStr(gfx, 0, SH - FH,
        " ;=UP .=DOWN ,=- /=+ Enter=edit C=back",
        TFT_DARKGREY, TFT_BLACK);
}

void BootMenu::renderWifiScan() {
    _dirty = true;
    auto* gfx = _menuSprite;

    int result = WiFi.scanComplete();
    if (result == -1) {
        static unsigned long lastDot = 0;
        static int dotCount = 0;
        if (millis() - lastDot > 300) {
            dotCount = (dotCount + 1) % 4;
            lastDot = millis();
        }
        char buf[24];
        snprintf(buf, sizeof(buf), "Scanning%.*s", dotCount, "...");
        drawStr(gfx, 40, SH / 2 - FH / 2, buf, TFT_YELLOW, TFT_BLACK);
        return;
    }

    if (result == -2) {
        startWifiScan();
        return;
    }

    _wifiCount = result;
    _scanDone = true;

    drawStr(gfx, 0, 0, "=== SELECT NETWORK ===", TFT_CYAN, TFT_BLACK);

    if (_wifiCount == 0) {
        drawStr(gfx, 20, SH / 2 - FH / 2, "No networks found", TFT_RED, TFT_BLACK);
        drawStr(gfx, 0, SH - FH, " C=back", TFT_DARKGREY, TFT_BLACK);
        return;
    }

    int maxVisible = (SH - FH - ROW) / ROW;
    int scrollOffset = 0;
    if (_wifiSel >= maxVisible) scrollOffset = _wifiSel - maxVisible + 1;

    int visible = min(_wifiCount - scrollOffset, maxVisible);

    if (scrollOffset > 0) {
        drawStr(gfx, SW - FW * 2, 0, "^", TFT_DARKGREY, TFT_BLACK);
    }

    for (int i = 0; i < visible; i++) {
        int idx = i + scrollOffset;
        int y = ROW + i * ROW;
        bool sel = (idx == _wifiSel);

        uint16_t bg = sel ? TFT_BLUE : TFT_BLACK;
        uint16_t fg = sel ? TFT_WHITE : TFT_LIGHTGRAY;

        if (sel) gfx->fillRect(4, y, SW - 8, FH, bg);

        String ssid = WiFi.SSID(idx);
        bool known = _known && _known->isKnown(ssid.c_str());

        int rssi = WiFi.RSSI(idx);
        const char* bars;
        if (rssi > -60)      bars = "###";
        else if (rssi > -75) bars = " ##";
        else if (rssi > -90) bars = "  #";
        else                 bars = "";

        if (ssid.length() > 14) ssid = ssid.substring(0, 12) + "..";

        char line[40];
        snprintf(line, sizeof(line), "%s %s", ssid.c_str(), bars);
        uint16_t textColor = known ? TFT_GREEN : fg;
        drawStr(gfx, 4, y, line, textColor, bg);
    }

    if (scrollOffset + visible < _wifiCount) {
        drawStr(gfx, SW - FW * 2, SH - FH - ROW, "v", TFT_DARKGREY, TFT_BLACK);
    }

    drawStr(gfx, 0, SH - FH - ROW,
        " ;=UP .=DOWN Enter=OK C=back",
        TFT_DARKGREY, TFT_BLACK);
    drawStr(gfx, 0, SH - FH,
        " green=known  Del=back",
        TFT_DARKGREY, TFT_BLACK);
}

void BootMenu::renderWifiPassword() {
    _dirty = true;
    auto* gfx = _menuSprite;

    drawStr(gfx, 0, 0, "=== ENTER PASSWORD ===", TFT_CYAN, TFT_BLACK);

    char buf[40];
    snprintf(buf, sizeof(buf), "SSID: %s", _editSettings.staSsid);
    drawStr(gfx, 0, ROW, buf, TFT_YELLOW, TFT_BLACK);

    drawStr(gfx, 0, ROW * 2, "Password:", TFT_LIGHTGRAY, TFT_BLACK);

    gfx->fillRect(4, ROW * 3, SW - 8, FH, TFT_NAVY);
    drawStr(gfx, 4, ROW * 3, (_editBuffer + ((millis() / 500) & 1 ? "_" : " ")).c_str(),
            TFT_WHITE, TFT_NAVY);

    drawStr(gfx, 0, SH - FH,
        " Enter=OK  Del=back  C=cancel",
        TFT_DARKGREY, TFT_BLACK);
}

void BootMenu::renderHelp() {
    auto* gfx = _menuSprite;

    // Temporarily use smaller text size for help content
    gfx->setTextSize(1);

    drawStr(gfx, 0, 0, "=== UARTerm Help ===", TFT_CYAN, TFT_BLACK);

    const char* lines[] = {
        " Navigation:",
        " ; - Up    . - Down",
        " Enter - Select",
        " Del - Back  C - Exit",
        "",
        " Monitor mode:",
        " ; - Baud+  . - Baud-",
        " / - Swap pins",
        " R - Rescan   H - Hex",
        " C - Clear  Del - Menu",
        " S - SD log  P - Screenshot"
    };

    int n = sizeof(lines) / sizeof(lines[0]);
    for (int i = 0; i < n; i++) {
        drawStr(gfx, 4, ROW + i * ROW, lines[i], TFT_LIGHTGRAY, TFT_BLACK);
    }

    // Restore normal text size
    gfx->setTextSize(2);

    drawStr(gfx, 0, SH - FH, " C=back", TFT_DARKGREY, TFT_BLACK);
}
