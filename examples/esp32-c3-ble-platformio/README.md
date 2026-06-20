# ESP32-C3 BLE Voltage Example

PlatformIO example for `ESP32-C3 Super Mini` with:

- `A0` battery voltage input
- optional `A1` lift switch input
- optional `A2` WS2812 RGB LED via FastLED
- optional onboard LED for status
- BLE advertisement and GATT characteristic

## BLE Design

This firmware exposes:

- Device name: `ESP32C3_BLE-BATTMETER-01`
- Service UUID: `7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001`
- Characteristic UUID: `7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001`

The characteristic supports:

- `READ`
- `NOTIFY`

The characteristic value is a JSON string like:

```json
{
  "deviceId": "ESP32C3-BATTMETER-01",
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

- Slow blink / purple blink: advertising, no BLE client connected
- Fast blink / orange blink: measuring
- Solid on / green: stable and ready
- Off / blue: connected but idle, depending on enabled LED mode

## Configuration Flags

Edit the flags near the top of `src/main.cpp`:

- `USE_LIFT_SWITCH`
- `USE_RGB_LED`
- `USE_ONBOARD_LED`
- `USE_MOCK_VOLTAGE`

Recommended combinations:

- Bring-up test:
  - `USE_LIFT_SWITCH = false`
  - `USE_RGB_LED = false`
  - `USE_ONBOARD_LED = true`
  - `USE_MOCK_VOLTAGE = true`

- Real hardware:
  - `USE_LIFT_SWITCH = true`
  - `USE_RGB_LED = true`
  - `USE_ONBOARD_LED = true`
  - `USE_MOCK_VOLTAGE = false`

## Important

- `A0` must read through a proper voltage divider, never directly from the battery.
- Adjust `DIVIDER_RATIO` in `src/main.cpp` to match the real circuit.
- If `A0/A1/A2` or `LED_BUILTIN` do not map correctly on your board package, replace them with real GPIO numbers.

## Notes

- `Lift_SW` is expected as `A1 -> switch -> GND` when enabled
- `RGB` assumes a single `WS2812` on `A2` when enabled
- `sampledAtMs` is uptime-based, useful when BLE is used without Wi-Fi/NTP
- Serial Monitor prints ADC and BLE state about once per second
- BLE payload includes `mockVoltageUsed`, `liftSwitchUsed`, and `rgbLedUsed`
