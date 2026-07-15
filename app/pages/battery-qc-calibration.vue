<script setup lang="ts">
interface VoltMeterCalibrationRecord {
  id: string
  deviceId: string
  deviceName: string | null
  inaGain: number
  inaOffset: number
  pointOneMeter: number
  pointOneEsp: number
  pointTwoMeter: number
  pointTwoEsp: number
  syncedAt: string | null
  createdAt: string
  updatedAt: string
}

useHead({
  title: 'QC Calibration',
})

const bleConnected = ref(false)
const bleDeviceName = ref('BLE Volt Meter')
const actionMessage = ref('เข้า Calibration Mode ที่อุปกรณ์ก่อน แล้วกด Connect BLE')
const calibrateSubmitting = ref(false)
const calibrateSyncing = ref(false)
const calibrateResyncing = ref(false)
const calibratePointOneMeter = ref('')
const calibratePointOneEsp = ref('')
const calibratePointTwoMeter = ref('')
const calibratePointTwoEsp = ref('')
const calibrateResultGain = ref<number | null>(null)
const calibrateResultOffset = ref<number | null>(null)
const calibratePreviewMessage = ref('')
const currentEspInaGain = ref<number | null>(null)
const currentEspInaOffset = ref<number | null>(null)
const lastEspVoltReading = ref<number | null>(null)
const bleMeterDeviceId = ref('')
const bleMeterFirmwareVersion = ref('')
const savedDbCalibration = ref<VoltMeterCalibrationRecord | null>(null)

const BLE_SERVICE_UUID = '7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001'
const BLE_STATUS_CHARACTERISTIC_UUID = '7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001'
const BLE_CALIBRATION_CHARACTERISTIC_UUID = '7f9e0003-6a9d-4f7e-8d4d-32e7be6f1001'
const BLE_DEVICE_NAME_PREFIX = 'PUMA-Voltmeter-'

let bleDevice: BluetoothDevice | null = null
let bleStatusCharacteristic: BluetoothRemoteGATTCharacteristic | null = null
let bleCalibrationCharacteristic: BluetoothRemoteGATTCharacteristic | null = null

const currentEspCalibrationLabel = computed(() => {
  if (currentEspInaGain.value === null || currentEspInaOffset.value === null) {
    return 'ยังไม่ได้อ่านค่าปัจจุบันจาก ESP'
  }

  return `ESP current: gain ${currentEspInaGain.value.toFixed(5)} • offset ${currentEspInaOffset.value >= 0 ? '+' : ''}${currentEspInaOffset.value.toFixed(5)}`
})

const savedDbCalibrationLabel = computed(() => {
  if (!savedDbCalibration.value) {
    return 'ยังไม่มี calibration ล่าสุดในฐานข้อมูลสำหรับเครื่องนี้'
  }

  return `DB latest: gain ${savedDbCalibration.value.inaGain.toFixed(5)} • offset ${savedDbCalibration.value.inaOffset >= 0 ? '+' : ''}${savedDbCalibration.value.inaOffset.toFixed(5)}`
})

const calibrationBleStatusLabel = computed(() => {
  if (bleConnected.value && bleStatusCharacteristic) {
    return bleDeviceName.value
      ? `Connected: ${bleDeviceName.value}`
      : 'Connected to BLE Volt Meter'
  }

  return 'ยังไม่ได้เชื่อมต่อ BLE สำหรับ calibration'
})

const calibrationSaveButtonLabel = computed(() => bleConnected.value && bleCalibrationCharacteristic
  ? 'Save To DB + Sync BLE'
  : 'Save To DB')

const canResyncCalibrationToBle = computed(() => Boolean(
  bleConnected.value
  && bleCalibrationCharacteristic
  && savedDbCalibration.value,
))

