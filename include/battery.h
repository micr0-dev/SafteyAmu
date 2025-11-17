#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

// Battery monitoring pin
#define BATTERY_ADC_PIN 35

// Battery functions
void initBattery();
float getBatteryVoltage();
int getBatteryPercent();
String getBatteryStatus();

#endif