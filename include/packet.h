#ifndef PACKET_H
#define PACKET_H

#include <Arduino.h>

// Packet version
#define PACKET_VERSION 1

// Packet structure (20 bytes total)
struct __attribute__((packed)) SafetyPacket
{
    // Byte 0: Flags
    uint8_t flags;

    // Bytes 1-2: Sequence number
    uint16_t sequence;

    // Bytes 3-6: Timestamp (seconds since boot)
    uint32_t timestamp;

    // Bytes 7-10: Latitude (degrees × 10^7)
    int32_t latitude;

    // Bytes 11-14: Longitude (degrees × 10^7)
    int32_t longitude;

    // Bytes 15-16: Altitude (meters)
    int16_t altitude;

    // Byte 17: Signal strength
    uint8_t signal;

    // Byte 18: Battery percentage
    uint8_t battery;

    // Byte 19: CRC8 checksum
    uint8_t crc;
};

// Flag bit positions
#define FLAG_VERSION_SHIFT 5 // Bits 5-7 (3 bits)
#define FLAG_EMERGENCY 4     // Bit 4
#define FLAG_GPS_VALID 3     // Bit 3
#define FLAG_MOVING 2        // Bit 2
#define FLAG_LOW_BATTERY 1   // Bit 1
#define FLAG_RESERVED 0      // Bit 0

// Helper macros
#define SET_FLAG(flags, bit) ((flags) |= (1 << (bit)))
#define CLEAR_FLAG(flags, bit) ((flags) &= ~(1 << (bit)))
#define GET_FLAG(flags, bit) (((flags) >> (bit)) & 1)
#define SET_VERSION(flags, ver) ((flags) = ((flags) & 0x1F) | ((ver) << FLAG_VERSION_SHIFT))
#define GET_VERSION(flags) (((flags) >> FLAG_VERSION_SHIFT) & 0x07)

// CRC8 calculation
uint8_t calculateCRC8(const uint8_t *data, size_t length);

// Packet creation
void createSafetyPacket(SafetyPacket *packet, uint16_t seq, bool emergency);

#endif