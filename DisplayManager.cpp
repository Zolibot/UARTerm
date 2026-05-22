#include "DisplayManager.h"
#include <M5Cardputer.h>

void DisplayManager::init() {
    auto& gfx = M5Cardputer.Display;
    gfx.setRotation(1);
    gfx.setTextSize(1);
    gfx.setTextWrap(false);
    gfx.fillScreen(TFT_BLACK);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);

    int spriteW = gfx.width();

    _barSprite = new LGFX_Sprite(&gfx);
    _barSprite->createSprite(spriteW, BAR_H);
    _barSprite->setTextSize(1);
    _barSprite->setTextWrap(false);
    _barSprite->setTextColor(TFT_CYAN, TFT_NAVY);

    _bottomSprite = new LGFX_Sprite(&gfx);
    _bottomSprite->createSprite(spriteW, BAR_H);
    _bottomSprite->setTextSize(1);
    _bottomSprite->setTextWrap(false);
    _bottomSprite->setTextColor(TFT_CYAN, TFT_BLACK);

    _sprite = new LGFX_Sprite(&gfx);
    _spriteReady = _sprite->createSprite(spriteW, DATA_AREA_H);
    if (_spriteReady) {
        _sprite->setTextSize(1);
        _sprite->setTextWrap(false);
        _sprite->fillSprite(TFT_BLACK);
        _sprite->setTextColor(TFT_WHITE, TFT_BLACK);
        _sprite->pushSprite(0, DATA_AREA_Y);
    }

    clearLines();
    _dirty = true;
}

void DisplayManager::clearLines() {
    for (int i = 0; i < MAX_LINES; i++) {
        memset(_lines[i], ' ', LINE_LEN - 1);
        _lines[i][LINE_LEN - 1] = '\0';
    }
    _head = 0;
    _col = 0;
    _hexCount = 0;
}

void DisplayManager::newline() {
    _lines[_head][LINE_LEN - 1] = '\0';
    if (_head >= MAX_LINES - 1) {
        for (int i = 0; i < MAX_LINES - 1; i++) {
            memcpy(_lines[i], _lines[i + 1], LINE_LEN);
        }
    } else {
        _head++;
    }
    _col = 0;
    memset(_lines[_head], ' ', LINE_LEN - 1);
    _lines[_head][LINE_LEN - 1] = '\0';
}

void DisplayManager::addChar(char c) {
    uint8_t b = (uint8_t)c;

    if (_hexMode) {
        if (_hexCount < 13) {
            _hexBuf[_hexCount++] = b;
        }
        if (_hexCount >= 13) {
            char buf[LINE_LEN];
            int pos = 0;
            for (int i = 0; i < _hexCount && pos < LINE_LEN - 4; i++) {
                pos += snprintf(buf + pos, LINE_LEN - pos, "%02X ", _hexBuf[i]);
            }
            buf[pos] = '\0';
            strncpy(_lines[_head], buf, LINE_LEN - 1);
            _lines[_head][LINE_LEN - 1] = '\0';
            _col = pos;
            newline();
            _hexCount = 0;
        }
        _dirty = true;
        return;
    }

    if (b == '\n') {
        newline();
    } else if (b == '\r') {
        _col = 0;
    } else {
        if (_col >= LINE_LEN - 2) {
            newline();
        }
        _lines[_head][_col++] = (b >= 0x20 && b <= 0x7E) ? b : '.';
    }
    _dirty = true;
}

void DisplayManager::showMessage(const char* msg) {
    clearLines();
    strncpy(_lines[_head], msg, LINE_LEN - 1);
    _lines[_head][LINE_LEN - 1] = '\0';
    _col = strnlen(msg, LINE_LEN - 1);
    if (_col >= LINE_LEN - 1) _col = LINE_LEN - 2;
    _dirty = true;
}

