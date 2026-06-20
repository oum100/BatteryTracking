# ESP32-C3 SoftAP Provisioning Example

PlatformIO example for `ESP32-C3 Super Mini` with:

- `A0` battery voltage input
- `A1` lift switch input
- `A2` WS2812 RGB LED via FastLED
- SoftAP Wi-Fi setup page
- `/voltage` endpoint for the Nuxt app

## Flow

1. On boot, the device tries saved office Wi-Fi credentials.
2. If connect fails, it starts a setup AP:
   - SSID: `ESP32-LIFTER-SETUP`
   - Password: `12345678`
3. Connect a phone to that AP temporarily.
4. Open `http://192.168.4.1`
5. Enter office Wi-Fi SSID and password.
6. The device reboots and joins office Wi-Fi.
7. After that, the phone and ESP32 can both stay on office Wi-Fi and the app can call `/voltage`.

## Important

- `A0` must read through a proper voltage divider, not directly from the battery.
- Adjust `DIVIDER_RATIO` in `src/main.cpp` to match the real circuit.
- If `A0/A1/A2` do not map correctly on your board package, replace them with the real GPIO numbers.

## Endpoints

- `GET /voltage`
- `GET /wifi/status`
- `POST /wifi/save`
- `POST /wifi/clear`
