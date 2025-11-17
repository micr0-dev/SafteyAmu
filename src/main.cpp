#include <Arduino.h>
#include <SSLClient.h>
#include "modem.h"
#include "gps.h"
#include "battery.h"
#include "packet.h"

// Configuration
const char *apn = "hologram";
const char *server = "tx.micr0.dev";
const int port = 8443; // HTTPS port
const char *endpoint = "/api/data";

// Root CA Certificate for your server (self-signed for testing)
// Generate with: openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes
const char *root_ca = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDZTCCAk2gAwIBAgIUE5FTj7WOVOJOBogdfo8pc3osslowDQYJKoZIhvcNAQEL
BQAwQjELMAkGA1UEBhMCWFgxFTATBgNVBAcMDERlZmF1bHQgQ2l0eTEcMBoGA1UE
CgwTRGVmYXVsdCBDb21wYW55IEx0ZDAeFw0yNTExMTcxNzE1MTNaFw0yNjExMTcx
NzE1MTNaMEIxCzAJBgNVBAYTAlhYMRUwEwYDVQQHDAxEZWZhdWx0IENpdHkxHDAa
BgNVBAoME0RlZmF1bHQgQ29tcGFueSBMdGQwggEiMA0GCSqGSIb3DQEBAQUAA4IB
DwAwggEKAoIBAQC1N6kzkv1BXqjtFZ/KutsuiT4hSmdMeFnFkwoW+IESd1QSSSZU
JgwdpRNhwve7H0EvHF9gWPpu86iiu5BuWRB0zeZgG8nob4wJpYdEOlGxwwF6tN91
iu95cMJfHrOtGYb+dQqo2q+f7uAedyz0cP/F0kOnhpI/AkVwcg1JDrAatWJMtBZA
7GWpFZb8S9ln3/XooZrHIKLiABoHYNrEMjjoiTMZ6HK7K2M5YGKs4303ssNDlmgn
9tpo0z5RBYV/e0fVLOwvNnPFzr9RN7KHoffaBoHIpsUe4GK+JMRjJGdHIgXV2ZXZ
9XTR70H3FhuITCR3RmLduBpMKmE5K7rxrauTAgMBAAGjUzBRMB0GA1UdDgQWBBRV
20FhLfIDVGrB/+abMPomb4JbQTAfBgNVHSMEGDAWgBRV20FhLfIDVGrB/+abMPom
b4JbQTAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBZDYWn1B4W
h2Ab7SPQkaBLd/I8RUuaNscqGpDIyqPRs3Csew7LrThwHhrC1vL5FK9mOJQJuQg8
GJKAsHzemki3vTCHYYc+BsSqETQ7O4/ibjbJPtjeW4+8JqEKl+2M8MquyaV0iCNo
TJr8dlN/1GSTp0P9wsbTQ3EROrSqBeY/ifdjPB4LfGOYNOj2cuA+WwafSrP9COj0
S1ugQnk+1E7pZ2Z8zOceTuOIwpdnPwuAnVHb9s8YphfUbz4vI1Y2EWRbrrq9toPm
YC2f27mtof6HrTtuHD1kxUvAUMo0kT6s65UZsC2dEuQ22+hjbgSWksRptgD7B7D9
jqXGrcOHN+r8
-----END CERTIFICATE-----
)EOF";

// Packet sequence number
uint16_t packetSequence = 0;

// SSL Client wrapping the TinyGSM client
SSLClient secureClient(&client);

// Connection state
bool connectionEstablished = false;

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║  LILYGO T-A7670G Safety Device        ║");
    Serial.println("║  HTTPS Persistent Connection          ║");
    Serial.println("╚═══════════════════════════════════════╝");

    setupModem();

    if (!waitForModemBoot(30000))
    {
        Serial.println("\n✗ MODEM FAILED TO BOOT!");
        while (1)
        {
            delay(1000);
        }
    }

    if (!initModem())
    {
        Serial.println("✗ Failed to initialize modem!");
        while (1)
        {
            delay(1000);
        }
    }

    printModemInfo();

    if (!connectNetwork(apn))
    {
        Serial.println("✗ Failed to connect to network!");
        while (1)
        {
            delay(1000);
        }
    }

    initBattery();

    if (!initGPS())
    {
        Serial.println("⚠ Warning: GPS initialization failed");
        Serial.println("Continuing without GPS...");
    }

    // Configure SSL client
    Serial.println("\n=== Configuring SSL/TLS ===");
    secureClient.setCACert(root_ca);
    secureClient.setHandshakeTimeout(60000);
    secureClient.setTimeout(30000);
    secureClient.setInsecure(); // For testing with self-signed cert
    Serial.println("✓ SSL configured");

    // Establish persistent HTTPS connection
    Serial.println("\n=== Establishing Persistent Connection ===");
    Serial.print("Connecting to ");
    Serial.print(server);
    Serial.print(":");
    Serial.print(port);
    Serial.print("... ");

    if (secureClient.connect(server, port))
    {
        Serial.println("✓ HTTPS connection established!");
        connectionEstablished = true;

        Serial.println("✓ TLS handshake complete");
        Serial.println("✓ Connection will persist for all transmissions");
    }
    else
    {
        Serial.println("✗ Failed to establish HTTPS connection");
        Serial.println("Will retry in loop...");
    }

    Serial.println("\n✓ Setup complete! Transmitting safety packets.\n");
}