void DisplayManager::renderTopBar(const UartScanner& scanner) {
    if (!_barSprite) return;

    char buf[LINE_LEN];
    buf[0] = '\0';

    auto& cfg = scanner.getConfig();
    auto state = scanner.getState();

    // Always show UART state
    if (scanner.isConnected()) {
        snprintf(buf, sizeof(buf), "%sTX:G%d RX:G%d %lu",
            scanner.isManualOverride() ? "MAN " : "",
            cfg.txPin, cfg.rxPin, cfg.baudRate);
    } else {
        switch (state) {
            case UartScanner::STATE_PIN_DETECT:
                snprintf(buf, sizeof(buf), "[ SCAN PINS G1/G2 ]"); break;
            case UartScanner::STATE_BAUD_SCAN:
                snprintf(buf, sizeof(buf), "[ SCAN %lu ]", scanner.getCurrentBaudTest()); break;
            default:
                snprintf(buf, sizeof(buf), "[ WAIT ]"); break;
        }
    }

    _barSprite->fillSprite(TFT_NAVY);
    _barSprite->setCursor(2, 1);
    _barSprite->print(buf);

    int rightX = _barSprite->width() - 2;

    // SD logging indicator with text
    if (_sdLogging) {
        _barSprite->setCursor(rightX - 36, 1);
        _barSprite->setTextColor(TFT_ORANGE, TFT_NAVY);
        _barSprite->print("LOG");
        _barSprite->setTextColor(TFT_CYAN, TFT_NAVY);
        uint16_t sdColor = ((millis() / 500) & 1) ? TFT_ORANGE : TFT_NAVY;
        _barSprite->fillCircle(rightX - 6, 5, 3, sdColor);
        rightX -= 42;
    }

    // Activity LED
    if (scanner.isConnected() || (_dispMode == DISP_NETWORK && _netIP[0])) {
        uint16_t ledColor = ((millis() / 500) & 1) ? TFT_GREEN : TFT_NAVY;
        _barSprite->fillCircle(rightX, 5, 3, ledColor);
    }

    _barSprite->pushSprite(0, STATUS_BAR_Y);
}

void DisplayManager::renderDataArea() {
    if (!_spriteReady || !_sprite) return;

    int maxLines = DATA_AREA_H / CHAR_H;

    _sprite->fillSprite(TFT_BLACK);

    int validLines = _head + 1;
    int startLine = (validLines > maxLines) ? (validLines - maxLines) : 0;
    int count = (validLines > maxLines) ? maxLines : validLines;

    for (int i = 0; i < count; i++) {
        int idx = startLine + i;
        int screenY = i * CHAR_H;

        int len = LINE_LEN - 1;
        while (len > 0 && (_lines[idx][len - 1] == ' ' || _lines[idx][len - 1] == '\0')) {
            len--;
        }

        if (len > 0) {
            for (int j = 0; j < len && j * CHAR_W < _sprite->width(); j++) {
                char c = _lines[idx][j];
                if (c == '\0') break;
                if (c < 0x20 || c > 0x7E) c = '.';
                _sprite->drawChar(c, j * CHAR_W, screenY);
            }
        }
    }

    _sprite->pushSprite(0, DATA_AREA_Y);
}

void DisplayManager::renderBottomBar() {
    if (!_bottomSprite) return;

    _bottomSprite->fillSprite(TFT_BLACK);

    if (_bottomText[0]) {
        _bottomSprite->setCursor(2, 1);
        _bottomSprite->setTextColor(TFT_CYAN, TFT_BLACK);
        _bottomSprite->print(_bottomText);
    }

    _bottomSprite->pushSprite(0, BOTTOM_BAR_Y);
}

void DisplayManager::capture(uint16_t* buf) const {
    int w = M5Cardputer.Display.width();
    if (_barSprite) _barSprite->readRect(0, 0, w, BAR_H, buf);
    if (_sprite && _spriteReady) _sprite->readRect(0, 0, w, DATA_AREA_H, buf + w * BAR_H);
    if (_bottomSprite) _bottomSprite->readRect(0, 0, w, BAR_H, buf + w * (BAR_H + DATA_AREA_H));
}

void DisplayManager::render(const UartScanner& scanner) {
    renderTopBar(scanner);
    renderBottomBar();

    if (_dirty) {
        _dirty = false;
        renderDataArea();
    }
}
