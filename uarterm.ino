#include <M5Cardputer.h>
#include "UartScanner.h"
#include "DisplayManager.h"
#include "BootMenu.h"
#include "NetManager.h"
#include "SettingsManager.h"
#include "SDLogger.h"
#include "KnownNetworks.h"
#include "AppConfig.h"

enum AppMode : uint8_t {
    APP_BOOT,
    APP_SCANNER,
    APP_AP,
    APP_STA
};

UartScanner scanner(PIN_G1, PIN_G2);
DisplayManager display;
BootMenu bootMenu;
NetManager network;
SettingsManager settingsMan;
SDLogger sdLogger;
KnownNetworks knownNetworks;
AppSettings appSettings;

AppMode _appMode = APP_BOOT;
bool _sdLogActive = false;
unsigned long _lastRender = 0;

void pumpUart();
void startMode(AppMode mode);
void stopMode();
void takeScreenshot();

// ── Keyboard per-mode ─────────────────────────────────────────────

bool handleUartKeyboard() {
    M5Cardputer.update();
    if (!M5Cardputer.Keyboard.isChange()) return false;
    if (!M5Cardputer.Keyboard.isPressed()) return false;

    auto state = M5Cardputer.Keyboard.keysState();

    if (state.del) return true;

    for (auto c : state.word) {
        if (c == 'p' || c == 'P') { takeScreenshot(); continue; }
        if (c == ';') {
            scanner.baudUp();
            char msg[24];
            snprintf(msg, sizeof(msg), "Manual %lu baud", scanner.getConfig().baudRate);
            display.showMessage(msg);
        }
        if (c == '.') {
            scanner.baudDown();
            char msg[24];
            snprintf(msg, sizeof(msg), "Manual %lu baud", scanner.getConfig().baudRate);
            display.showMessage(msg);
        }
        if (c == '/') {
            scanner.swapPins();
            display.showMessage("Pins swapped");
        }
        if (c == 'r' || c == 'R') {
            if (scanner.isManualOverride()) {
                scanner.clearManualOverride();
                display.showMessage("Auto mode restored");
            } else {
                scanner.reset();
                display.showMessage("Rescanning...");
            }
        }
        if (c == 'h' || c == 'H') {
            display.toggleHexMode();
            display.showMessage(display.getHexMode() ? "HEX mode" : "TEXT mode");
        }
        if (c == 'c' || c == 'C') {
            display.showMessage("Cleared");
        }
        if (c == 's' || c == 'S') {
            if (_sdLogActive) {
                sdLogger.end();
                _sdLogActive = false;
                display.showMessage("SD log OFF");
            } else {
                int modeNum = (_appMode == APP_AP) ? 2 : (_appMode == APP_STA) ? 3 : 1;
                _sdLogActive = sdLogger.begin(modeNum);
                if (_sdLogActive) {
                    char msg[32];
                    snprintf(msg, sizeof(msg), "SD log ON: %s", sdLogger.getFileName());
                    display.showMessage(msg);
                    display.setSDLogFile(sdLogger.getFileName());
                } else {
                    display.showMessage("SD log FAILED!");
                }
            }
            display.setSDLogging(_sdLogActive);
        }
    }
    return false;
}

// ── UART pump shared by all modes ────────────────────────────────

void pumpUart() {
    if (!scanner.isConnected()) return;

    auto& uart = scanner.getSerial();
    uint8_t buf[256];
    int cnt = 0;

    while (uart.available() && cnt < 256) {
        uint8_t b = uart.read();
        display.addChar((char)b);
        buf[cnt++] = b;
    }

    if (cnt > 0) {
        // Log in any mode
        if (_sdLogActive && sdLogger.isActive()) {
            sdLogger.write(buf, cnt);
        }
        // Broadcast only in network modes
        if (_appMode == APP_AP || _appMode == APP_STA) {
            if (network.isRunning() && network.getClientCount() > 0) {
                network.broadcast(buf, cnt);
            }
        }
    }
}

// ── Mode management ───────────────────────────────────────────────

