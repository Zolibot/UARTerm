#pragma once
#include <Arduino.h>
#include <SD.h>

class SDLogger {
public:
    bool begin(int modeNumber);
    void end();
    void write(uint8_t byte);
    void write(const uint8_t* data, size_t len);
    void flush();
    bool isActive() const { return _active; }
    const char* getFileName() const { return _fileName; }

private:
    static constexpr const char* DIR_PATH = "/uarterm";
    static constexpr size_t BUF_SIZE = 512;

    File _file;
    uint8_t _buffer[BUF_SIZE];
    size_t _pos = 0;
    bool _active = false;
    int _mode = 0;
    char _fileName[32] = "";

    int findNextNumber();
};
