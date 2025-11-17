#include "gps.h"

// Global GPS data
GPSData currentGPS = {false, 0.0, 0.0, 0.0, 0.0, 0, ""};

bool initGPS()
{
    Serial.println("\n=== Initializing GPS ===");

    // Power on GPS
    Serial.print("Powering on GPS... ");
    modem.sendAT("+CGNSPWR=1");
    if (modem.waitResponse(10000L) != 1)
    {
        Serial.println("✗ Failed");
        Serial.println("Note: A7670G models may not have built-in GPS");
        return false;
    }
    Serial.println("✓ GPS powered on");

    delay(1000);

    Serial.println("✓ GPS initialization complete");
    Serial.println("Note: GPS may take 30-60 seconds for first fix outdoors");

    return true;
}

bool updateGPSData()
{
    // Send command to get GPS info
    modem.sendAT("+CGNSSINFO");

    String response = "";
    if (modem.waitResponse(1000L, response) != 1)
    {
        currentGPS.isValid = false;
        return false;
    }

    // Parse response
    // Format: +CGNSSINFO: <mode>,<GPS fix status>,<UTC time>,<latitude>,<longitude>,<altitude>,<speed>,<course>,<PDOP>,<HDOP>,<VDOP>,<satellites in view>,<HPA>,<VPA>

    int firstComma = response.indexOf(',');
    if (firstComma < 0)
    {
        currentGPS.isValid = false;
        return false;
    }

    // Find GPS fix status (second field)
    int secondComma = response.indexOf(',', firstComma + 1);
    if (secondComma < 0)
    {
        currentGPS.isValid = false;
        return false;
    }

    String fixStatus = response.substring(firstComma + 1, secondComma);
    fixStatus.trim();

    // Check if we have a valid fix (not empty and not 0)
    if (fixStatus.length() == 0 || fixStatus == "0" || fixStatus == ",")
    {
        currentGPS.isValid = false;
        return false;
    }

    // Parse the fields
    int fieldStart = secondComma + 1;
    int fieldEnd;

    // UTC time (field 3)
    fieldEnd = response.indexOf(',', fieldStart);
    currentGPS.timestamp = response.substring(fieldStart, fieldEnd);
    fieldStart = fieldEnd + 1;

    // Latitude (field 4)
    fieldEnd = response.indexOf(',', fieldStart);
    String latStr = response.substring(fieldStart, fieldEnd);
    currentGPS.latitude = latStr.toFloat();
    fieldStart = fieldEnd + 1;

    // Longitude (field 5)
    fieldEnd = response.indexOf(',', fieldStart);
    String lonStr = response.substring(fieldStart, fieldEnd);
    currentGPS.longitude = lonStr.toFloat();
    fieldStart = fieldEnd + 1;

    // Altitude (field 6)
    fieldEnd = response.indexOf(',', fieldStart);
    String altStr = response.substring(fieldStart, fieldEnd);
    currentGPS.altitude = altStr.toFloat();
    fieldStart = fieldEnd + 1;

    // Speed (field 7)
    fieldEnd = response.indexOf(',', fieldStart);
    String speedStr = response.substring(fieldStart, fieldEnd);
    currentGPS.speed = speedStr.toFloat();
    fieldStart = fieldEnd + 1;

    // Skip course (field 8)
    fieldEnd = response.indexOf(',', fieldStart);
    fieldStart = fieldEnd + 1;

    // Skip PDOP, HDOP, VDOP (fields 9, 10, 11)
    for (int i = 0; i < 3; i++)
    {
        fieldEnd = response.indexOf(',', fieldStart);
        if (fieldEnd < 0)
            break;
        fieldStart = fieldEnd + 1;
    }

    // Satellites (field 12)
    fieldEnd = response.indexOf(',', fieldStart);
    if (fieldEnd > 0)
    {
        String satStr = response.substring(fieldStart, fieldEnd);
        currentGPS.satellites = satStr.toInt();
    }

    currentGPS.isValid = true;
    return true;
}

GPSData getGPSData()
{
    return currentGPS;
}

String getGPSString()
{
    if (!currentGPS.isValid)
    {
        return "No GPS fix";
    }

    String gpsStr = "Lat: ";
    gpsStr += String(currentGPS.latitude, 6);
    gpsStr += ", Lon: ";
    gpsStr += String(currentGPS.longitude, 6);
    gpsStr += ", Alt: ";
    gpsStr += String(currentGPS.altitude, 1);
    gpsStr += "m, Sats: ";
    gpsStr += String(currentGPS.satellites);

    return gpsStr;
}