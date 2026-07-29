<script setup lang="ts">
interface VoltMeterCalibrationRecord {
  id: string;
  deviceId: string;
  deviceName: string | null;
  source: CalibrationSource;
  inaGain: number;
  inaOffset: number;
  pointOneMeter: number;
  pointOneEsp: number;
  pointTwoMeter: number;
  pointTwoEsp: number;
  syncedAt: string | null;
  createdAt: string;
  updatedAt: string;
}

type CalibrationSource = "INA226" | "INA228";

useHead({
  title: "QC Calibration",
});

definePageMeta({
  middleware: "calibration-auth",
});

const bleConnected = ref(false);
const bleDeviceName = ref("BLE Volt Meter");
const actionMessage = ref(
  "สลับ Volt Meter เป็น Calibration Mode ผ่าน Serial แล้วเชื่อมต่อ BLE",
);
const calibrateSubmitting = ref(false);
const calibrateSyncing = ref(false);
const calibrateResyncing = ref(false);
const calibratePointOneMeter = ref("");
const calibratePointOneEsp = ref("");
const calibratePointTwoMeter = ref("");
const calibratePointTwoEsp = ref("");
const calibrateResultGain = ref<number | null>(null);
const calibrateResultOffset = ref<number | null>(null);
const calibratePreviewMessage = ref("");
const calibrationSource = ref<CalibrationSource>("INA226");
const currentEspInaGain = ref<number | null>(null);
const currentEspInaOffset = ref<number | null>(null);
const lastEspVoltReading = ref<number | null>(null);
const bleMeterDeviceId = ref("");
const bleMeterFirmwareVersion = ref("");
const savedDbCalibration = ref<VoltMeterCalibrationRecord | null>(null);

const BLE_SERVICE_UUID = "7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001";
const BLE_STATUS_CHARACTERISTIC_UUID = "7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001";
const BLE_CALIBRATION_CHARACTERISTIC_UUID =
  "7f9e0003-6a9d-4f7e-8d4d-32e7be6f1001";
const BLE_DEVICE_NAME_PREFIX = "PUMA-Voltmeter-";

let bleDevice: BluetoothDevice | null = null;
let bleStatusCharacteristic: BluetoothRemoteGATTCharacteristic | null = null;
let bleCalibrationCharacteristic: BluetoothRemoteGATTCharacteristic | null =
  null;

const currentEspCalibrationLabel = computed(() => {
  if (currentEspInaGain.value === null || currentEspInaOffset.value === null) {
    return "ยังไม่ได้อ่านค่าปัจจุบันจาก ESP";
  }

  return `${calibrationSource.value} current: gain ${currentEspInaGain.value.toFixed(5)} • offset ${currentEspInaOffset.value >= 0 ? "+" : ""}${currentEspInaOffset.value.toFixed(5)}`;
});

const calibrationSourceLabel = computed(() => calibrationSource.value);

const savedDbCalibrationLabel = computed(() => {
  if (!savedDbCalibration.value) {
    return "ยังไม่มี calibration ล่าสุดในฐานข้อมูลสำหรับเครื่องนี้";
  }

  return `DB ${savedDbCalibration.value.source}: gain ${savedDbCalibration.value.inaGain.toFixed(5)} • offset ${savedDbCalibration.value.inaOffset >= 0 ? "+" : ""}${savedDbCalibration.value.inaOffset.toFixed(5)}`;
});

const calibrationBleStatusLabel = computed(() => {
  if (bleConnected.value && bleStatusCharacteristic) {
    return bleDeviceName.value
      ? `Connected: ${bleDeviceName.value}`
      : "Connected to BLE Volt Meter";
  }

  return "ยังไม่ได้เชื่อมต่อ BLE สำหรับ calibration";
});

const calibrationSaveButtonLabel = computed(() =>
  bleConnected.value && bleCalibrationCharacteristic
    ? "Save To DB + Sync BLE"
    : "Save To DB",
);

const canResyncCalibrationToBle = computed(() =>
  Boolean(
    bleConnected.value &&
    bleCalibrationCharacteristic &&
    savedDbCalibration.value,
  ),
);

const calibrateResultLabel = computed(() => {
  if (
    calibrateResultGain.value === null ||
    calibrateResultOffset.value === null
  ) {
    return "กรอกค่าเทียบ 2 จุด เพื่อคำนวณ gain และ offset";
  }

  const gain = calibrateResultGain.value.toFixed(5);
  const offset =
    calibrateResultOffset.value >= 0
      ? `+${calibrateResultOffset.value.toFixed(5)}`
      : calibrateResultOffset.value.toFixed(5);

  return `Vcal = (Vesp x ${gain}) ${offset.startsWith("-") ? "-" : "+"} ${Math.abs(calibrateResultOffset.value).toFixed(5)}`;
});

