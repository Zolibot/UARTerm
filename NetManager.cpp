#include "NetManager.h"
#include <sys/socket.h>
#include <unistd.h>

bool NetManager::beginAP(const char* ssid, const char* password, uint16_t port) {
    stop();

    _port = port;
    _mode = MODE_AP;

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, password)) {
        _mode = MODE_NONE;
        return false;
    }

    IPAddress ip = WiFi.softAPIP();
    snprintf(_ipStr, sizeof(_ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    _server = new WiFiServer(_port);
    _server->begin();

    _running = true;
    _clientCount = 0;
    _lastCleanup = millis();
    return true;
}

bool NetManager::beginSTA(const char* ssid, const char* password, uint16_t port) {
    stop();

    _port = port;
    _mode = MODE_STA;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long timeout = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        _mode = MODE_NONE;
        return false;
    }

    IPAddress ip = WiFi.localIP();
    snprintf(_ipStr, sizeof(_ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    _server = new WiFiServer(_port);
    _server->begin();

    _running = true;
    _clientCount = 0;
    _lastCleanup = millis();
    return true;
}

void NetManager::stop() {
    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }

    for (int i = 0; i < _clientCount; i++) {
        _clients[i].stop();
    }
    _clientCount = 0;

    if (_mode != MODE_NONE) {
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
    }

    _mode = MODE_NONE;
    _running = false;
    _ipStr[0] = '\0';
}

void NetManager::update() {
    if (!_running || !_server) return;

    acceptNew();

    unsigned long now = millis();
    if (now - _lastCleanup >= CLEANUP_INTERVAL) {
        _lastCleanup = now;
        cleanupDisconnected();
    }
}

void NetManager::acceptNew() {
    WiFiClient newClient = _server->available();
    if (!newClient) return;

    if (_clientCount >= MAX_CLIENTS) {
        newClient.stop();
        return;
    }

    newClient.setNoDelay(true);
    _clients[_clientCount++] = newClient;
}

void NetManager::cleanupDisconnected() {
    int writeIdx = 0;
    for (int i = 0; i < _clientCount; i++) {
        if (_clients[i] && _clients[i].connected()) {
            if (writeIdx != i) {
                _clients[writeIdx] = _clients[i];
            }
            writeIdx++;
        } else {
            _clients[i].stop();
        }
    }
    _clientCount = writeIdx;
}

void NetManager::broadcast(const uint8_t* data, size_t len) {
    if (!_running || _clientCount == 0 || len == 0) return;

    for (int i = 0; i < _clientCount; i++) {
        if (!_clients[i]) continue;

        int fd = _clients[i].fd();
        if (fd < 0) continue;

        fd_set wfds;
        struct timeval tv = {0, 0};
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);

        if (select(fd + 1, NULL, &wfds, NULL, &tv) <= 0) continue;

        size_t left = len;
        const uint8_t* ptr = data;

        while (left > 0) {
            int ret = ::write(fd, ptr, left);
            if (ret > 0) {
                ptr += ret;
                left -= ret;
            } else if (ret < 0) {
                int err = errno;
                if (err != EAGAIN && err != EWOULDBLOCK) {
                    _clients[i].stop();
                }
                break;
            }
        }
    }
}

void NetManager::broadcast(const char* str) {
    broadcast((const uint8_t*)str, strlen(str));
}

bool NetManager::isSTAConnected() const {
    if (_mode != MODE_STA) return false;
    return WiFi.status() == WL_CONNECTED;
}
