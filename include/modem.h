#ifndef MODEM_H
#define MODEM_H

// IMPORTANT: Define modem model BEFORE including TinyGSM
#define TINY_GSM_MODEM_SIM7600

#include <Arduino.h>
#include <TinyGsmClient.h>

// Pin definitions for T-A7670G
#define MODEM_TX_PIN 26
#define MODEM_RX_PIN 27
#define MODEM_PWRKEY_PIN 4
#define MODEM_DTR_PIN 25
#define MODEM_RESET_PIN 5
#define MODEM_RESET_LEVEL HIGH
#define BOARD_POWERON_PIN 12
#define MODEM_POWERON_PULSE_WIDTH_MS 1000

// Serial for modem communication
#define SerialAT Serial1

// External modem instance
extern TinyGsm modem;
extern TinyGsmClient client;

// Modem functions
void setupModem();
bool waitForModemBoot(uint32_t timeout_ms = 30000);
bool initModem();
bool connectNetwork(const char *apn);
void disconnectNetwork();
void printModemInfo();
int getSignalQuality();
bool isNetworkConnected();
bool isGprsConnected();

#endif