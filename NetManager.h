#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

class NetManager {
public:
    enum Mode : uint8_t {
        MODE_NONE,
        MODE_AP,
        MODE_STA
    };

    bool beginAP(const char* ssid, const char* password, uint16_t port);
    bool beginSTA(const char* ssid, const char* password, uint16_t port);
    void stop();
    void update();
    void broadcast(const uint8_t* data, size_t len);
    void broadcast(const char* str);

    Mode getMode() const { return _mode; }
    uint16_t getPort() const { return _port; }
    int getClientCount() const { return _clientCount; }
    bool isRunning() const { return _running; }
    const char* getIPStr() const { return _ipStr; }
    bool isSTAConnected() const;

private:
    static constexpr int MAX_CLIENTS = 8;
    static constexpr unsigned long CLEANUP_INTERVAL = 5000;

    Mode _mode = MODE_NONE;
    WiFiServer* _server = nullptr;
    WiFiClient _clients[MAX_CLIENTS];
    int _clientCount = 0;
    bool _running = false;
    uint16_t _port = 8888;
    char _ipStr[16] = "";
    unsigned long _lastCleanup = 0;

    void cleanupDisconnected();
    void acceptNew();
};
