# ESP32-C3 BLE Voltage Example

PlatformIO example for `ESP32-C3 Super Mini` with:

- `A0` battery voltage input
- `A1` lift switch input
- `A2` WS2812 RGB LED via FastLED
- BLE advertisement and GATT characteristic

## BLE Design

This firmware exposes:

- Device name: `ESP32-LIFTER-01`
- Service UUID: `7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001`
- Characteristic UUID: `7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001`

The characteristic supports:

- `READ`
- `NOTIFY`

The characteristic value is a JSON string like:

```json
{
  "deviceId": "ESP32-C3-LIFTER-01",
  "transport": "ble",
  "sampledAt": "",
  "sampledAtMs": 18234,
  "triggered": true,
  "switchPressed": true,
  "contact": true,
  "probeContact": true,
  "measureEnabled": true,
  "status": "ready",
  "state": "ready",
  "stable": true,
  "voltage": 12.45
}
```

## LED State

- Purple blink: advertising, no BLE client connected
- Blue: connected but idle
- Orange blink: measuring
- Green: stable and ready

## Important

- `A0` must read through a proper voltage divider, never directly from the battery.
- Adjust `DIVIDER_RATIO` in `src/main.cpp` to match the real circuit.
- If `A0/A1/A2` do not map correctly on your board package, replace them with real GPIO numbers.

## Notes

- `Lift_SW` is expected to be wired as `A1 -> switch -> GND`
- The firmware uses `INPUT_PULLUP`, so pressed means `LOW`
- `sampledAtMs` is uptime-based, useful when BLE is used without Wi-Fi/NTP
