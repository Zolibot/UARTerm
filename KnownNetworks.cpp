#include "KnownNetworks.h"

bool KnownNetworks::load() {
    _count = 0;
    File f = SD.open(FILE_PATH, FILE_READ);
    if (!f) return false;

    while (f.available() && _count < MAX_NETWORKS) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int sep = line.indexOf('|');
        if (sep < 0) continue;

        String ssid = line.substring(0, sep);
        String pass = line.substring(sep + 1);
        ssid.trim();
        pass.trim();

        if (ssid.length() > 0 && ssid.length() < 32) {
            strncpy(_entries[_count].ssid, ssid.c_str(), sizeof(_entries[_count].ssid) - 1);
            strncpy(_entries[_count].password, pass.c_str(), sizeof(_entries[_count].password) - 1);
            _count++;
        }
    }
    f.close();
    return _count > 0;
}

bool KnownNetworks::save() {
    File f = SD.open(FILE_PATH, FILE_WRITE);
    if (!f) return false;

    for (int i = 0; i < _count; i++) {
        f.print(_entries[i].ssid);
        f.print('|');
        f.println(_entries[i].password);
    }
    f.close();
    return true;
}

void KnownNetworks::add(const char* ssid, const char* password) {
    if (!ssid || !ssid[0]) return;

    int idx = findIndex(ssid);
    if (idx >= 0) {
        strncpy(_entries[idx].password, password ? password : "",
                sizeof(_entries[idx].password) - 1);
        return;
    }

    if (_count >= MAX_NETWORKS) return;

    strncpy(_entries[_count].ssid, ssid, sizeof(_entries[_count].ssid) - 1);
    strncpy(_entries[_count].password, password ? password : "",
            sizeof(_entries[_count].password) - 1);
    _count++;
}

bool KnownNetworks::isKnown(const char* ssid) const {
    return findIndex(ssid) >= 0;
}

const char* KnownNetworks::getPassword(const char* ssid) const {
    int idx = findIndex(ssid);
    if (idx < 0) return nullptr;
    return _entries[idx].password;
}

int KnownNetworks::findIndex(const char* ssid) const {
    if (!ssid) return -1;
    for (int i = 0; i < _count; i++) {
        if (strcmp(_entries[i].ssid, ssid) == 0) return i;
    }
    return -1;
}
