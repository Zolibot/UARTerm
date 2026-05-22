#include "SettingsManager.h"
#include <SPI.h>

static constexpr int SD_CS   = 4;
static constexpr int SD_MOSI = 7;
static constexpr int SD_MISO = 8;
static constexpr int SD_SCLK = 6;

bool SettingsManager::begin() {
    _ready = SD.begin();
    if (!_ready) {
        SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
        _ready = SD.begin(SD_CS, SPI, 25000000);
    }
    if (_ready) {
        SD.mkdir("/uarterm");
    }
    return _ready;
}

static void trim(String& s) {
    while (s.length() > 0 && s[0] == ' ') s.remove(0, 1);
    while (s.length() > 0 && s[s.length() - 1] == ' ') s.remove(s.length() - 1, 1);
}

bool SettingsManager::load(AppSettings& settings) {
    if (!_ready) return false;
    if (!SD.exists(FILE_PATH)) return false;

    File f = SD.open(FILE_PATH, FILE_READ);
    if (!f) return false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int eq = line.indexOf('=');
        if (eq < 0) continue;

        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        trim(key);
        trim(val);

        if (key == "baud")                settings.baud = val.toInt();
        else if (key == "rx_pin")         settings.rxPin = (val == "auto") ? -1 : val.toInt();
        else if (key == "tx_pin")         settings.txPin = (val == "auto") ? -1 : val.toInt();
        else if (key == "ap_ssid")        strncpy(settings.apSsid, val.c_str(), sizeof(settings.apSsid) - 1);
        else if (key == "ap_password")    strncpy(settings.apPassword, val.c_str(), sizeof(settings.apPassword) - 1);
        else if (key == "sta_ssid")       strncpy(settings.staSsid, val.c_str(), sizeof(settings.staSsid) - 1);
        else if (key == "sta_password")   strncpy(settings.staPassword, val.c_str(), sizeof(settings.staPassword) - 1);
        else if (key == "tcp_port")       settings.tcpPort = val.toInt();
        else if (key == "sd_logging")     settings.sdLogging = (val == "true" || val == "1");
    }
    f.close();
    return true;
}

bool SettingsManager::save(const AppSettings& settings) {
    if (!_ready) return false;

    File f = SD.open(FILE_PATH, FILE_WRITE);
    if (!f) return false;

    f.println("baud=" + String(settings.baud));
    f.println("rx_pin=" + String(settings.rxPin >= 0 ? String(settings.rxPin) : "auto"));
    f.println("tx_pin=" + String(settings.txPin >= 0 ? String(settings.txPin) : "auto"));
    f.println("ap_ssid=" + String(settings.apSsid));
    f.println("ap_password=" + String(settings.apPassword));
    f.println("sta_ssid=" + String(settings.staSsid));
    f.println("sta_password=" + String(settings.staPassword));
    f.println("tcp_port=" + String(settings.tcpPort));
    f.println("sd_logging=" + String(settings.sdLogging ? "true" : "false"));

    f.close();
    return true;
}
