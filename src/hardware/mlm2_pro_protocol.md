# MLM2PRO Integration Protocol (GSPro Open API)

## Overview
Instead of reverse-engineering the Rapsodo MLM2PRO proprietary Bluetooth or Wi-Fi packets, we will leverage its **official GSPro integration feature**. 

When a user links their MLM2PRO to GSPro via the Rapsodo mobile app, the app expects to find a GSPro Connect server running on the local PC. By implementing the **GSPro OpenAPI spec**, our engine will impersonate GSPro, allowing the MLM2PRO (and any other GSPro-compatible launch monitor) to seamlessly send shot data directly into our game.

## The GSPro Open Connect Protocol
*   **Transport:** TCP/IP Socket
*   **Host:** `127.0.0.1` (Localhost)
*   **Port:** `921` (Default GSPro port)
*   **Authentication:** None. Open socket.

## Data Payload (JSON)
The client (MLM2PRO app) will send a JSON payload to our TCP socket every time a shot is registered. We must parse this JSON to populate our `ShotData` struct.

### Example Incoming JSON Payload
```json
{
  "DeviceID": "MLM2PRO",
  "Units": "Yards",
  "ShotNumber": 1,
  "APIversion": "1",
  "BallData": {
    "Speed": 160.5,
    "SpinAxis": -5.2,
    "TotalSpin": 2500,
    "HLA": 1.2,
    "VLA": 12.5
  },
  "ClubData": {
    "Speed": 110.1
  },
  "ShotDataOptions": {
    "ContainsBallData": true,
    "ContainsClubData": true
  }
}
```

## Mapping to mgv::hardware::ShotData
Our `Mlm2ProAdapter` (which is effectively a GSPro API Server) will parse the JSON and map it to our internal struct:

*   `BallData.Speed` (MPH) -> `ball_speed_mps` (Convert MPH to m/s: `* 0.44704`)
*   `BallData.VLA` (Vertical Launch Angle) -> `launch_angle_deg`
*   `BallData.HLA` (Horizontal Launch Angle) -> `launch_direction_deg`
*   `BallData.TotalSpin` -> `total_spin_rpm`
*   `BallData.SpinAxis` -> `spin_axis_deg`

## Implementation Steps for `Mlm2ProAdapter.cpp`
1. Spin up a `std::jthread` running a TCP socket listener on `0.0.0.0:921`.
2. Accept incoming connections (usually from the mobile device on the same Wi-Fi network).
3. Read incoming bytes into a string buffer until a newline or complete JSON object is formed.
4. Use a lightweight JSON parser (e.g., nlohmann/json or write a rapid parser).
5. Convert units and fire the `callback_`.
