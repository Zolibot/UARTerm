#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

#define PIN_G1 1
#define PIN_G2 2

class UartScanner {
public:
    enum State : uint8_t {
        STATE_PIN_DETECT,
        STATE_BAUD_SCAN,
        STATE_CONNECTED
    };

    struct Config {
        int rxPin = -1;
        int txPin = -1;
        unsigned long baudRate = 0;
    };

    UartScanner(int pinA = PIN_G1, int pinB = PIN_G2);

    void begin();
    void update();
    void reset();
    bool isConnected() const { return _state == STATE_CONNECTED; }
    State getState() const { return _state; }
    const Config& getConfig() const { return _config; }
    unsigned long getCurrentBaudTest() const { return _currentBaudTest; }
    HardwareSerial& getSerial() { return _serial; }

    void baudUp();
    void baudDown();
    void swapPins();
    void clearManualOverride();
    bool isManualOverride() const { return _manualOverride; }

private:
    static const unsigned long BAUD_RATES[];
    static const int BAUD_COUNT;
    static constexpr unsigned long SCAN_BAUD_MS = 300;
    static constexpr unsigned long IDLE_MS = 10000;
    static constexpr unsigned long PIN_TIMEOUT_MS = 2000;
    static constexpr int SCAN_BUF_SIZE = 128;

    int _pinA, _pinB;
    State _state = STATE_PIN_DETECT;
    Config _config;
    unsigned long _timer = 0;
    int _baudIndex = 0;
    unsigned long _currentBaudTest = 0;
    int _scanCycles = 0;
    bool _manualOverride = false;
    unsigned long _manualBaud = 0;
    HardwareSerial _serial;

    uint8_t _scanBuf[SCAN_BUF_SIZE];
    int _scanPos = 0;
    int _bestScore = 0;
    int _bestBaudIdx = -1;

    int findRxPin();
    void applyBaudRate(unsigned long baud);
    void stopSerial();
    int scoreByte(uint8_t b);
    void startScan();
    void tryNextBaud();
};