function deriveDeviceIdFromBleName(name: string) {
  const trimmed = name.trim();
  if (!trimmed) {
    return "";
  }

  if (trimmed.startsWith(BLE_DEVICE_NAME_PREFIX)) {
    return `PUMA-VoltMeter-${trimmed.slice(BLE_DEVICE_NAME_PREFIX.length)}`;
  }

  return "";
}

function parseCalibrationNumber(value: string) {
  const parsed = Number.parseFloat(value.trim());
  return Number.isFinite(parsed) ? parsed : null;
}

function payloadNumber(payload: Record<string, any>, key: string) {
  const value = payload?.[key];
  return typeof value === "number" ? value : Number.parseFloat(String(value ?? ""));
}

function activeSourcePayloadKeys() {
  return calibrationSource.value === "INA228"
    ? { gain: "ina228_gain", offset: "ina228_offset" }
    : { gain: "ina226_gain", offset: "ina226_offset" };
}

function updateCalibrationPreview() {
  const meterOne = parseCalibrationNumber(calibratePointOneMeter.value);
  const espOne = parseCalibrationNumber(calibratePointOneEsp.value);
  const meterTwo = parseCalibrationNumber(calibratePointTwoMeter.value);
  const espTwo = parseCalibrationNumber(calibratePointTwoEsp.value);

  calibratePreviewMessage.value = "";
  calibrateResultGain.value = null;
  calibrateResultOffset.value = null;

  if ([meterOne, espOne, meterTwo, espTwo].some((value) => value === null)) {
    return;
  }

  if (espOne === espTwo) {
    calibratePreviewMessage.value =
      "ค่า ESP Volt Meter ทั้งสองจุดต้องไม่เท่ากัน";
    return;
  }

  const gain = (meterTwo! - meterOne!) / (espTwo! - espOne!);
  const offset = meterOne! - espOne! * gain;

  if (!Number.isFinite(gain) || !Number.isFinite(offset)) {
    calibratePreviewMessage.value = "ไม่สามารถคำนวณ gain / offset ได้";
    return;
  }

  calibrateResultGain.value = gain;
  calibrateResultOffset.value = offset;
  calibratePreviewMessage.value = `คำนวณแล้ว gain ${gain.toFixed(5)} และ offset ${offset >= 0 ? "+" : ""}${offset.toFixed(5)}`;
}

function fillCalibrationEspInput(point: 1 | 2) {
  if (lastEspVoltReading.value === null) {
    actionMessage.value = "ยังไม่มีค่า ESP Volt Meter ล่าสุดให้ดึงมาใช้";
    return;
  }

  if (point === 1) {
    calibratePointOneEsp.value = lastEspVoltReading.value.toFixed(3);
  } else {
    calibratePointTwoEsp.value = lastEspVoltReading.value.toFixed(3);
  }

  updateCalibrationPreview();
}

async function readBleStatusPayload() {
  if (!bleStatusCharacteristic) {
    return null;
  }

  const value = await bleStatusCharacteristic.readValue();
  const text = new TextDecoder().decode(value);
  return JSON.parse(text) as Record<string, any>;
}

