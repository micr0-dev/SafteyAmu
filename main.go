package main

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"time"
)

// Flag bit positions (must match ESP32)
const (
	FlagVersionShift = 5
	FlagEmergency    = 4
	FlagGPSValid     = 3
	FlagMoving       = 2
	FlagLowBattery   = 1
	FlagReserved     = 0
)

// SafetyPacket matches the C struct (20 bytes)
type SafetyPacket struct {
	Flags     uint8
	Sequence  uint16
	Timestamp uint32
	Latitude  int32
	Longitude int32
	Altitude  int16
	Signal    uint8
	Battery   uint8
	CRC       uint8
}

// DecodedPacket for JSON output
type DecodedPacket struct {
	Version        uint8   `json:"version"`
	Sequence       uint16  `json:"sequence"`
	Timestamp      uint32  `json:"timestamp"`
	Emergency      bool    `json:"emergency"`
	GPS            GPSData `json:"gps"`
	SignalStrength uint8   `json:"signal_strength"`
	BatteryPercent uint8   `json:"battery_percent"`
	LowBattery     bool    `json:"low_battery"`
	CRC            CRCInfo `json:"crc"`
}

type GPSData struct {
	Valid     bool     `json:"valid"`
	Latitude  *float64 `json:"latitude"`
	Longitude *float64 `json:"longitude"`
	Altitude  *int16   `json:"altitude"`
	Moving    bool     `json:"moving"`
}

type CRCInfo struct {
	Received   string `json:"received"`
	Calculated string `json:"calculated"`
	Valid      bool   `json:"valid"`
}

// CRC8 lookup table (CRC-8-CCITT)
var crc8Table = [256]uint8{
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
	0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
	0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
	0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
	0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
	0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
	0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
	0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
	0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
	0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
	0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
	0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
	0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
	0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
	0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3,
}

func calculateCRC8(data []byte) uint8 {
	crc := uint8(0x00)
	for _, b := range data {
		crc = crc8Table[crc^b]
	}
	return crc
}

func getFlag(flags uint8, bit uint) bool {
	return (flags>>bit)&1 == 1
}

func getVersion(flags uint8) uint8 {
	return (flags >> FlagVersionShift) & 0x07
}

func decodeSafetyPacket(data []byte) (*DecodedPacket, error) {
	if len(data) != 20 {
		return nil, fmt.Errorf("invalid packet size: %d (expected 20)", len(data))
	}

	// Parse binary data (little-endian)
	reader := bytes.NewReader(data)
	var packet SafetyPacket

	if err := binary.Read(reader, binary.LittleEndian, &packet); err != nil {
		return nil, fmt.Errorf("failed to parse packet: %v", err)
	}

	// Verify CRC
	crcCalculated := calculateCRC8(data[:19])
	crcValid := crcCalculated == packet.CRC

	// Extract flags
	version := getVersion(packet.Flags)
	emergency := getFlag(packet.Flags, FlagEmergency)
	gpsValid := getFlag(packet.Flags, FlagGPSValid)
	moving := getFlag(packet.Flags, FlagMoving)
	lowBattery := getFlag(packet.Flags, FlagLowBattery)

	// Convert GPS coordinates
	var latitude, longitude *float64
	var altitude *int16

	if gpsValid {
		lat := float64(packet.Latitude) / 10000000.0
		lon := float64(packet.Longitude) / 10000000.0
		latitude = &lat
		longitude = &lon
		altitude = &packet.Altitude
	}

	decoded := &DecodedPacket{
		Version:        version,
		Sequence:       packet.Sequence,
		Timestamp:      packet.Timestamp,
		Emergency:      emergency,
		SignalStrength: packet.Signal,
		BatteryPercent: packet.Battery,
		LowBattery:     lowBattery,
		GPS: GPSData{
			Valid:     gpsValid,
			Latitude:  latitude,
			Longitude: longitude,
			Altitude:  altitude,
			Moving:    moving,
		},
		CRC: CRCInfo{
			Received:   fmt.Sprintf("0x%02X", packet.CRC),
			Calculated: fmt.Sprintf("0x%02X", crcCalculated),
			Valid:      crcValid,
		},
	}

	return decoded, nil
}

