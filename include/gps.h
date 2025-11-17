#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include "modem.h"

// GPS data structure
struct GPSData
{
    bool isValid;
    float latitude;
    float longitude;
    float altitude;
    float speed;
    int satellites;
    String timestamp;
};

// GPS functions
bool initGPS();
bool updateGPSData();
GPSData getGPSData();
String getGPSString();

#endif