async function syncCalibrationFromBle() {
  if (!bleConnected.value || !bleStatusCharacteristic) {
    actionMessage.value =
      "เชื่อมต่อ BLE Volt Meter ก่อนอ่าน calibration จากอุปกรณ์";
    return;
  }

  calibrateSyncing.value = true;

  try {
    const payload = await readBleStatusPayload();
    const deviceId = String(
      payload?.device_id ?? payload?.deviceId ?? "",
    ).trim();
    const firmwareVersion = String(
      payload?.fw ?? payload?.firmwareVersion ?? "",
    ).trim();
    const sourceKeys = activeSourcePayloadKeys();
    const selectedGain = payloadNumber(payload, sourceKeys.gain);
    const selectedOffset = payloadNumber(payload, sourceKeys.offset);
    // INA226 fields had no source suffix before the dual-monitor firmware.
    const inaGain = Number.isFinite(selectedGain)
      ? selectedGain
      : calibrationSource.value === "INA226"
        ? payloadNumber(payload, "ina_gain")
        : selectedGain;
    const inaOffset = Number.isFinite(selectedOffset)
      ? selectedOffset
      : calibrationSource.value === "INA226"
        ? payloadNumber(payload, "ina_offset")
        : selectedOffset;
    const voltage = payloadNumber(payload, "voltage");

    bleMeterDeviceId.value =
      deviceId || deriveDeviceIdFromBleName(bleDeviceName.value);
    bleMeterFirmwareVersion.value = firmwareVersion;
    currentEspInaGain.value = Number.isFinite(inaGain) ? inaGain : null;
    currentEspInaOffset.value = Number.isFinite(inaOffset) ? inaOffset : null;
    lastEspVoltReading.value = Number.isFinite(voltage)
      ? voltage
      : lastEspVoltReading.value;
    actionMessage.value = bleMeterDeviceId.value
      ? `อ่าน calibration ${calibrationSourceLabel.value} จาก ${bleMeterDeviceId.value} แล้ว`
      : `อ่าน calibration ${calibrationSourceLabel.value} จาก BLE Volt Meter แล้ว`;
  } catch (error) {
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "อ่านค่าคาลิเบรตจาก BLE ไม่สำเร็จ";
  } finally {
    calibrateSyncing.value = false;
  }
}

async function loadCalibrationFromDb() {
  const deviceId =
    bleMeterDeviceId.value || deriveDeviceIdFromBleName(bleDeviceName.value);
  if (!deviceId) {
    savedDbCalibration.value = null;
    actionMessage.value =
      "ยังไม่มี device ID จาก BLE ให้อ่าน calibration ในฐานข้อมูล";
    return;
  }

  try {
    const response = await $fetch<{
      ok: boolean;
      calibration: VoltMeterCalibrationRecord | null;
    }>("/api/voltmeter-calibrations/latest", {
      query: { deviceId, source: calibrationSource.value },
    });
    savedDbCalibration.value = response.calibration;
    actionMessage.value = response.calibration
      ? `โหลด calibration ${calibrationSourceLabel.value} ล่าสุดของ ${deviceId} จาก DB แล้ว`
      : `ยังไม่มี calibration ${calibrationSourceLabel.value} ของ ${deviceId} ใน DB`;
  } catch (error) {
    savedDbCalibration.value = null;
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "โหลด calibration จาก DB ไม่สำเร็จ";
  }
}

function applySavedCalibrationToForm() {
  if (!savedDbCalibration.value) {
    actionMessage.value = "ยังไม่มี calibration ใน DB ให้โหลด";
    return;
  }

  calibratePointOneMeter.value =
    savedDbCalibration.value.pointOneMeter.toFixed(3);
  calibratePointOneEsp.value = savedDbCalibration.value.pointOneEsp.toFixed(3);
  calibratePointTwoMeter.value =
    savedDbCalibration.value.pointTwoMeter.toFixed(3);
  calibratePointTwoEsp.value = savedDbCalibration.value.pointTwoEsp.toFixed(3);
  calibrateResultGain.value = savedDbCalibration.value.inaGain;
  calibrateResultOffset.value = savedDbCalibration.value.inaOffset;
  calibratePreviewMessage.value = `โหลด calibration ${savedDbCalibration.value.source} ล่าสุดจาก DB แล้ว (gain ${savedDbCalibration.value.inaGain.toFixed(5)}, offset ${savedDbCalibration.value.inaOffset >= 0 ? "+" : ""}${savedDbCalibration.value.inaOffset.toFixed(5)})`;
}

async function selectCalibrationSource(source: CalibrationSource) {
  if (calibrationSource.value === source) {
    return;
  }

  calibrationSource.value = source;
  calibratePointOneMeter.value = "";
  calibratePointOneEsp.value = "";
  calibratePointTwoMeter.value = "";
  calibratePointTwoEsp.value = "";
  calibrateResultGain.value = null;
  calibrateResultOffset.value = null;
  calibratePreviewMessage.value = "";

  if (bleConnected.value) {
    await syncCalibrationFromBle();
    await loadCalibrationFromDb();
  } else {
    savedDbCalibration.value = null;
    actionMessage.value = `เลือกตาราง calibration ของ ${source}; เชื่อมต่อ BLE เพื่ออ่านค่าและโหลดข้อมูลจาก DB`;
  }
}