void startMode(AppMode mode) {
    stopMode();
    _appMode = mode;

    switch (mode) {
        case APP_SCANNER:
            display.setDisplayMode(DisplayManager::DISP_UART);
            display.setBottomInfo("");
            display.showMessage("UART Scanner mode");
            display.showMessage("Awaiting signal on G1/G2...");
            scanner.begin();
            break;

        case APP_AP: {
            const char* ssid = appSettings.apSsid[0] ? appSettings.apSsid : "Cardputer_UART";
            const char* pass = appSettings.apPassword;
            uint16_t port = appSettings.tcpPort ? appSettings.tcpPort : 8888;

            {
                auto& gfx = M5Cardputer.Display;
                gfx.fillScreen(TFT_BLACK);
                gfx.setTextSize(2);
                gfx.setTextColor(TFT_WHITE, TFT_BLACK);
                gfx.setCursor(10, 20);
                gfx.println("Starting AP...");
                gfx.setCursor(10, 42);
                gfx.print(ssid);
            }

            if (!network.beginAP(ssid, pass[0] ? pass : nullptr, port)) {
                display.showMessage("AP failed!");
                delay(2000);
                _appMode = APP_BOOT;
                bootMenu.reset();
                return;
            }

            display.setDisplayMode(DisplayManager::DISP_NETWORK);
            display.setNetworkInfo("AP", network.getIPStr(), network.getPort(), 0);

            scanner.begin();

            if (appSettings.sdLogging) {
                _sdLogActive = sdLogger.begin(2);
                display.setSDLogging(_sdLogActive);
                if (_sdLogActive) display.setSDLogFile(sdLogger.getFileName());
            }
            break;
        }

        case APP_STA:
            {
                auto& gfx = M5Cardputer.Display;
                gfx.fillScreen(TFT_BLACK);
                gfx.setTextSize(2);
                gfx.setTextColor(TFT_WHITE, TFT_BLACK);
                gfx.setCursor(10, 20);
                gfx.println("Connecting to");
                gfx.setCursor(10, 42);
                gfx.print(appSettings.staSsid);
            }

            if (!network.beginSTA(appSettings.staSsid, appSettings.staPassword,
                                  appSettings.tcpPort ? appSettings.tcpPort : 8888)) {
                auto& gfx = M5Cardputer.Display;
                gfx.fillScreen(TFT_BLACK);
                gfx.setTextSize(2);
                gfx.setTextColor(TFT_RED, TFT_BLACK);
                gfx.setCursor(10, 20);
                gfx.print("Connection");
                gfx.setCursor(10, 42);
                gfx.print("failed!");
                gfx.setCursor(10, 64);
                gfx.setTextColor(TFT_WHITE, TFT_BLACK);
                gfx.print(appSettings.staSsid);
                delay(2000);
                appSettings.staPassword[0] = '\0';
                _appMode = APP_BOOT;
                bootMenu.retryPassword();
                return;
            }

            knownNetworks.add(appSettings.staSsid, appSettings.staPassword);
            knownNetworks.save();

            display.setDisplayMode(DisplayManager::DISP_NETWORK);
            display.setNetworkInfo("STA", network.getIPStr(), network.getPort(), 0);

            scanner.begin();

            if (appSettings.sdLogging) {
                _sdLogActive = sdLogger.begin(3);
                display.setSDLogging(_sdLogActive);
                if (_sdLogActive) display.setSDLogFile(sdLogger.getFileName());
            }
            break;

        default:
            break;
    }
}

void stopMode() {
    if (_sdLogActive) {
        sdLogger.end();
        _sdLogActive = false;
        display.setSDLogging(false);
        display.setSDLogFile("");
    }
    if (network.isRunning()) {
        network.stop();
    }
    WiFi.mode(WIFI_OFF);
}

// ── Boot Menu integration ─────────────────────────────────────────

void handleBootMenuEnter() {
    if (bootMenu.isConfirmed()) {
        int mode = bootMenu.getSelectedMode();

        // Save settings to SD if changed
        if (bootMenu.isSettingsDirty()) {
            if (settingsMan.isReady()) {
                settingsMan.save(appSettings);
                knownNetworks.add(appSettings.staSsid, appSettings.staPassword);
                knownNetworks.save();
            }
        }

        switch (mode) {
            case BootMenu::MODE_UART_SCANNER:
                startMode(APP_SCANNER);
                break;
            case BootMenu::MODE_AP_SERVER:
                startMode(APP_AP);
                break;
            case BootMenu::MODE_STA_SERVER:
                startMode(APP_STA);
                break;
            case BootMenu::MODE_SETTINGS:
                break;
        }
    }
}

// ── Screenshot ────────────────────────────────────────────────────

