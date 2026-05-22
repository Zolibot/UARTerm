#include "UartScanner.h"

const unsigned long UartScanner::BAUD_RATES[] = {
    300, 1200, 2400, 4800, 9600, 19200, 38400, 57600,
    115200, 230400, 460800, 921600
};
const int UartScanner::BAUD_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

UartScanner::UartScanner(int pinA, int pinB)
    : _pinA(pinA), _pinB(pinB), _serial(2)
{}

void UartScanner::begin() {
    _state = STATE_PIN_DETECT;
    _timer = millis();
    _baudIndex = 0;
    _config = Config();
    _currentBaudTest = 0;
    _scanCycles = 0;
    _bestScore = 0;
    _bestBaudIdx = -1;
    _scanPos = 0;
}

void UartScanner::reset() {
    stopSerial();
    begin();
}

void UartScanner::stopSerial() {
    _serial.end();
    delay(10);
}

void UartScanner::applyBaudRate(unsigned long baud) {
    stopSerial();
    if (_config.rxPin >= 0 && _config.txPin >= 0 && _config.rxPin != _config.txPin) {
        _serial.begin(baud, SERIAL_8N1, _config.rxPin, _config.txPin);
    }
    _currentBaudTest = baud;
}

int UartScanner::scoreByte(uint8_t b) {
    if (b == '$') return 5;
    if (b == ',') return 3;
    if (b == '*') return 3;
    if (b == '\n') return 3;
    if (b == '\r') return 2;
    if (b >= 0x20 && b <= 0x7E) return 1;
    if (b == '\t' || b == '\b') return 1;
    return 0;
}

void UartScanner::startScan() {
    _baudIndex = 0;
    _bestScore = 0;
    _bestBaudIdx = -1;
    _scanPos = 0;
    _scanCycles = 0;
    _state = STATE_BAUD_SCAN;

    if (_config.rxPin >= 0) {
        while (_serial.available()) _serial.read();
    }
    tryNextBaud();
}

void UartScanner::tryNextBaud() {
    if (_baudIndex >= BAUD_COUNT) {
        _scanCycles++;
        _baudIndex = 0;

        if (_bestBaudIdx >= 0) {
            _config.baudRate = BAUD_RATES[_bestBaudIdx];
            applyBaudRate(_config.baudRate);
            _state = STATE_CONNECTED;
            _timer = millis();
            return;
        }

        if (_scanCycles >= 2) {
            _state = STATE_PIN_DETECT;
            _timer = millis();
            _scanCycles = 0;
        }
        return;
    }

    unsigned long baud = BAUD_RATES[_baudIndex];
    _config.baudRate = baud;
    applyBaudRate(baud);
    _scanPos = 0;
    _timer = millis();
}

// ── Baud up/down ──────────────────────────────────────────────────

void UartScanner::baudUp() {
    _manualOverride = true;
    if (_config.baudRate == 0) _config.baudRate = BAUD_RATES[0];
    for (int i = 0; i < BAUD_COUNT - 1; i++) {
        if (BAUD_RATES[i] == _config.baudRate || (_manualBaud > 0 && BAUD_RATES[i] == _manualBaud)) {
            _manualBaud = BAUD_RATES[i + 1];
            _config.baudRate = _manualBaud;
            _config.rxPin = (_config.rxPin < 0) ? _pinA : _config.rxPin;
            _config.txPin = (_config.txPin < 0) ? _pinB : _config.txPin;
            applyBaudRate(_manualBaud);
            _state = STATE_CONNECTED;
            _timer = millis();
            return;
        }
    }
    _manualBaud = BAUD_RATES[0];
    _config.baudRate = _manualBaud;
    _config.rxPin = (_config.rxPin < 0) ? _pinA : _config.rxPin;
    _config.txPin = (_config.txPin < 0) ? _pinB : _config.txPin;
    applyBaudRate(_manualBaud);
    _state = STATE_CONNECTED;
    _timer = millis();
}