async function syncSavedCalibrationToBle() {
  if (!savedDbCalibration.value) {
    actionMessage.value = "ยังไม่มี calibration ใน DB สำหรับ sync";
    return;
  }

  if (!bleConnected.value || !bleCalibrationCharacteristic) {
    actionMessage.value =
      "เชื่อมต่อ BLE Volt Meter ก่อน แล้วค่อย sync calibration";
    return;
  }

  calibrateResyncing.value = true;

  try {
    const sourceKeys = activeSourcePayloadKeys();
    const syncPayload = JSON.stringify({
      cmd: "set_calibration",
      [sourceKeys.gain]: savedDbCalibration.value.inaGain,
      [sourceKeys.offset]: savedDbCalibration.value.inaOffset,
    });

    await bleCalibrationCharacteristic.writeValue(
      new TextEncoder().encode(syncPayload),
    );

    const syncedAt = new Date().toISOString();
    const response = await $fetch<{
      ok: boolean;
      calibration: VoltMeterCalibrationRecord;
    }>("/api/voltmeter-calibrations", {
      method: "POST",
      body: {
        deviceId: savedDbCalibration.value.deviceId,
        deviceName: savedDbCalibration.value.deviceName,
        source: calibrationSource.value,
        inaGain: savedDbCalibration.value.inaGain,
        inaOffset: savedDbCalibration.value.inaOffset,
        pointOneMeter: savedDbCalibration.value.pointOneMeter,
        pointOneEsp: savedDbCalibration.value.pointOneEsp,
        pointTwoMeter: savedDbCalibration.value.pointTwoMeter,
        pointTwoEsp: savedDbCalibration.value.pointTwoEsp,
        syncedAt,
      },
    });

    savedDbCalibration.value = response.calibration;
    await syncCalibrationFromBle();
    actionMessage.value = `sync calibration ${calibrationSourceLabel.value} ล่าสุดจาก DB ไปที่ ${savedDbCalibration.value.deviceId} แล้ว`;
  } catch (error) {
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "sync calibration จาก DB ไป BLE ไม่สำเร็จ";
  } finally {
    calibrateResyncing.value = false;
  }
}

function handleBleDisconnect() {
  bleConnected.value = false;
  bleDeviceName.value = "BLE Volt Meter";
  bleDevice = null;
  bleStatusCharacteristic = null;
  bleCalibrationCharacteristic = null;
  bleMeterDeviceId.value = "";
  bleMeterFirmwareVersion.value = "";
  actionMessage.value = "BLE Volt Meter disconnected";
}

async function connectToBleVoltMeterDevice(
  device: BluetoothDevice,
  showConnectedMessage = true,
) {
  bleDevice = device;
  bleDeviceName.value = device.name || "BLE Volt Meter";
  bleDevice.addEventListener("gattserverdisconnected", handleBleDisconnect);

  const server = await bleDevice.gatt?.connect();
  if (!server) {
    throw new Error("BLE Volt Meter GATT connection failed");
  }

  const service = await server.getPrimaryService(BLE_SERVICE_UUID);
  bleStatusCharacteristic = await service.getCharacteristic(
    BLE_STATUS_CHARACTERISTIC_UUID,
  );
  bleCalibrationCharacteristic = await service.getCharacteristic(
    BLE_CALIBRATION_CHARACTERISTIC_UUID,
  );
  bleConnected.value = true;
  bleMeterDeviceId.value = deriveDeviceIdFromBleName(bleDeviceName.value);
  await syncCalibrationFromBle();
  await loadCalibrationFromDb();

  if (showConnectedMessage) {
    actionMessage.value = bleDeviceName.value.startsWith(BLE_DEVICE_NAME_PREFIX)
      ? `${bleDeviceName.value} connected`
      : `${bleDeviceName.value} connected (service matched)`;
  }
}

async function connectBleVoltMeter() {
  if (typeof window === "undefined") {
    return;
  }

  if (bleConnected.value && bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect();
    handleBleDisconnect();
    return;
  }

  const bluetooth = (navigator as any).bluetooth;
  if (!bluetooth?.requestDevice) {
    actionMessage.value = "Web Bluetooth is not supported in this browser";
    return;
  }

  try {
    actionMessage.value = "Select BLE Volt Meter in Calibration Mode";

    const device = await bluetooth.requestDevice({
      acceptAllDevices: true,
      optionalServices: [BLE_SERVICE_UUID],
    });

    await connectToBleVoltMeterDevice(device);
  } catch (error) {
    if (bleDevice?.gatt?.connected) {
      bleDevice.gatt.disconnect();
    }

    bleDevice = null;
    bleConnected.value = false;
    bleDeviceName.value = "BLE Volt Meter";
    actionMessage.value =
      error instanceof Error
        ? `${error.message}. ถ้าอุปกรณ์เคย pair แบบ HID keyboard ให้ Disconnect/Forget จาก Bluetooth ก่อน แล้วสั่ง calibrate ผ่าน Serial ใหม่`
        : "Unable to connect BLE Volt Meter";
  }
}