const calibrateResultLabel = computed(() => {
  if (calibrateResultGain.value === null || calibrateResultOffset.value === null) {
    return 'กรอกค่าเทียบ 2 จุด เพื่อคำนวณ gain และ offset'
  }

  const gain = calibrateResultGain.value.toFixed(5)
  const offset = calibrateResultOffset.value >= 0
    ? `+${calibrateResultOffset.value.toFixed(5)}`
    : calibrateResultOffset.value.toFixed(5)

  return `Vcal = (Vesp x ${gain}) ${offset.startsWith('-') ? '-' : '+'} ${Math.abs(calibrateResultOffset.value).toFixed(5)}`
})

function deriveDeviceIdFromBleName(name: string) {
  const trimmed = name.trim()
  if (!trimmed) {
    return ''
  }

  if (trimmed.startsWith(BLE_DEVICE_NAME_PREFIX)) {
    return `PUMA-VoltMeter-${trimmed.slice(BLE_DEVICE_NAME_PREFIX.length)}`
  }

  return ''
}

function parseCalibrationNumber(value: string) {
  const parsed = Number.parseFloat(value.trim())
  return Number.isFinite(parsed) ? parsed : null
}

function updateCalibrationPreview() {
  const meterOne = parseCalibrationNumber(calibratePointOneMeter.value)
  const espOne = parseCalibrationNumber(calibratePointOneEsp.value)
  const meterTwo = parseCalibrationNumber(calibratePointTwoMeter.value)
  const espTwo = parseCalibrationNumber(calibratePointTwoEsp.value)

  calibratePreviewMessage.value = ''
  calibrateResultGain.value = null
  calibrateResultOffset.value = null

  if ([meterOne, espOne, meterTwo, espTwo].some(value => value === null)) {
    return
  }

  if (espOne === espTwo) {
    calibratePreviewMessage.value = 'ค่า ESP Volt Meter ทั้งสองจุดต้องไม่เท่ากัน'
    return
  }

  const gain = (meterTwo! - meterOne!) / (espTwo! - espOne!)
  const offset = meterOne! - (espOne! * gain)

  if (!Number.isFinite(gain) || !Number.isFinite(offset)) {
    calibratePreviewMessage.value = 'ไม่สามารถคำนวณ gain / offset ได้'
    return
  }

  calibrateResultGain.value = gain
  calibrateResultOffset.value = offset
  calibratePreviewMessage.value = `คำนวณแล้ว gain ${gain.toFixed(5)} และ offset ${offset >= 0 ? '+' : ''}${offset.toFixed(5)}`
}

function fillCalibrationEspInput(point: 1 | 2) {
  if (lastEspVoltReading.value === null) {
    actionMessage.value = 'ยังไม่มีค่า ESP Volt Meter ล่าสุดให้ดึงมาใช้'
    return
  }

  if (point === 1) {
    calibratePointOneEsp.value = lastEspVoltReading.value.toFixed(3)
  } else {
    calibratePointTwoEsp.value = lastEspVoltReading.value.toFixed(3)
  }

  updateCalibrationPreview()
}

async function readBleStatusPayload() {
  if (!bleStatusCharacteristic) {
    return null
  }

  const value = await bleStatusCharacteristic.readValue()
  const text = new TextDecoder().decode(value)
  return JSON.parse(text) as Record<string, any>
}