bool sendBinaryPacketPersistent(const SafetyPacket *packet)
{
    // Check if connection is still alive
    if (!connectionEstablished || !secureClient.connected())
    {
        Serial.println("⚠ Connection lost, reconnecting...");

        if (secureClient.connect(server, port))
        {
            Serial.println("✓ Reconnected");
            connectionEstablished = true;
        }
        else
        {
            Serial.println("✗ Reconnection failed");
            connectionEstablished = false;
            return false;
        }
    }

    Serial.println("\n=== Sending Binary Packet (HTTPS) ===");

    // Send HTTP POST header
    secureClient.print("POST ");
    secureClient.print(endpoint);
    secureClient.println(" HTTP/1.1");
    secureClient.print("Host: ");
    secureClient.println(server);
    secureClient.println("Content-Type: application/octet-stream");
    secureClient.print("Content-Length: ");
    secureClient.println(sizeof(SafetyPacket));
    secureClient.println("Connection: keep-alive"); // Keep connection alive!
    secureClient.println();

    // Send binary packet (encrypted by SSL layer)
    secureClient.write((uint8_t *)packet, sizeof(SafetyPacket));
    secureClient.flush();

    Serial.print("✓ Sent ");
    Serial.print(sizeof(SafetyPacket));
    Serial.println(" bytes (encrypted)");

    // Wait for response
    uint32_t timeout = millis();
    bool headersDone = false;

    while (secureClient.connected() && millis() - timeout < 5000L)
    {
        while (secureClient.available())
        {
            String line = secureClient.readStringUntil('\n');

            if (!headersDone)
            {
                Serial.println(line);
                if (line == "\r" || line.length() == 0)
                {
                    headersDone = true;
                }
            }

            timeout = millis();
        }

        if (headersDone)
            break;
        delay(10);
    }

    Serial.println("✓ Packet sent (connection kept alive)\n");

    return true;
}

void printPacketInfo(const SafetyPacket *packet)
{
    Serial.println("📦 Packet Info:");
    Serial.print("  Sequence: ");
    Serial.println(packet->sequence);
    Serial.print("  Emergency: ");
    Serial.println(GET_FLAG(packet->flags, FLAG_EMERGENCY) ? "🚨 YES" : "no");

    if (GET_FLAG(packet->flags, FLAG_GPS_VALID))
    {
        Serial.print("  📍 GPS: ");
        Serial.print(packet->latitude / 10000000.0, 7);
        Serial.print(", ");
        Serial.println(packet->longitude / 10000000.0, 7);
    }

    Serial.print("  📶 Signal: ");
    Serial.println(packet->signal);
    Serial.print("  🔋 Battery: ");
    Serial.print(packet->battery);
    Serial.println("%");
}

void loop()
{
    static unsigned long lastSend = 0;

    if (millis() - lastSend > 30000) // Every 30 seconds
    {
        lastSend = millis();

        // Update GPS
        updateGPSData();

        // Status check
        Serial.print("Status - Signal: ");
        Serial.print(getSignalQuality());
        Serial.print(" | GPRS: ");
        Serial.print(isGprsConnected() ? "✓" : "✗");
        Serial.print(" | HTTPS: ");
        Serial.print(connectionEstablished ? "✓" : "✗");
        Serial.print(" | Battery: ");
        Serial.print(getBatteryPercent());
        Serial.println("%");

        // Create safety packet
        SafetyPacket packet;
        bool emergency = false; // TODO: Add emergency trigger

        createSafetyPacket(&packet, packetSequence++, emergency);

        printPacketInfo(&packet);

        // Send packet over persistent HTTPS connection
        if (sendBinaryPacketPersistent(&packet))
        {
            Serial.println("✓ Safety packet transmitted securely!");
        }
        else
        {
            Serial.println("✗ Failed to send packet");
        }
    }

    delay(100);
}