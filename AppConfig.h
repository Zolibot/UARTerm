#pragma once
#include <Arduino.h>

struct AppSettings {
    unsigned long baud = 0;
    int rxPin = -1;
    int txPin = -1;
    char apSsid[32] = "Cardputer_UART";
    char apPassword[64] = "";
    char staSsid[32] = "";
    char staPassword[64] = "";
    uint16_t tcpPort = 8888;
    bool sdLogging = false;
};