async function syncCalibrationFromBle() {
  if (!bleConnected.value || !bleStatusCharacteristic) {
    actionMessage.value = 'เชื่อมต่อ BLE Volt Meter ก่อนอ่าน calibration จากอุปกรณ์'
    return
  }

  calibrateSyncing.value = true

  try {
    const payload = await readBleStatusPayload()
    const deviceId = String(payload?.device_id ?? payload?.deviceId ?? '').trim()
    const firmwareVersion = String(payload?.fw ?? payload?.firmwareVersion ?? '').trim()
    const inaGain = typeof payload?.ina_gain === 'number' ? payload.ina_gain : Number.parseFloat(String(payload?.ina_gain ?? ''))
    const inaOffset = typeof payload?.ina_offset === 'number' ? payload.ina_offset : Number.parseFloat(String(payload?.ina_offset ?? ''))
    const voltage = typeof payload?.voltage === 'number' ? payload.voltage : Number.parseFloat(String(payload?.voltage ?? ''))

    bleMeterDeviceId.value = deviceId || deriveDeviceIdFromBleName(bleDeviceName.value)
    bleMeterFirmwareVersion.value = firmwareVersion
    currentEspInaGain.value = Number.isFinite(inaGain) ? inaGain : null
    currentEspInaOffset.value = Number.isFinite(inaOffset) ? inaOffset : null
    lastEspVoltReading.value = Number.isFinite(voltage) ? voltage : lastEspVoltReading.value
    actionMessage.value = bleMeterDeviceId.value
      ? `อ่าน calibration จาก ${bleMeterDeviceId.value} แล้ว`
      : 'อ่าน calibration จาก BLE Volt Meter แล้ว'
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'อ่านค่าคาลิเบรตจาก BLE ไม่สำเร็จ'
  }
  finally {
    calibrateSyncing.value = false
  }
}

async function loadCalibrationFromDb() {
  const deviceId = bleMeterDeviceId.value || deriveDeviceIdFromBleName(bleDeviceName.value)
  if (!deviceId) {
    savedDbCalibration.value = null
    actionMessage.value = 'ยังไม่มี device ID จาก BLE ให้อ่าน calibration ในฐานข้อมูล'
    return
  }

  try {
    const response = await $fetch<{ ok: boolean, calibration: VoltMeterCalibrationRecord | null }>('/api/voltmeter-calibrations/latest', {
      query: { deviceId },
    })
    savedDbCalibration.value = response.calibration
    actionMessage.value = response.calibration
      ? `โหลด calibration ล่าสุดของ ${deviceId} จาก DB แล้ว`
      : `ยังไม่มี calibration ของ ${deviceId} ใน DB`
  }
  catch (error) {
    savedDbCalibration.value = null
    actionMessage.value = error instanceof Error ? error.message : 'โหลด calibration จาก DB ไม่สำเร็จ'
  }
}

function applySavedCalibrationToForm() {
  if (!savedDbCalibration.value) {
    actionMessage.value = 'ยังไม่มี calibration ใน DB ให้โหลด'
    return
  }

  calibratePointOneMeter.value = savedDbCalibration.value.pointOneMeter.toFixed(3)
  calibratePointOneEsp.value = savedDbCalibration.value.pointOneEsp.toFixed(3)
  calibratePointTwoMeter.value = savedDbCalibration.value.pointTwoMeter.toFixed(3)
  calibratePointTwoEsp.value = savedDbCalibration.value.pointTwoEsp.toFixed(3)
  calibrateResultGain.value = savedDbCalibration.value.inaGain
  calibrateResultOffset.value = savedDbCalibration.value.inaOffset
  calibratePreviewMessage.value = `โหลด calibration ล่าสุดจาก DB แล้ว (gain ${savedDbCalibration.value.inaGain.toFixed(5)}, offset ${savedDbCalibration.value.inaOffset >= 0 ? '+' : ''}${savedDbCalibration.value.inaOffset.toFixed(5)})`
}

