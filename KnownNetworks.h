#pragma once
#include <Arduino.h>
#include <SD.h>

class KnownNetworks {
public:
    static constexpr int MAX_NETWORKS = 16;
    static constexpr const char* FILE_PATH = "/uarterm/known.cfg";

    bool load();
    bool save();
    void add(const char* ssid, const char* password);
    bool isKnown(const char* ssid) const;
    const char* getPassword(const char* ssid) const;
    int count() const { return _count; }

private:
    struct Entry {
        char ssid[32];
        char password[64];
    };

    Entry _entries[MAX_NETWORKS];
    int _count = 0;

    int findIndex(const char* ssid) const;
};