void UartScanner::baudDown() {
    _manualOverride = true;
    if (_config.baudRate == 0) _config.baudRate = BAUD_RATES[0];
    for (int i = 1; i < BAUD_COUNT; i++) {
        if (BAUD_RATES[i] == _config.baudRate || (_manualBaud > 0 && BAUD_RATES[i] == _manualBaud)) {
            _manualBaud = BAUD_RATES[i - 1];
            _config.baudRate = _manualBaud;
            _config.rxPin = (_config.rxPin < 0) ? _pinA : _config.rxPin;
            _config.txPin = (_config.txPin < 0) ? _pinB : _config.txPin;
            applyBaudRate(_manualBaud);
            _state = STATE_CONNECTED;
            _timer = millis();
            return;
        }
    }
    _manualBaud = BAUD_RATES[BAUD_COUNT - 1];
    _config.baudRate = _manualBaud;
    _config.rxPin = (_config.rxPin < 0) ? _pinA : _config.rxPin;
    _config.txPin = (_config.txPin < 0) ? _pinB : _config.txPin;
    applyBaudRate(_manualBaud);
    _state = STATE_CONNECTED;
    _timer = millis();
}

void UartScanner::swapPins() {
    if (_config.rxPin < 0 || _config.txPin < 0) return;
    _manualOverride = true;
    int tmp = _config.rxPin;
    _config.rxPin = _config.txPin;
    _config.txPin = tmp;

    stopSerial();
    delay(50);
    pinMode(_config.rxPin, INPUT);
    unsigned long baud = _config.baudRate;
    if (baud == 0) baud = BAUD_RATES[0];
    _serial.begin(baud, SERIAL_8N1, _config.rxPin, _config.txPin);
    _config.baudRate = baud;

    _state = STATE_CONNECTED;
    _timer = millis();
}

void UartScanner::clearManualOverride() {
    _manualOverride = false;
    _manualBaud = 0;
    stopSerial();
    begin();
}

// ── Pin detection ─────────────────────────────────────────────────

int UartScanner::findRxPin() {
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);
    delay(5);

    int fallsA = 0, fallsB = 0;
    uint8_t lastA = HIGH, lastB = HIGH;

    unsigned long start = millis();
    while (millis() - start < PIN_TIMEOUT_MS) {
        uint8_t curA = digitalRead(_pinA);
        uint8_t curB = digitalRead(_pinB);

        if (lastA == HIGH && curA == LOW) fallsA++;
        if (lastB == HIGH && curB == LOW) fallsB++;

        lastA = curA;
        lastB = curB;

        if (fallsA >= 4 && fallsA > fallsB) return _pinA;
        if (fallsB >= 4 && fallsB > fallsA) return _pinB;

        delayMicroseconds(100);
    }

    if (fallsA > fallsB && fallsA > 0) return _pinA;
    if (fallsB > fallsA && fallsB > 0) return _pinB;

    return -1;
}

// ── Main update ───────────────────────────────────────────────────

void UartScanner::update() {
    if (_manualOverride) {
        if (_state != STATE_CONNECTED) {
            _state = STATE_CONNECTED;
            _timer = millis();
        }
        return;
    }

    switch (_state) {
        case STATE_PIN_DETECT: {
            int rx = findRxPin();
            if (rx >= 0) {
                _config.rxPin = rx;
                _config.txPin = (rx == _pinA) ? _pinB : _pinA;
                _scanCycles = 0;
                startScan();
            }
            break;
        }

        case STATE_BAUD_SCAN: {
            if (_scanPos < SCAN_BUF_SIZE) {
                while (_serial.available() && _scanPos < SCAN_BUF_SIZE) {
                    _scanBuf[_scanPos++] = _serial.read();
                }
            } else {
                while (_serial.available()) _serial.read();
            }

            if (millis() - _timer >= SCAN_BAUD_MS) {
                int score = 0;
                for (int i = 0; i < _scanPos; i++) {
                    score += scoreByte(_scanBuf[i]);
                }

                if (score > _bestScore) {
                    _bestScore = score;
                    _bestBaudIdx = _baudIndex;
                }

                _baudIndex++;
                tryNextBaud();
            }
            break;
        }

        case STATE_CONNECTED: {
            if (_serial.available() > 0) {
                _timer = millis();
            } else if (_timer > 0 && millis() - _timer > IDLE_MS) {
                stopSerial();
                _config.rxPin = -1;
                _config.baudRate = 0;
                _scanCycles = 0;
                _state = STATE_PIN_DETECT;
                _timer = millis();
            } else if (_timer == 0) {
                _timer = millis();
            }
            break;
        }
    }
}
