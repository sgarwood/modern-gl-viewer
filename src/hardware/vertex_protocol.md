# Vertex Golf Sensor Protocol (BLE GATT)

## Overview
Unlike the MLM2PRO, the Vertex Golf putting sensor does not broadcast to an open TCP API. It operates via standard Bluetooth Low Energy (BLE). To integrate it, we must connect directly to the sensor using a C++ BLE library (e.g., `BlueZ` on Linux or `WinRT` on Windows) and subscribe to its custom GATT characteristics.

## Protocol Reverse-Engineering Strategy
Because the Vertex GATT protocol is proprietary and undocumented, the implementation requires a reverse-engineering phase using standard BLE analysis:

### 1. GATT Discovery (nRF Connect)
Using an app like *nRF Connect*, we scan for the Vertex sensor and map its Services and Characteristics. 
*   Look for a **Notification (NOTIFY)** characteristic. This is the pipeline the sensor uses to push live stroke telemetry to the phone without being polled.

### 2. Payload Sniffing (HCI Snoop)
To decode the hex payloads sent over the NOTIFY characteristic, we must:
1. Enable `Bluetooth HCI Snoop Log` in Android Developer Options.
2. Pair the Vertex sensor with the official Android app.
3. Hit 5-10 putts with varying speeds and face angles.
4. Pull the `btsnoop_hci.log` file into Wireshark.
5. Cross-reference the hex data with the metrics displayed on the app screen (e.g., matching a hex value of `0x00A0` to a 1.0 m/s putter speed).

### 3. Data Structure Hypothesis
Based on typical golf sensor BLE payloads, the byte array likely follows a packed struct format similar to:
```cpp
// Hypothetical unpacked Vertex BLE payload
struct VertexBlePayload {
    uint8_t packet_id;
    uint16_t impact_speed_mm_per_sec;
    int16_t face_angle_at_impact_tenths_deg;
    int16_t club_path_tenths_deg;
    // ...
} __attribute__((packed));
```

## Implementation Steps for `VertexAdapter.cpp`
1. Integrate a cross-platform BLE library (like `SimpleBLE`).
2. Run a background `std::jthread` that scans for devices advertising the Vertex UUID.
3. Connect to the device and subscribe to the identified `NOTIFY` characteristic.
4. In the BLE receive callback, parse the byte array into putting metrics (Ball Speed, Launch Direction).
5. We will calculate the *ball speed* implicitly from the *club speed* and *smash factor* if the sensor only provides club data.
6. Populate `ShotData` (setting `is_putt = true`) and fire the `callback_`.