void takeScreenshot() {
    static unsigned long _lastSS = 0;
    if (millis() - _lastSS < 500) return;
    _lastSS = millis();

    auto& gfx = M5Cardputer.Display;
    int w = gfx.width();
    int h = gfx.height();

    uint16_t* pixels = new (std::nothrow) uint16_t[w * h];
    if (!pixels) { display.showMessage("SS: no mem"); return; }

    if (_appMode == APP_BOOT) {
        bootMenu.capture(pixels);
    } else {
        display.capture(pixels);
    }

    int num = 1;
    File dir = SD.open("/uarterm");
    if (dir) {
        for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
            if (!entry.isDirectory()) {
                const char* name = entry.name();
                if (strncmp(name, "SCR_", 4) == 0) {
                    int n = atoi(name + 4);
                    if (n >= num) num = n + 1;
                }
            }
            entry.close();
        }
        dir.close();
    }

    char path[32];
    snprintf(path, sizeof(path), "/uarterm/SCR_%03d.bmp", num);
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        delete[] pixels;
        display.showMessage("SS: no SD");
        return;
    }

    uint32_t rowSize = (w * 2 + 3) & ~3;
    uint32_t dataSize = rowSize * h;
    uint32_t fileSize = 14 + 40 + 16 + dataSize;

    uint8_t hdr[14] = {
        'B','M', 0,0,0,0, 0,0, 0,0, 14+40+16,0,0,0
    };
    hdr[2] = fileSize; hdr[3] = fileSize >> 8;
    hdr[4] = fileSize >> 16; hdr[5] = fileSize >> 24;
    f.write(hdr, 14);

    uint8_t dib[40] = {
        40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 16,0, 3,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
    };
    dib[4] = w; dib[5] = w >> 8;
    dib[8] = h; dib[9] = h >> 8;
    f.write(dib, 40);

    uint32_t masks[3] = { 0xF800, 0x07E0, 0x001F };
    f.write((uint8_t*)masks, 12);

    for (int y = h - 1; y >= 0; y--) {
        f.write((uint8_t*)(pixels + y * w), w * 2);
        uint8_t pad = rowSize - w * 2;
        if (pad) f.write((uint8_t*)"\0\0\0\0", pad);
        if (y % 20 == 0) delay(1);
    }

    f.close();
    delete[] pixels;

    char msg[24];
    snprintf(msg, sizeof(msg), "SS: SCR_%03d.bmp", num);
    display.showMessage(msg);
}

// ── Arduino entry points ──────────────────────────────────────────

static void showSplash() {
    auto* gfx = bootMenu.getSprite();
    if (!gfx) return;

    gfx->fillSprite(TFT_BLACK);

    gfx->setTextSize(4);
    gfx->setTextColor(TFT_CYAN, TFT_BLACK);
    int w = 7 * 6 * 4;
    gfx->setCursor((gfx->width() - w) / 2, gfx->height() / 2 - 28);
    gfx->print("UARTerm");

    gfx->setTextSize(1);
    gfx->setTextColor(TFT_DARKGREY, TFT_BLACK);
    w = 10 * 6;
    gfx->setCursor((gfx->width() - w) / 2, gfx->height() / 2 + 10);
    gfx->print("Cardputer ADV");

    gfx->pushSprite(0, 0);

    unsigned long start = millis();
    while (millis() - start < 2500) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto ks = M5Cardputer.Keyboard.keysState();
            for (auto k : ks.word) {
                if (k == 'p' || k == 'P') { takeScreenshot(); break; }
            }
        }
        delay(20);
    }
}

void setup() {
    Serial.begin(115200);

    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    display.init();

    settingsMan.begin();
    settingsMan.load(appSettings);
    knownNetworks.load();

    bootMenu.begin(appSettings);
    bootMenu.setKnownNetworks(&knownNetworks);
    bootMenu.setScreenshotCallback(takeScreenshot);

    showSplash();

    _appMode = APP_BOOT;
}

void loop() {
    switch (_appMode) {
        case APP_BOOT:
            bootMenu.update();
            bootMenu.render();
            handleBootMenuEnter();
            break;

        case APP_SCANNER:
            if (handleUartKeyboard()) { stopMode(); _appMode = APP_BOOT; bootMenu.reset(); break; }
            scanner.update();
            pumpUart();

            if (millis() - _lastRender > 50) {
                display.render(scanner);
                _lastRender = millis();
            }
            break;

        case APP_AP:
        case APP_STA:
            if (handleUartKeyboard()) { stopMode(); _appMode = APP_BOOT; bootMenu.reset(); break; }
            scanner.update();
            network.update();
            pumpUart();

            {
                const char* modeStr = (_appMode == APP_AP) ? "AP" :
                    (network.isSTAConnected() ? "STA" : "");
                if (modeStr[0]) {
                    display.setNetworkInfo(modeStr, network.getIPStr(), network.getPort(),
                                           network.getClientCount());

                    // Bottom bar: network info + SSID
                    const char* ssid = (_appMode == APP_AP) ? appSettings.apSsid : appSettings.staSsid;
                    char bottomInfo[42];
                    snprintf(bottomInfo, sizeof(bottomInfo), "%s %s %s:%u C:%d",
                        modeStr, ssid, network.getIPStr(), network.getPort(), network.getClientCount());
                    display.setBottomInfo(bottomInfo);
                } else {
                    display.setNetworkInfo("", "", 0, 0);
                    display.setBottomInfo("");
                }
            }

            if (millis() - _lastRender > 50) {
                display.render(scanner);
                _lastRender = millis();
            }
            break;
    }

    delay(5);
}
