# ESP32 Voltage Contract

เอกสารนี้ใช้คุยระหว่างฝั่ง `Nuxt mobile UI` และฝั่ง `ESP32 firmware`

เป้าหมายคือให้หน้า [battery-movement.vue](/Users/teerin/Documents/MyDev/Nuxt4/BatteryTracking/app/pages/battery-movement.vue) อ่านค่าแรงดันจาก ESP32 ได้แบบอัตโนมัติใน `STEP 3`

## Endpoint

แนะนำให้ ESP32 เปิด HTTP endpoint:

```txt
GET /voltage
```

ตัวอย่างจริง:

```txt
http://192.168.4.1/voltage
```

## Response JSON

แนะนำให้ส่งอย่างน้อย field เหล่านี้:

```json
{
  "deviceId": "ESP32-LIFTER-01",
  "status": "ready",
  "state": "ready",
  "stable": true,
  "triggered": true,
  "switchPressed": true,
  "contact": true,
  "probeContact": true,
  "measureEnabled": true,
  "voltage": 12.45,
  "sampledAt": "2026-06-11T14:23:18.000Z"
}
```

## Field Meaning

- `deviceId`: รหัสอุปกรณ์ ESP32
- `voltage`: ค่าแรงดันล่าสุด
- `sampledAt`: เวลาอ่านค่า
- `stable`: ค่านี้นิ่งแล้ว พร้อมให้บันทึก
- `triggered`: มี trigger จาก micro switch หรือ logic วัดแล้ว
- `switchPressed`: micro switch ถูกกดอยู่
- `contact`: probe แตะจุดวัดแล้ว
- `probeContact`: alias ของ contact ถ้าฝั่ง firmware ใช้ชื่อนี้
- `measureEnabled`: อนุญาตให้ระบบถือว่ากำลังวัดจริง
- `status` / `state`: string สำหรับบอกสถานะรวม เช่น `idle`, `measuring`, `ready`

## Recommended States

### 1. Idle

ยังไม่แตะหัววัด หรือยังไม่เริ่มวัด

```json
{
  "status": "idle",
  "state": "idle",
  "stable": false,
  "triggered": false,
  "switchPressed": false,
  "contact": false,
  "probeContact": false,
  "measureEnabled": false,
  "voltage": 0
}
```

### 2. Measuring

แตะหัววัดแล้ว แต่ค่ากำลังแกว่ง

```json
{
  "status": "measuring",
  "state": "measuring",
  "stable": false,
  "triggered": true,
  "switchPressed": true,
  "contact": true,
  "probeContact": true,
  "measureEnabled": true,
  "voltage": 12.32
}
```

### 3. Ready

ค่าคงที่แล้ว พร้อมให้มือถือรับเข้า flow

```json
{
  "status": "ready",
  "state": "ready",
  "stable": true,
  "triggered": true,
  "switchPressed": true,
  "contact": true,
  "probeContact": true,
  "measureEnabled": true,
  "voltage": 12.45
}
```

## Current Frontend Logic

ตอนนี้หน้า `battery-movement` จะ:

1. เข้า `STEP 3`
2. เริ่ม polling endpoint ทุก `900ms`
3. พยายามหา `voltage`
4. รอให้ค่าใกล้กันอย่างน้อย `2 ครั้ง`
5. ถ้า `stable = true` หรือ `ready = true` หรือ `state/status = ready`
6. จะรับค่าเข้า draft แล้วไป step ถัดไปอัตโนมัติ

## Important Note About Low Readings

ถ้า ESP32 ส่งค่า `0`, `0.1`, `1.5`, `2.0V` ระหว่าง probe ยังไม่นิ่ง

ฝั่ง UI ตอนนี้จะยังไม่ถือว่า “พร้อมบันทึก” ทันที ยกเว้น:

- firmware บอก `stable: true`
- หรือ `state/status = ready`
- หรือมี trigger/contact จริง และค่าเริ่มนิ่งซ้ำกัน

ดังนั้นฝั่ง firmware ควรส่ง flag `stable` ชัดที่สุด

## Micro Switch Recommendation

ถ้าติด `micro switch` กับตัวยกได้ แนะนำให้ ESP32 ใช้ switch นี้เป็น trigger หลัก:

- switch ไม่ถูกกด = `triggered: false`
- switch ถูกกด แต่ค่ายังแกว่ง = `triggered: true`, `stable: false`
- switch ถูกกด และค่าเฉลี่ยนิ่งแล้ว = `triggered: true`, `stable: true`

แบบนี้มือถือจะรู้ทันทีว่า “วัดจริง” หรือ “ยังไม่พร้อม”

## Mock API In Nuxt

ในโปรเจกต์นี้มี mock route สำหรับทดสอบแล้ว:

```txt
GET /api/mock/esp32-voltage
```

ไฟล์:

- [server/api/mock/esp32-voltage.get.ts](/Users/teerin/Documents/MyDev/Nuxt4/BatteryTracking/server/api/mock/esp32-voltage.get.ts)

ตัวอย่าง:

```txt
/api/mock/esp32-voltage
/api/mock/esp32-voltage?mode=ready
/api/mock/esp32-voltage?mode=unstable
/api/mock/esp32-voltage?mode=ready&voltage=12.67
/api/mock/esp32-voltage?mode=idle&contact=0
```

## Integration Recommendation

ลำดับที่ควรทำต่อ:

1. ให้ฝั่ง ESP32 ทำ `/voltage` ตาม contract นี้ก่อน
2. ทดสอบกับ mock route ให้ flow มือถือจบครบ
3. เปลี่ยน endpoint ในหน้า `battery-movement` จาก mock เป็น IP จริง
4. ค่อยต่อ backend save จริงภายหลัง

## Firmware Starter

มีตัวอย่าง firmware แล้วที่:

- [examples/esp32-voltage-server/esp32-voltage-server.ino](/Users/teerin/Documents/MyDev/Nuxt4/BatteryTracking/examples/esp32-voltage-server/esp32-voltage-server.ino)
- [docs/esp32-firmware-example.md](/Users/teerin/Documents/MyDev/Nuxt4/BatteryTracking/docs/esp32-firmware-example.md)