async function submitCalibrationToDbAndBle() {
  updateCalibrationPreview();

  if (
    calibrateResultGain.value === null ||
    calibrateResultOffset.value === null
  ) {
    actionMessage.value =
      calibratePreviewMessage.value || "คำนวณ gain / offset ยังไม่สำเร็จ";
    return;
  }

  const deviceId =
    bleMeterDeviceId.value || deriveDeviceIdFromBleName(bleDeviceName.value);
  if (!deviceId) {
    actionMessage.value =
      "เชื่อมต่อ BLE Volt Meter ก่อน เพื่อระบุ device ID สำหรับบันทึก calibration";
    return;
  }

  calibrateSubmitting.value = true;

  try {
    const payload = {
      deviceId,
      deviceName: bleDeviceName.value || null,
      source: calibrationSource.value,
      inaGain: Number(calibrateResultGain.value.toFixed(5)),
      inaOffset: Number(calibrateResultOffset.value.toFixed(5)),
      pointOneMeter: Number(
        parseCalibrationNumber(calibratePointOneMeter.value)?.toFixed(3),
      ),
      pointOneEsp: Number(
        parseCalibrationNumber(calibratePointOneEsp.value)?.toFixed(3),
      ),
      pointTwoMeter: Number(
        parseCalibrationNumber(calibratePointTwoMeter.value)?.toFixed(3),
      ),
      pointTwoEsp: Number(
        parseCalibrationNumber(calibratePointTwoEsp.value)?.toFixed(3),
      ),
      syncedAt: null as string | null,
    };

    const savedResponse = await $fetch<{
      ok: boolean;
      calibration: VoltMeterCalibrationRecord;
    }>("/api/voltmeter-calibrations", {
      method: "POST",
      body: payload,
    });

    savedDbCalibration.value = savedResponse.calibration;

    if (bleCalibrationCharacteristic) {
      const sourceKeys = activeSourcePayloadKeys();
      const syncPayload = JSON.stringify({
        cmd: "set_calibration",
        [sourceKeys.gain]: payload.inaGain,
        [sourceKeys.offset]: payload.inaOffset,
      });

      await bleCalibrationCharacteristic.writeValue(
        new TextEncoder().encode(syncPayload),
      );

      const syncedAt = new Date().toISOString();
      const syncedResponse = await $fetch<{
        ok: boolean;
        calibration: VoltMeterCalibrationRecord;
      }>("/api/voltmeter-calibrations", {
        method: "POST",
        body: {
          ...payload,
          syncedAt,
        },
      });

      savedDbCalibration.value = syncedResponse.calibration;
      await syncCalibrationFromBle();
      actionMessage.value = `บันทึก calibration ${calibrationSourceLabel.value} ลง DB และ sync ไป ${deviceId} แล้ว`;
    } else {
      actionMessage.value = `บันทึก calibration ${calibrationSourceLabel.value} ลง DB สำหรับ ${deviceId} แล้ว ยังไม่ได้ sync BLE`;
    }

    currentEspInaGain.value = calibrateResultGain.value;
    currentEspInaOffset.value = calibrateResultOffset.value;
  } catch (error) {
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "บันทึกหรือ sync calibration ไม่สำเร็จ";
  } finally {
    calibrateSubmitting.value = false;
  }
}
</script>

