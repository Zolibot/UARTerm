#pragma once
#include <Arduino.h>
#include <SD.h>
#include "AppConfig.h"

class SettingsManager {
public:
    bool begin();
    bool load(AppSettings& settings);
    bool save(const AppSettings& settings);
    bool isReady() const { return _ready; }

private:
    bool _ready = false;
    static constexpr const char* FILE_PATH = "/uarterm/settings.cfg";

    void writeLine(File& f, const char* key, const char* val);
    void writeLine(File& f, const char* key, unsigned long val);
    void writeLine(File& f, const char* key, int val);
    void writeLine(File& f, const char* key, bool val);

    const char* findVal(const String& line, const char* key);
};
