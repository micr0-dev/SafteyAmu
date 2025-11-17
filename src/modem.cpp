#define TINY_GSM_MODEM_SIM7600 // Use SIM7600 definition for A7670

#include "modem.h"

// Create modem instance
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

void setupModem()
{
    Serial.println("\n=== Starting Modem Setup ===");

    // 1. Turn on board power
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
    Serial.println("✓ Board power ON");
    delay(500);

    // 2. Reset modem
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, LOW);
    delay(100);
    digitalWrite(MODEM_RESET_PIN, HIGH);
    delay(2600);
    digitalWrite(MODEM_RESET_PIN, LOW);
    Serial.println("✓ Modem reset complete");

    // 3. Set DTR pin
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);

    // 4. Power on modem with PWRKEY
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(100);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(MODEM_POWERON_PULSE_WIDTH_MS);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    Serial.println("✓ Modem power key pressed");

    // 5. Initialize serial communication with modem
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    Serial.println("✓ Serial communication initialized");
}

bool waitForModemBoot(uint32_t timeout_ms)
{
    Serial.println("\n=== Waiting for Modem to Boot ===");
    Serial.print("Probing modem");

    uint32_t startTime = millis();

    // Clear any pending data
    while (SerialAT.available())
    {
        SerialAT.read();
    }

    while (millis() - startTime < timeout_ms)
    {
        // Send AT command
        SerialAT.println("AT");

        // Wait for response
        uint32_t cmdStart = millis();
        String response = "";

        while (millis() - cmdStart < 1000)
        {
            if (SerialAT.available())
            {
                char c = SerialAT.read();
                response += c;

                // Check if we got OK
                if (response.indexOf("OK") >= 0)
                {
                    Serial.println(" ✓");
                    Serial.println("✓ Modem is ready!");

                    // Print boot time
                    uint32_t bootTime = (millis() - startTime) / 1000;
                    Serial.print("Boot time: ");
                    Serial.print(bootTime);
                    Serial.println(" seconds");

                    // Clear any remaining data
                    delay(100);
                    while (SerialAT.available())
                    {
                        SerialAT.read();
                    }

                    return true;
                }
            }
            delay(10);
        }

        Serial.print(".");
        delay(500);
    }

    Serial.println(" ✗");
    Serial.println("✗ Modem boot timeout!");
    return false;
}

bool initModem()
{
    Serial.println("\n=== Initializing Modem ===");

    // Test basic AT command
    Serial.print("Testing AT command... ");
    modem.sendAT("");
    if (modem.waitResponse(1000) != 1)
    {
        Serial.println("✗ FAILED");
        return false;
    }
    Serial.println("✓ OK");

    return true;
}

void printModemInfo()
{
    Serial.println("\n--- Modem Information ---");

    String modemInfo = modem.getModemInfo();
    Serial.print("Modem: ");
    Serial.println(modemInfo);

    // Check SIM card
    Serial.print("SIM Status: ");
    int simStatus = modem.getSimStatus();
    if (simStatus == 1)
    {
        Serial.println("✓ SIM Ready");

        String ccid = modem.getSimCCID();
        Serial.print("SIM CCID: ");
        Serial.println(ccid);

        String imei = modem.getIMEI();
        Serial.print("IMEI: ");
        Serial.println(imei);

        String imsi = modem.getIMSI();
        Serial.print("IMSI: ");
        Serial.println(imsi);

        String cop = modem.getOperator();
        Serial.print("Operator: ");
        Serial.println(cop);
    }
    else
    {
        Serial.print("✗ SIM Not Ready (Status: ");
        Serial.print(simStatus);
        Serial.println(")");
    }
}

bool connectNetwork(const char *apn)
{
    Serial.println("\n=== Connecting to Network ===");

    // Give radio extra time
    Serial.println("Initializing radio...");
    delay(3000);

    // Set network mode to automatic
    Serial.print("Setting network mode... ");
    modem.sendAT("+CNMP=2"); // 2 = Automatic
    modem.waitResponse(5000L);
    Serial.println("✓");

    // Wait for FULL network registration (not just searching)
    Serial.println("Waiting for network registration...");
    Serial.print("Status: ");

    uint32_t startTime = millis();
    int regStatus = 0;

    // Wait up to 90 seconds for FULL registration (status 1 or 5)
    while (millis() - startTime < 90000L)
    {
        modem.sendAT("+CREG?");
        String resp = "";
        if (modem.waitResponse(1000L, resp) == 1)
        {
            // Parse response: +CREG: 0,<status>
            int idx = resp.lastIndexOf(',');
            if (idx > 0)
            {
                regStatus = resp.substring(idx + 1).toInt();

                Serial.print(regStatus);
                Serial.print(" ");

                // Status 1 = registered home, 5 = registered roaming
                if (regStatus == 1 || regStatus == 5)
                {
                    Serial.println();
                    break;
                }
            }
        }
        delay(2000); // Check every 2 seconds
    }

    if (regStatus != 1 && regStatus != 5)
    {
        Serial.println("\n✗ Registration timeout");
        return false;
    }

    Serial.println("✓ Fully Registered!");

    // NOW check signal quality (after registration)
    delay(2000); // Give modem time to update signal reading

    int16_t signalQuality = modem.getSignalQuality();
    Serial.print("Signal Quality: ");
    Serial.print(signalQuality);
    if (signalQuality == 99)
    {
        Serial.println(" (Still reading... but registered!)");
    }
    else
    {
        Serial.print(" / 31");
        if (signalQuality < 10)
        {
            Serial.println(" - WEAK");
        }
        else if (signalQuality < 15)
        {
            Serial.println(" - Fair");
        }
        else
        {
            Serial.println(" - Good");
        }
    }

    // Check final registration status
    regStatus = modem.getRegistrationStatus();
    Serial.print("Registration Status: ");
    switch (regStatus)
    {
    case 0:
        Serial.println("Not registered");
        break;
    case 1:
        Serial.println("✓ Registered (Home)");
        break;
    case 2:
        Serial.println("Searching");
        break;
    case 3:
        Serial.println("Denied");
        break;
    case 5:
        Serial.println("✓ Registered (Roaming)");
        break;
    default:
        Serial.print("Unknown (");
        Serial.print(regStatus);
        Serial.println(")");
        break;
    }

    // Connect to GPRS
    Serial.print("Connecting to APN: ");
    Serial.print(apn);
    Serial.print("... ");
    if (!modem.gprsConnect(apn))
    {
        Serial.println("✗ Failed");
        return false;
    }
    Serial.println("✓ Connected");

    if (modem.isGprsConnected())
    {
        Serial.println("✓ GPRS Connected");

        IPAddress local = modem.localIP();
        Serial.print("Local IP: ");
        Serial.println(local);

        return true;
    }

    return false;
}

void disconnectNetwork()
{
    Serial.println("Disconnecting from network...");
    modem.gprsDisconnect();
    Serial.println("✓ Disconnected");
}

int getSignalQuality()
{
    return modem.getSignalQuality();
}

bool isNetworkConnected()
{
    return modem.isNetworkConnected();
}

bool isGprsConnected()
{
    return modem.isGprsConnected();
}