func handleData(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	timestamp := time.Now().Format("2006-01-02 15:04:05")

	fmt.Println("\n" + strings.Repeat("=", 60))
	fmt.Printf("📨 Received POST request at %s\n", timestamp)
	fmt.Println(strings.Repeat("=", 60))

	// Show if it came through Apache (HTTPS)
	if r.Header.Get("X-Forwarded-Proto") == "https" {
		fmt.Println("🔒 Connection: HTTPS (via Apache reverse proxy)")
	} else {
		fmt.Println("⚠️  Connection: HTTP (direct)")
	}

	// Read binary data
	body, err := io.ReadAll(r.Body)
	if err != nil {
		log.Printf("Error reading body: %v", err)
		http.Error(w, "Failed to read body", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	// Check content type
	contentType := r.Header.Get("Content-Type")
	if contentType != "application/octet-stream" {
		log.Printf("Unexpected content type: %s", contentType)
		http.Error(w, "Expected application/octet-stream", http.StatusBadRequest)
		return
	}

	fmt.Printf("\n📦 Binary Packet (%d bytes)\n", len(body))
	fmt.Printf("Raw hex: %s\n", hex.EncodeToString(body))

	// Decode packet
	packet, err := decodeSafetyPacket(body)
	if err != nil {
		log.Printf("Decode error: %v", err)
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Print decoded packet
	fmt.Println("\n✅ Decoded Packet:")
	fmt.Printf("  Version: %d\n", packet.Version)
	fmt.Printf("  Sequence: #%d\n", packet.Sequence)
	fmt.Printf("  Timestamp: %ds\n", packet.Timestamp)

	if packet.Emergency {
		fmt.Println("  🚨 EMERGENCY ALERT! 🚨")
	}

	if packet.GPS.Valid {
		fmt.Println("  📍 Location:")
		fmt.Printf("    Latitude:  %.7f°\n", *packet.GPS.Latitude)
		fmt.Printf("    Longitude: %.7f°\n", *packet.GPS.Longitude)
		fmt.Printf("    Altitude:  %dm\n", *packet.GPS.Altitude)
		fmt.Printf("    Moving:    %v\n", packet.GPS.Moving)
	} else {
		fmt.Println("  📍 GPS: No fix")
	}

	fmt.Printf("  📶 Signal: %d/31\n", packet.SignalStrength)
	fmt.Printf("  🔋 Battery: %d%%", packet.BatteryPercent)
	if packet.LowBattery {
		fmt.Println(" ⚠️ LOW!")
	} else {
		fmt.Println()
	}

	fmt.Printf("  ✓ CRC: %s ", packet.CRC.Received)
	if packet.CRC.Valid {
		fmt.Println("(✓ valid)")
	} else {
		fmt.Printf("(✗ INVALID! Expected %s)\n", packet.CRC.Calculated)
	}

	fmt.Println(strings.Repeat("=", 60) + "\n")

	// Send response
	response := map[string]interface{}{
		"status":    "success",
		"message":   "Binary packet received and decoded",
		"packet":    packet,
		"timestamp": timestamp,
		"secure":    r.Header.Get("X-Forwarded-Proto") == "https",
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(response)
}

func handleRoot(w http.ResponseWriter, r *http.Request) {
	html := `
<!DOCTYPE html>
<html>
<head>
    <title>Safety Device Receiver</title>
</head>
<body>
    <h1>🔒 Safety Device Receiver</h1>
    <p>Go backend running!</p>
    <p>Accepts binary packets at /api/data</p>
    <p>✓ Designed for Apache reverse proxy</p>
</body>
</html>
`
	w.Header().Set("Content-Type", "text/html")
	w.Write([]byte(html))
}

func main() {
	fmt.Println("\n" + strings.Repeat("=", 60))
	fmt.Println("🚀 Starting Safety Device Receiver (Go)")
	fmt.Println(strings.Repeat("=", 60))
	fmt.Println("📡 Listening on http://localhost:8082")
	fmt.Println("📍 Endpoint: /api/data")
	fmt.Println("🔒 Designed to run behind Apache reverse proxy")
	fmt.Println(strings.Repeat("=", 60))
	fmt.Println("\nPress Ctrl+C to stop\n")

	http.HandleFunc("/", handleRoot)
	http.HandleFunc("/api/data", handleData)

	log.Fatal(http.ListenAndServe("127.0.0.1:8082", nil))
}