async function syncSavedCalibrationToBle() {
  if (!savedDbCalibration.value) {
    actionMessage.value = 'ยังไม่มี calibration ใน DB สำหรับ sync'
    return
  }

  if (!bleConnected.value || !bleCalibrationCharacteristic) {
    actionMessage.value = 'เชื่อมต่อ BLE Volt Meter ก่อน แล้วค่อย sync calibration'
    return
  }

  calibrateResyncing.value = true

  try {
    const syncPayload = JSON.stringify({
      cmd: 'set_calibration',
      ina_gain: savedDbCalibration.value.inaGain,
      ina_offset: savedDbCalibration.value.inaOffset,
    })

    await bleCalibrationCharacteristic.writeValue(new TextEncoder().encode(syncPayload))

    const syncedAt = new Date().toISOString()
    const response = await $fetch<{ ok: boolean, calibration: VoltMeterCalibrationRecord }>('/api/voltmeter-calibrations', {
      method: 'POST',
      body: {
        deviceId: savedDbCalibration.value.deviceId,
        deviceName: savedDbCalibration.value.deviceName,
        inaGain: savedDbCalibration.value.inaGain,
        inaOffset: savedDbCalibration.value.inaOffset,
        pointOneMeter: savedDbCalibration.value.pointOneMeter,
        pointOneEsp: savedDbCalibration.value.pointOneEsp,
        pointTwoMeter: savedDbCalibration.value.pointTwoMeter,
        pointTwoEsp: savedDbCalibration.value.pointTwoEsp,
        syncedAt,
      },
    })

    savedDbCalibration.value = response.calibration
    await syncCalibrationFromBle()
    actionMessage.value = `sync calibration ล่าสุดจาก DB ไปที่ ${savedDbCalibration.value.deviceId} แล้ว`
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'sync calibration จาก DB ไป BLE ไม่สำเร็จ'
  }
  finally {
    calibrateResyncing.value = false
  }
}

function handleBleDisconnect() {
  bleConnected.value = false
  bleDeviceName.value = 'BLE Volt Meter'
  bleDevice = null
  bleStatusCharacteristic = null
  bleCalibrationCharacteristic = null
  bleMeterDeviceId.value = ''
  bleMeterFirmwareVersion.value = ''
  actionMessage.value = 'BLE Volt Meter disconnected'
}

async function connectToBleVoltMeterDevice(device: BluetoothDevice, showConnectedMessage = true) {
  bleDevice = device
  bleDeviceName.value = device.name || 'BLE Volt Meter'
  bleDevice.addEventListener('gattserverdisconnected', handleBleDisconnect)

  const server = await bleDevice.gatt?.connect()
  if (!server) {
    throw new Error('BLE Volt Meter GATT connection failed')
  }

  const service = await server.getPrimaryService(BLE_SERVICE_UUID)
  bleStatusCharacteristic = await service.getCharacteristic(BLE_STATUS_CHARACTERISTIC_UUID)
  bleCalibrationCharacteristic = await service.getCharacteristic(BLE_CALIBRATION_CHARACTERISTIC_UUID)
  bleConnected.value = true
  bleMeterDeviceId.value = deriveDeviceIdFromBleName(bleDeviceName.value)
  await syncCalibrationFromBle()
  await loadCalibrationFromDb()

  if (showConnectedMessage) {
    actionMessage.value = bleDeviceName.value.startsWith(BLE_DEVICE_NAME_PREFIX)
      ? `${bleDeviceName.value} connected`
      : `${bleDeviceName.value} connected (service matched)`
  }
}

async function connectBleVoltMeter() {
  if (typeof window === 'undefined') {
    return
  }

  if (bleConnected.value && bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect()
    handleBleDisconnect()
    return
  }

  const bluetooth = (navigator as any).bluetooth
  if (!bluetooth?.requestDevice) {
    actionMessage.value = 'Web Bluetooth is not supported in this browser'
    return
  }

  try {
    actionMessage.value = 'Select BLE Volt Meter in Calibration Mode'

    const device = await bluetooth.requestDevice({
      acceptAllDevices: true,
      optionalServices: [BLE_SERVICE_UUID],
    })

    await connectToBleVoltMeterDevice(device)
  }
  catch (error) {
    if (bleDevice?.gatt?.connected) {
      bleDevice.gatt.disconnect()
    }

    bleDevice = null
    bleConnected.value = false
    bleDeviceName.value = 'BLE Volt Meter'
    actionMessage.value = error instanceof Error
      ? `${error.message}. ถ้าอุปกรณ์เคย pair แบบ HID keyboard ให้ Disconnect/Forget จาก Bluetooth ของเครื่องก่อน แล้วบูตเข้า Calibration Mode ใหม่`
      : 'Unable to connect BLE Volt Meter'
  }
}