<template>
  <main
    class="min-h-screen bg-[radial-gradient(circle_at_top_left,_rgba(14,165,233,0.16),_transparent_32%),linear-gradient(180deg,_#dbe7f2_0%,_#edf3f8_52%,_#d5e1eb_100%)] px-4 py-5 text-slate-950 sm:px-6 lg:px-8"
  >
    <div class="mx-auto max-w-7xl space-y-4">
      <UCard
        :ui="{
          root: 'rounded-[15px] border border-slate-800 bg-[linear-gradient(115deg,_#0f2742_0%,_#12395a_58%,_#0b1f36_100%)] shadow-[0_20px_48px_rgba(15,23,42,0.2)]',
          body: 'p-5 sm:p-6',
        }"
      >
        <div class="flex flex-wrap items-center justify-between gap-5">
          <div class="min-w-0">
            <div
              class="text-xs font-black uppercase tracking-[0.24em] text-sky-200"
            >
              QC Calibration
            </div>
            <h1
              class="mt-2 text-3xl font-black tracking-tight text-white sm:text-4xl"
            >
              Volt Meter Calibration
            </h1>
            <p
              class="mt-2 max-w-2xl text-sm font-semibold leading-6 text-slate-200"
            >
              เทียบค่าอ้างอิง 2 จุด คำนวณ gain / offset แยกตามเซนเซอร์ และ sync กลับเข้า
              Volt Meter
            </p>
          </div>
          <div class="flex items-center gap-3">
            <img
              src="/branding/logo-puma-battery.png"
              alt="PUMA Battery"
              class="h-12 w-auto object-contain sm:h-14"
            />
            <UButton
              to="/"
              color="neutral"
              variant="soft"
              class="rounded-full border border-white/25 bg-white/10 px-4 text-sm font-black text-white hover:bg-white/20"
              >Home</UButton
            >
          </div>
        </div>
      </UCard>

      <UAlert
        color="info"
        variant="soft"
        icon="i-lucide-info"
        title="Calibration status"
        :description="actionMessage"
        :ui="{
          root: 'rounded-[15px] border border-sky-300 bg-sky-50 text-slate-950',
          title: 'font-black',
          description: 'font-semibold text-slate-700',
        }"
      />

      <UCard
        :ui="{
          root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm',
          body: 'p-4 sm:p-5',
        }"
      >
        <div class="flex flex-wrap items-center justify-between gap-3">
          <div>
            <p class="text-xs font-black uppercase tracking-[0.2em] text-sky-700">
              Calibration table
            </p>
            <h2 class="mt-1 text-lg font-black text-slate-950">
              เลือกเซนเซอร์ที่จะคาลิเบรต
            </h2>
            <p class="mt-1 text-sm font-semibold text-slate-600">
              แต่ละเซนเซอร์มีค่า gain, offset และจุดอ้างอิงของตัวเอง
            </p>
          </div>
          <div class="grid grid-cols-2 gap-2">
            <UButton
              :color="calibrationSource === 'INA226' ? 'primary' : 'neutral'"
              :variant="calibrationSource === 'INA226' ? 'solid' : 'outline'"
              class="justify-center rounded-full px-5 font-black"
              @click="selectCalibrationSource('INA226')"
              >INA226 · 0x40</UButton
            >
            <UButton
              :color="calibrationSource === 'INA228' ? 'primary' : 'neutral'"
              :variant="calibrationSource === 'INA228' ? 'solid' : 'outline'"
              class="justify-center rounded-full px-5 font-black"
              @click="selectCalibrationSource('INA228')"
              >INA228 · 0x44</UButton
            >
          </div>
        </div>
      </UCard>

      <section class="grid gap-4 xl:grid-cols-3">
        <UCard
          :ui="{
            root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm',
            body: 'p-5',
          }"
        >
          <div class="flex items-center gap-3">
            <UBadge
              color="primary"
              variant="solid"
              class="rounded-full px-3 py-1 font-black"
              >1</UBadge
            >
            <h2 class="font-black text-slate-950">เข้า Calibration Mode</h2>
          </div>
          <p class="mt-4 text-sm font-semibold leading-6 text-slate-700">
            เชื่อมต่อ USB Serial แล้วส่งคำสั่งต่อไปนี้
            เครื่องจะจำโหมดและรีสตาร์ตเข้า BLE Calibration Mode. หลังเชื่อมต่อแล้ว
            เลือก INA226 หรือ INA228 บนหน้านี้ก่อนอ่านและบันทึกค่า
          </p>
          <code
            class="mt-3 block rounded-[8px] bg-slate-950 px-4 py-3 text-sm font-black text-emerald-300"
            >mode cal</code
          >
          <p class="mt-3 text-xs leading-5 text-slate-600">
            ใช้
            <code class="font-bold text-slate-900">mode calibrate</code> ได้เช่นกัน.
            คำสั่งนี้ไม่เลือกเซนเซอร์ให้เอง; ปุ่ม BOOT ใช้เป็นทางเลือกสำรองเฉพาะตอนเปิดเครื่อง ไม่ใช่ขั้นตอนปกติ.
          </p>
        </UCard>

        <UCard
          :ui="{
            root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm',
            body: 'p-5',
          }"
        >
          <div class="flex items-center gap-3">
            <UBadge
              color="primary"
              variant="solid"
              class="rounded-full px-3 py-1 font-black"
              >2</UBadge
            >
            <h2 class="font-black text-slate-950">เชื่อมและอ่านอุปกรณ์</h2>
          </div>
          <p
            class="mt-4 text-sm font-bold"
            :class="bleConnected ? 'text-emerald-700' : 'text-slate-700'"
          >
            {{ calibrationBleStatusLabel }}
          </p>
          <div class="mt-4 flex flex-wrap gap-2">
            <UButton
              color="primary"
              class="rounded-full px-4 font-black"
              @click="connectBleVoltMeter"
              >{{ bleConnected ? "Disconnect BLE" : "Connect BLE" }}</UButton
            >
            <UButton
              color="neutral"
              variant="outline"
              :loading="calibrateSyncing"
              :disabled="!bleConnected"
              class="rounded-full px-4 font-black"
              @click="syncCalibrationFromBle"
              >Read BLE</UButton
            >
          </div>
          <p class="mt-3 text-xs leading-5 text-slate-600">
            หากเคย pair เป็น HID keyboard ให้ Disconnect หรือ Forget ก่อน
            แล้วจึงสั่ง
            <code class="font-bold text-slate-900">mode cal</code> ใหม่.
          </p>
        </UCard>

        <UCard
          :ui="{
            root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm',
            body: 'p-5',
          }"
        >
          <div class="flex items-center gap-3">
            <UBadge
              color="primary"
              variant="solid"
              class="rounded-full px-3 py-1 font-black"
              >3</UBadge
            >
            <h2 class="font-black text-slate-950">ค่าเดิมและ Device</h2>
          </div>
          <div class="mt-4 grid grid-cols-3 gap-2 text-xs">
            <div class="rounded-[8px] bg-slate-100 p-3">
              <div class="font-bold text-slate-500">Device</div>
              <div class="mt-1 break-all font-black text-slate-950">
                {{ bleMeterDeviceId || "-" }}
              </div>
            </div>
            <div class="rounded-[8px] bg-slate-100 p-3">
              <div class="font-bold text-slate-500">Firmware</div>
              <div class="mt-1 font-black text-slate-950">
                {{ bleMeterFirmwareVersion || "-" }}
              </div>
            </div>
            <div class="rounded-[8px] bg-slate-100 p-3">
              <div class="font-bold text-slate-500">ESP Read</div>
              <div class="mt-1 font-black text-slate-950">
                {{
                  lastEspVoltReading !== null
                    ? `${lastEspVoltReading.toFixed(3)} V`
                    : "-"
                }}
              </div>
            </div>
          </div>
          <div class="mt-3 flex flex-wrap gap-2">
            <UButton
              color="neutral"
              variant="outline"
              class="rounded-full px-4 text-xs font-black"
              @click="loadCalibrationFromDb"
              >Load latest DB</UButton
            ><UButton
              color="neutral"
              variant="outline"
              :disabled="!savedDbCalibration"
              class="rounded-full px-4 text-xs font-black"
              @click="applySavedCalibrationToForm"
              >Use DB values</UButton
            ><UButton
              color="neutral"
              variant="outline"
              :loading="calibrateResyncing"
              :disabled="!canResyncCalibrationToBle"
              class="rounded-full px-4 text-xs font-black"
              @click="syncSavedCalibrationToBle"
              >Re-sync device</UButton
            >
          </div>
        </UCard>
      </section>

      <section class="grid items-stretch gap-4 xl:grid-cols-2">
        <UCard
          :ui="{
            root: 'h-full rounded-[15px] border border-slate-200 bg-white shadow-sm',
            body: 'p-5',
          }"
        >
          <div class="flex flex-wrap items-start justify-between gap-3">
            <div>
              <p
                class="text-xs font-black uppercase tracking-[0.2em] text-sky-700"
              >
                4. Reference points
              </p>
              <h2 class="mt-1 text-2xl font-black text-slate-950">
                บันทึกค่าเทียบ 2 จุด
              </h2>
              <p class="mt-1 text-sm font-semibold text-slate-600">
                Meter Voltage คือค่าอ้างอิงจากมิเตอร์มาตรฐาน. ESP Volt Meter
                คือค่าที่อ่านจากอุปกรณ์.
              </p>
            </div>
            <p class="text-sm font-semibold text-slate-600">
              {{ currentEspCalibrationLabel }}
            </p>
          </div>
          <div class="mt-5 grid gap-4 lg:grid-cols-2">
            <div class="rounded-[8px] border border-slate-200 bg-slate-50 p-4">
              <div class="flex items-center justify-between gap-3">
                <h3 class="font-black text-slate-950">Point 1</h3>
                <UButton
                  color="neutral"
                  variant="outline"
                  size="xs"
                  class="rounded-full font-black"
                  @click="fillCalibrationEspInput(1)"
                  >Use last ESP read</UButton
                >
              </div>
              <div class="mt-4 grid gap-3 sm:grid-cols-2">
                <UFormField label="Meter Voltage (V)" required
                  ><UInput
                    v-model="calibratePointOneMeter"
                    placeholder="เช่น 12.600"
                    variant="outline"
                    inputmode="decimal"
                    :ui="{ base: 'rounded-[8px] font-bold' }"
                    @update:model-value="
                      updateCalibrationPreview
                    " /></UFormField
                ><UFormField label="ESP Volt Meter (V)" required
                  ><UInput
                    v-model="calibratePointOneEsp"
                    placeholder="เช่น 12.540"
                    variant="outline"
                    inputmode="decimal"
                    :ui="{ base: 'rounded-[8px] font-bold' }"
                    @update:model-value="updateCalibrationPreview"
                /></UFormField>
              </div>
            </div>
            <div class="rounded-[8px] border border-slate-200 bg-slate-50 p-4">
              <div class="flex items-center justify-between gap-3">
                <h3 class="font-black text-slate-950">Point 2</h3>
                <UButton
                  color="neutral"
                  variant="outline"
                  size="xs"
                  class="rounded-full font-black"
                  @click="fillCalibrationEspInput(2)"
                  >Use last ESP read</UButton
                >
              </div>
              <div class="mt-4 grid gap-3 sm:grid-cols-2">
                <UFormField label="Meter Voltage (V)" required
                  ><UInput
                    v-model="calibratePointTwoMeter"
                    placeholder="เช่น 14.200"
                    variant="outline"
                    inputmode="decimal"
                    :ui="{ base: 'rounded-[8px] font-bold' }"
                    @update:model-value="
                      updateCalibrationPreview
                    " /></UFormField
                ><UFormField label="ESP Volt Meter (V)" required
                  ><UInput
                    v-model="calibratePointTwoEsp"
                    placeholder="เช่น 14.110"
                    variant="outline"
                    inputmode="decimal"
                    :ui="{ base: 'rounded-[8px] font-bold' }"
                    @update:model-value="updateCalibrationPreview"
                /></UFormField>
              </div>
            </div>
          </div>
        </UCard>

        <UCard
          :ui="{
            root: 'h-full rounded-[15px] border border-slate-800 bg-slate-950 shadow-[0_16px_40px_rgba(15,23,42,0.2)]',
            body: 'flex h-full flex-col p-5',
          }"
        >
          <p
            class="text-xs font-black uppercase tracking-[0.2em] text-emerald-300"
          >
            Calculated result
          </p>
          <h2 class="mt-2 text-xl font-black leading-7 text-white">
            {{ calibrateResultLabel }}
          </h2>
          <p class="mt-3 text-sm font-semibold leading-6 text-slate-300">
            {{
              calibratePreviewMessage || "กรอกข้อมูลให้ครบ 4 ช่องเพื่อคำนวณผล"
            }}
          </p>
          <div class="mt-5 grid grid-cols-2 gap-3">
            <div class="rounded-[8px] bg-white/10 p-4">
              <div
                class="text-xs font-black uppercase tracking-[0.18em] text-slate-300"
              >
                Gain
              </div>
              <div class="mt-2 text-2xl font-black text-white">
                {{
                  calibrateResultGain !== null
                    ? calibrateResultGain.toFixed(5)
                    : "-"
                }}
              </div>
            </div>
            <div class="rounded-[8px] bg-white/10 p-4">
              <div
                class="text-xs font-black uppercase tracking-[0.18em] text-slate-300"
              >
                Offset
              </div>
              <div class="mt-2 text-2xl font-black text-white">
                {{
                  calibrateResultOffset !== null
                    ? `${calibrateResultOffset >= 0 ? "+" : ""}${calibrateResultOffset.toFixed(5)}`
                    : "-"
                }}
              </div>
            </div>
          </div>
          <div class="mt-auto border-t border-white/15 pt-5">
            <p class="text-xs font-semibold leading-5 text-slate-300">
              {{ savedDbCalibrationLabel }}
            </p>
            <UButton
              color="primary"
              variant="solid"
              :loading="calibrateSubmitting"
              class="mt-4 w-full justify-center rounded-full py-3 font-black"
              @click="submitCalibrationToDbAndBle"
              >{{ calibrationSaveButtonLabel }}</UButton
            >
          </div>
        </UCard>
      </section>
    </div>
  </main>
</template>
