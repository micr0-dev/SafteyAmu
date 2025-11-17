#include "battery.h"

void initBattery()
{
    Serial.println("\n=== Initializing Battery Monitor ===");

    // Configure ADC for battery monitoring
    pinMode(BATTERY_ADC_PIN, INPUT);

    // Set ADC resolution (ESP32 default is 12-bit)
    analogReadResolution(12);

    // Set ADC attenuation for 0-3.3V range
    analogSetAttenuation(ADC_11db);

    Serial.println("✓ Battery monitoring initialized");

    // Initial reading
    float voltage = getBatteryVoltage();
    int percent = getBatteryPercent();

    Serial.print("Current Battery: ");
    Serial.print(voltage, 2);
    Serial.print("V (");
    Serial.print(percent);
    Serial.println("%)");
}

float getBatteryVoltage()
{
    // Read ADC value (0-4095 for 12-bit ADC)
    int adcValue = analogRead(BATTERY_ADC_PIN);

    // Convert to voltage
    // ESP32 ADC reference is 3.3V with 11db attenuation
    // For T-A7670G, there's typically a 2:1 voltage divider
    // So: actualVoltage = (adcValue / 4095.0) * 3.3 * 2.0
    float voltage = (adcValue / 4095.0) * 3.3 * 2.0;

    // If reading seems too high, adjust divider ratio
    // Some boards use different ratios
    if (voltage > 4.5)
    {
        voltage = (adcValue / 4095.0) * 3.3 * 1.5;
    }

    return voltage;
}

int getBatteryPercent()
{
    float voltage = getBatteryVoltage();

    // Li-ion battery voltage ranges (approximate)
    // 4.2V = 100%
    // 3.7V = 50%
    // 3.3V = 0%

    const float minVoltage = 3.3;
    const float maxVoltage = 4.2;

    if (voltage >= maxVoltage)
        return 100;
    if (voltage <= minVoltage)
        return 0;

    // Linear approximation
    int percent = (int)((voltage - minVoltage) / (maxVoltage - minVoltage) * 100);

    return constrain(percent, 0, 100);
}

String getBatteryStatus()
{
    float voltage = getBatteryVoltage();
    int percent = getBatteryPercent();

    String status = String(voltage, 2) + "V (" + String(percent) + "%)";

    if (percent > 80)
    {
        status += " - Good";
    }
    else if (percent > 50)
    {
        status += " - Fair";
    }
    else if (percent > 20)
    {
        status += " - Low";
    }
    else
    {
        status += " - Critical";
    }

    return status;
}