async function submitCalibrationToDbAndBle() {
  updateCalibrationPreview()

  if (calibrateResultGain.value === null || calibrateResultOffset.value === null) {
    actionMessage.value = calibratePreviewMessage.value || 'คำนวณ gain / offset ยังไม่สำเร็จ'
    return
  }

  const deviceId = bleMeterDeviceId.value || deriveDeviceIdFromBleName(bleDeviceName.value)
  if (!deviceId) {
    actionMessage.value = 'เชื่อมต่อ BLE Volt Meter ก่อน เพื่อระบุ device ID สำหรับบันทึก calibration'
    return
  }

  calibrateSubmitting.value = true

  try {
    const payload = {
      deviceId,
      deviceName: bleDeviceName.value || null,
      inaGain: Number(calibrateResultGain.value.toFixed(5)),
      inaOffset: Number(calibrateResultOffset.value.toFixed(5)),
      pointOneMeter: Number(parseCalibrationNumber(calibratePointOneMeter.value)?.toFixed(3)),
      pointOneEsp: Number(parseCalibrationNumber(calibratePointOneEsp.value)?.toFixed(3)),
      pointTwoMeter: Number(parseCalibrationNumber(calibratePointTwoMeter.value)?.toFixed(3)),
      pointTwoEsp: Number(parseCalibrationNumber(calibratePointTwoEsp.value)?.toFixed(3)),
      syncedAt: null as string | null,
    }

    const savedResponse = await $fetch<{ ok: boolean, calibration: VoltMeterCalibrationRecord }>('/api/voltmeter-calibrations', {
      method: 'POST',
      body: payload,
    })

    savedDbCalibration.value = savedResponse.calibration

    if (bleCalibrationCharacteristic) {
      const syncPayload = JSON.stringify({
        cmd: 'set_calibration',
        ina_gain: payload.inaGain,
        ina_offset: payload.inaOffset,
      })

      await bleCalibrationCharacteristic.writeValue(new TextEncoder().encode(syncPayload))

      const syncedAt = new Date().toISOString()
      const syncedResponse = await $fetch<{ ok: boolean, calibration: VoltMeterCalibrationRecord }>('/api/voltmeter-calibrations', {
        method: 'POST',
        body: {
          ...payload,
          syncedAt,
        },
      })

      savedDbCalibration.value = syncedResponse.calibration
      await syncCalibrationFromBle()
      actionMessage.value = `บันทึก calibration ลง DB และ sync ไป ${deviceId} แล้ว`
    } else {
      actionMessage.value = `บันทึก calibration ลง DB สำหรับ ${deviceId} แล้ว ยังไม่ได้ sync BLE`
    }

    currentEspInaGain.value = calibrateResultGain.value
    currentEspInaOffset.value = calibrateResultOffset.value
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'บันทึกหรือ sync calibration ไม่สำเร็จ'
  }
  finally {
    calibrateSubmitting.value = false
  }
}
</script>

