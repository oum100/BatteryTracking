# ESP32 Firmware Example

ไฟล์ตัวอย่าง firmware อยู่ที่:

- [examples/esp32-voltage-server/esp32-voltage-server.ino](/Users/teerin/Documents/MyDev/Nuxt4/BatteryTracking/examples/esp32-voltage-server/esp32-voltage-server.ino)

ไฟล์นี้ตั้งใจให้เป็น `starter` สำหรับ Arduino IDE หรือ PlatformIO โดยเน้นให้ตอบ endpoint:

```txt
GET /voltage
```

ให้ตรงกับ contract ใน:

- [docs/esp32-voltage-contract.md](/Users/teerin/Documents/MyDev/Nuxt4/BatteryTracking/docs/esp32-voltage-contract.md)

## สิ่งที่ตัวอย่างนี้ทำให้แล้ว

- ต่อ Wi-Fi แบบ `station mode`
- เปิด HTTP server port `80`
- มี endpoint `/voltage`
- ใช้ `micro switch` เป็น trigger ว่าเริ่มวัดจริงหรือยัง
- อ่าน ADC จากขาแรงดัน
- ทำ averaging หลาย sample
- เช็กความนิ่งของค่า
- ส่ง `status`, `state`, `stable`, `triggered`, `voltage`

## สิ่งที่ต้องแก้ก่อนใช้จริง

### 1. Wi-Fi

แก้ค่า:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

### 2. Pin

แก้ตามวงจรจริง:

```cpp
const int VOLTAGE_PIN = 34;
const int SWITCH_PIN = 27;
```

### 3. Voltage Divider

ถ้ากำลังวัดแบตที่แรงดันสูงกว่า ADC รับได้ ต้องมีวงจร divider แล้วคำนวณ ratio ให้ถูก

```cpp
const float DIVIDER_RATIO = 5.70f;
```

ตัวนี้สำคัญมาก ถ้าตั้งผิด ค่า voltage จะผิดทั้งระบบ

### 4. Calibration

ถ้ามี offset จริงจากฮาร์ดแวร์ ให้เติม:

```cpp
const float VOLTAGE_OFFSET = 0.00f;
```

## Logic ที่ใช้ตอนนี้

### Idle

ถ้า micro switch ยังไม่ถูกกด:

- `triggered = false`
- `stable = false`
- `status = idle`
- `voltage = 0`

### Measuring

ถ้า switch ถูกกด แต่ค่าเพิ่งเริ่มอ่าน:

- `triggered = true`
- `stable = false`
- `status = measuring`

### Ready

เมื่อ sample ครบ window และค่าเฉลี่ยต่างจากรอบก่อนน้อยกว่า `STABLE_DELTA` ต่อเนื่องหลายครั้ง:

- `stable = true`
- `status = ready`
- `state = ready`

## ค่า threshold ที่ปรับได้

```cpp
const unsigned long SAMPLE_INTERVAL_MS = 120;
const int SAMPLE_WINDOW = 8;
const float STABLE_DELTA = 0.12f;
const int STABLE_HITS_REQUIRED = 3;
```

ถ้าอ่านแล้วแกว่งมาก:

- เพิ่ม `SAMPLE_WINDOW`
- เพิ่ม `STABLE_HITS_REQUIRED`
- หรือขยาย `STABLE_DELTA` เล็กน้อย

ถ้าหน่วงช้าเกิน:

- ลด `SAMPLE_WINDOW`
- ลด `STABLE_HITS_REQUIRED`

## JSON ที่หน้ามือถือจะอ่านได้

ตัวอย่าง response:

```json
{
  "deviceId": "ESP32-LIFTER-01",
  "sampledAt": "2026-06-11T14:23:18Z",
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

## การทดสอบเร็ว

1. flash firmware เข้า ESP32
2. ดู IP จาก Serial Monitor
3. เปิดในมือถือหรือ browser:

```txt
http://<esp32-ip>/voltage
```

4. เอา IP นี้ไปใส่ในหน้า `battery-movement`
5. กด `Use Device IP` หรือพิมพ์ endpoint เอง

## ข้อควรระวัง

- ห้ามป้อนแรงดันเข้าขา ADC ตรง ถ้าเกินช่วงรับของ ESP32
- ต้องมี voltage divider ที่ออกแบบถูกต้อง
- ถ้าหน้างานมี noise มาก ควรเสริม RC filter หรือเฉลี่ยหลาย sample เพิ่ม
- ถ้าจะใช้แบบ access point แทน Wi-Fi โรงงาน ต้องแก้ส่วน connect Wi-Fi เพิ่ม

## ถัดไปที่ควรทำ

ถ้าจะเอาไปใช้จริงต่อ ผมแนะนำลำดับนี้:

1. ยืนยันวงจรแบ่งแรงดัน
2. วัดเทียบกับ multimeter แล้วหา calibration
3. ทดสอบ micro switch จริงบนตัวยก
4. ทดสอบ polling จากหน้า `battery-movement`
5. ค่อยต่อ save ลง database จริง
