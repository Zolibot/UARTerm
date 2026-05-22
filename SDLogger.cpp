#include "SDLogger.h"

bool SDLogger::begin(int modeNumber) {
    if (_active) end();

    _mode = modeNumber;
    int num = findNextNumber();

    char path[64];
    snprintf(path, sizeof(path), "%s/%03d_mode%d.log", DIR_PATH, num, _mode);

    _file = SD.open(path, FILE_WRITE);
    if (!_file) {
        _active = false;
        return false;
    }

    snprintf(_fileName, sizeof(_fileName), "%03d_mode%d.log", num, _mode);
    _pos = 0;
    _active = true;
    return true;
}

void SDLogger::end() {
    if (!_active) return;
    flush();
    _file.close();
    _active = false;
}

void SDLogger::write(uint8_t byte) {
    if (!_active) return;

    _buffer[_pos++] = byte;

    if (_pos >= BUF_SIZE) {
        flush();
    }
}

void SDLogger::write(const uint8_t* data, size_t len) {
    if (!_active || !data || len == 0) return;

    for (size_t i = 0; i < len; i++) {
        _buffer[_pos++] = data[i];
        if (_pos >= BUF_SIZE) {
            flush();
        }
    }
}

void SDLogger::flush() {
    if (!_active || _pos == 0) return;

    _file.write(_buffer, _pos);
    _file.flush();
    _pos = 0;
}

int SDLogger::findNextNumber() {
    int maxNum = 0;
    File dir = SD.open(DIR_PATH);
    if (!dir) return 1;

    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            const char* name = entry.name();
            if (strlen(name) >= 3) {
                int num = atoi(name);
                if (num > maxNum) maxNum = num;
            }
        }
        entry.close();
    }
    dir.close();

    return maxNum + 1;
}