<template>
  <main class="min-h-screen bg-[radial-gradient(circle_at_top,_rgba(168,85,247,0.12),_transparent_35%),linear-gradient(180deg,_#f7fafc_0%,_#eef2f7_100%)] px-4 py-6 lg:px-8">
    <div class="mx-auto max-w-7xl space-y-6">
      <UCard :ui="{ root: 'rounded-[28px] border-0 bg-slate-950 shadow-[0_24px_70px_rgba(15,23,42,0.22)]', body: 'p-6 lg:p-8' }">
        <div class="grid gap-6 lg:grid-cols-[minmax(0,1.15fr)_auto_minmax(0,0.9fr)] lg:items-center">
          <div class="min-w-0">
            <div class="text-xs font-black uppercase tracking-[0.24em] text-emerald-200/90">QC Calibration</div>
            <div class="mt-2 text-4xl font-black tracking-tight text-white">INA226 Gain / Offset</div>
            <div class="mt-3 max-w-2xl text-base leading-7 text-slate-300">
              ใช้สำหรับอ่านและ sync ค่า calibration ของ ESP Volt Meter โดยต้องบูตอุปกรณ์เข้า Calibration Mode ก่อนทุกครั้ง
            </div>
          </div>

          <div class="flex justify-center lg:px-4">
            <img src="/branding/logo-puma-battery.png" alt="PUMA Battery" class="h-20 w-auto object-contain lg:h-24" />
          </div>

          <div class="flex flex-wrap items-center justify-start gap-3 lg:justify-end">
            <UButton to="/" color="neutral" variant="soft" class="rounded-xl border border-white/20 bg-white/8 px-4 py-2.5 text-sm font-black text-white hover:bg-white/14">
              Home
            </UButton>
            <UButton to="/battery-qc-system" color="neutral" variant="soft" class="rounded-xl border border-white/20 bg-white/8 px-4 py-2.5 text-sm font-black text-white hover:bg-white/14">
              Battery QC System
            </UButton>
          </div>
        </div>
      </UCard>

      <div class="grid gap-6 xl:grid-cols-[minmax(0,1.1fr)_minmax(0,1fr)]">
        <UCard :ui="{ root: 'rounded-[24px] border-0 bg-white shadow-[0_18px_48px_rgba(15,23,42,0.08)]', body: 'p-6' }">
          <div class="text-xs font-black uppercase tracking-[0.22em] text-slate-500">Calibration Mode</div>
          <div class="mt-4 grid gap-3 text-sm leading-6 text-slate-700">
            <div>1. ปิดเครื่อง Volt Meter</div>
            <div>2. กดปุ่ม BOOT ค้างไว้ แล้วจ่ายไฟเข้าเครื่อง</div>
            <div>3. รอให้เครื่องเข้า BLE Calibration Mode แล้วค่อยกด <span class="font-black text-slate-950">Connect BLE</span></div>
            <div>4. ถ้าอุปกรณ์เคย pair เป็นคีย์บอร์ด HID มาก่อน ให้ Disconnect หรือ Forget จาก Bluetooth ของเครื่องก่อน</div>
            <div>5. อ่านค่าจาก 2 จุด แล้วบันทึกลง DB พร้อม sync กลับไปที่อุปกรณ์</div>
          </div>
        </UCard>

        <UCard :ui="{ root: 'rounded-[24px] border-0 bg-slate-950 text-white shadow-[0_18px_48px_rgba(15,23,42,0.18)]', body: 'p-6' }">
          <div class="text-xs font-black uppercase tracking-[0.22em] text-emerald-200">Calculated Result</div>
          <div class="mt-3 text-2xl font-black">{{ calibrateResultLabel }}</div>
          <div class="mt-3 text-sm text-white/75">{{ calibratePreviewMessage || 'พร้อมคำนวณเมื่อกรอกข้อมูลครบทั้ง 4 ช่อง' }}</div>
          <div class="mt-5 grid grid-cols-2 gap-3">
            <div class="rounded-[20px] bg-white/10 px-4 py-4">
              <div class="text-xs font-black uppercase tracking-[0.22em] text-white/60">Gain</div>
              <div class="mt-2 text-2xl font-black">{{ calibrateResultGain !== null ? calibrateResultGain.toFixed(5) : '-' }}</div>
            </div>
            <div class="rounded-[20px] bg-white/10 px-4 py-4">
              <div class="text-xs font-black uppercase tracking-[0.22em] text-white/60">Offset</div>
              <div class="mt-2 text-2xl font-black">{{ calibrateResultOffset !== null ? `${calibrateResultOffset >= 0 ? '+' : ''}${calibrateResultOffset.toFixed(5)}` : '-' }}</div>
            </div>
          </div>
        </UCard>
      </div>

      <div class="grid gap-6 xl:grid-cols-[minmax(0,1.2fr)_minmax(0,1fr)]">
        <UCard :ui="{ root: 'rounded-[24px] border-0 bg-white shadow-[0_18px_48px_rgba(15,23,42,0.08)]', body: 'p-6' }">
          <div class="grid gap-5 xl:grid-cols-[minmax(0,1fr)_260px]">
            <div class="min-w-0">
              <div class="text-xs font-black uppercase tracking-[0.22em] text-slate-500">Device Sync</div>
              <div class="mt-2 text-sm font-bold" :class="bleConnected ? 'text-emerald-700' : 'text-rose-600'">
                {{ calibrationBleStatusLabel }}
              </div>
              <div class="mt-2 text-sm font-semibold text-slate-600">{{ currentEspCalibrationLabel }}</div>
              <div class="mt-1 text-sm font-semibold text-slate-600">{{ savedDbCalibrationLabel }}</div>

              <div class="mt-4 grid gap-3 sm:grid-cols-3">
                <div class="rounded-[18px] border border-slate-200 bg-slate-50 px-4 py-3">
                  <div class="text-[11px] font-black uppercase tracking-[0.18em] text-slate-500">Device</div>
                  <div class="mt-1 break-all text-base font-black text-slate-950">
                    {{ bleMeterDeviceId || bleDeviceName }}
                  </div>
                </div>
                <div class="rounded-[18px] border border-slate-200 bg-slate-50 px-4 py-3">
                  <div class="text-[11px] font-black uppercase tracking-[0.18em] text-slate-500">Firmware</div>
                  <div class="mt-1 text-base font-black text-slate-950">
                    {{ bleMeterFirmwareVersion || '-' }}
                  </div>
                </div>
                <div class="rounded-[18px] border border-slate-200 bg-slate-50 px-4 py-3">
                  <div class="text-[11px] font-black uppercase tracking-[0.18em] text-slate-500">Last ESP Read</div>
                  <div class="mt-1 text-base font-black text-slate-950">
                    {{ lastEspVoltReading !== null ? `${lastEspVoltReading.toFixed(3)} V` : '-' }}
                  </div>
                </div>
              </div>
            </div>

            <div class="flex flex-wrap content-start justify-start gap-3 xl:justify-end">
              <UButton color="neutral" variant="soft" class="h-11 min-w-[200px] justify-center rounded-xl border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50 xl:min-w-[220px]" @click="connectBleVoltMeter">
                {{ bleConnected ? 'Disconnect BLE' : 'Connect BLE' }}
              </UButton>
              <UButton color="neutral" variant="soft" :loading="calibrateSyncing" :disabled="!bleConnected" class="h-11 min-w-[200px] justify-center rounded-xl border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-50 xl:min-w-[220px]" @click="syncCalibrationFromBle">
                Read BLE
              </UButton>
              <UButton color="neutral" variant="soft" class="h-11 min-w-[200px] justify-center rounded-xl border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50 xl:min-w-[220px]" @click="loadCalibrationFromDb">
                Load latest DB
              </UButton>
              <UButton color="neutral" variant="soft" :loading="calibrateResyncing" :disabled="!canResyncCalibrationToBle" class="h-11 min-w-[200px] justify-center rounded-xl border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-50 xl:min-w-[220px]" @click="syncSavedCalibrationToBle">
                Re-sync to device
              </UButton>
            </div>
          </div>
        </UCard>

        <UCard :ui="{ root: 'rounded-[24px] border-0 bg-white shadow-[0_18px_48px_rgba(15,23,42,0.08)]', body: 'p-6' }">
          <div class="flex items-center justify-between gap-4">
            <div>
              <div class="text-xs font-black uppercase tracking-[0.22em] text-slate-500">DB Helper</div>
              <div class="mt-2 text-sm leading-6 text-slate-600">
                ถ้ามี calibration ล่าสุดในฐานข้อมูลอยู่แล้ว สามารถโหลดกลับมาใส่ฟอร์ม แล้วกด sync เข้าอุปกรณ์ได้ทันที
              </div>
            </div>
            <UButton color="neutral" variant="soft" :disabled="!savedDbCalibration" class="h-11 rounded-xl border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-50" @click="applySavedCalibrationToForm">
              Use DB Values
            </UButton>
          </div>
        </UCard>
      </div>

      <div class="grid gap-6 lg:grid-cols-2">
        <UCard :ui="{ root: 'rounded-[24px] border-0 bg-white shadow-[0_18px_48px_rgba(15,23,42,0.08)]', body: 'p-6' }">
          <div class="flex items-center justify-between gap-4">
            <div class="text-2xl font-black text-slate-950">Point 1</div>
            <UButton color="neutral" variant="soft" class="h-11 rounded-xl border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50" @click="fillCalibrationEspInput(1)">
              Use Last ESP Read
            </UButton>
          </div>

          <div class="mt-6 grid gap-5 lg:grid-cols-2">
            <UFormField label="Meter Voltage" size="xl">
              <UInput v-model="calibratePointOneMeter" size="xl" class="w-full" :ui="{ base: 'h-14 rounded-full px-5 text-xl font-bold' }" @update:model-value="updateCalibrationPreview" />
            </UFormField>
            <UFormField label="ESP Volt Meter" size="xl">
              <UInput v-model="calibratePointOneEsp" size="xl" class="w-full" :ui="{ base: 'h-14 rounded-full px-5 text-xl font-bold' }" @update:model-value="updateCalibrationPreview" />
            </UFormField>
          </div>
        </UCard>

        <UCard :ui="{ root: 'rounded-[24px] border-0 bg-white shadow-[0_18px_48px_rgba(15,23,42,0.08)]', body: 'p-6' }">
          <div class="flex items-center justify-between gap-4">
            <div class="text-2xl font-black text-slate-950">Point 2</div>
            <UButton color="neutral" variant="soft" class="h-11 rounded-xl border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50" @click="fillCalibrationEspInput(2)">
              Use Last ESP Read
            </UButton>
          </div>

          <div class="mt-6 grid gap-5 lg:grid-cols-2">
            <UFormField label="Meter Voltage" size="xl">
              <UInput v-model="calibratePointTwoMeter" size="xl" class="w-full" :ui="{ base: 'h-14 rounded-full px-5 text-xl font-bold' }" @update:model-value="updateCalibrationPreview" />
            </UFormField>
            <UFormField label="ESP Volt Meter" size="xl">
              <UInput v-model="calibratePointTwoEsp" size="xl" class="w-full" :ui="{ base: 'h-14 rounded-full px-5 text-xl font-bold' }" @update:model-value="updateCalibrationPreview" />
            </UFormField>
          </div>
        </UCard>
      </div>

      <UCard :ui="{ root: 'rounded-[24px] border-0 bg-white shadow-[0_18px_48px_rgba(15,23,42,0.08)]', body: 'p-6' }">
        <div class="flex flex-col gap-4 lg:flex-row lg:items-center lg:justify-between">
          <div class="text-sm leading-7 text-slate-600">
            {{ actionMessage }}
          </div>
          <UButton color="neutral" variant="solid" :loading="calibrateSubmitting" class="h-12 rounded-xl bg-violet-700 px-5 text-sm font-black text-white hover:bg-violet-800 active:bg-violet-900" @click="submitCalibrationToDbAndBle">
            {{ calibrationSaveButtonLabel }}
          </UButton>
        </div>
      </UCard>
    </div>
  </main>
</template>
