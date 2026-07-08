<script setup lang="ts">
type JobPhase = 'BEFORE_CHARGE' | 'AFTER_CHARGE' | 'DELIVERY'
type VoltageUnit = 'V' | 'MV'
type ScanTarget = 'pallet' | 'battery'

interface EmployeeItem {
  id: string
  code: string
  name: string
}

interface SalesOrderItem {
  id: string
  soNumber: string
  description: string | null
}

interface BatteryJobSlot {
  id: string
  slotNumber: number
  batteryId: string
  beforeVoltage: number | null
  beforeVoltageMv: number | null
  beforeMeasuredAt: string | null
  afterVoltage: number | null
  afterVoltageMv: number | null
  afterMeasuredAt: string | null
  deliveryVoltage: number | null
  deliveryVoltageMv: number | null
  deliveryMeasuredAt: string | null
}

interface BatteryJobRecord {
  id: string
  phase: JobPhase
  status: 'OPEN' | 'BEFORE_CHARGE_COMPLETED' | 'AFTER_CHARGE_COMPLETED' | 'READY_FOR_DELIVERY'
  palletId: string
  workStartedAt: string
  operatorId: string | null
  operatorName: string | null
  salesOrderId: string | null
  salesOrderNumber: string | null
  beforeChargeCompletedAt: string | null
  afterChargeCompletedAt: string | null
  deliveryCompletedAt: string | null
  lockedAt: string | null
  slots: BatteryJobSlot[]
}

interface ScanDecisionResponse {
  ok: boolean
  palletId: string
  found: boolean
  action: 'LOAD_EXISTING' | 'OPEN_NEW_BEFORE_CHARGE'
  recommendedPhase: JobPhase
  reason: 'NO_JOB_FOUND' | 'ALL_PHASES_COMPLETED' | 'JOB_EXPIRED' | 'CONTINUE_EXISTING'
  job: BatteryJobRecord | null
}

const phase = ref<JobPhase>('BEFORE_CHARGE')
const currentJob = ref<BatteryJobRecord | null>(null)
const employees = ref<EmployeeItem[]>([])
const salesOrders = ref<SalesOrderItem[]>([])
const selectedSlotNumber = ref(1)
const workStartedAt = ref(toDateTimeLocalValue(new Date()))
const operatorId = ref('')
const salesOrderId = ref('')
const palletId = ref('')
const batteryScanInput = ref('')
const voltageInput = ref('')
const voltageUnit = ref<VoltageUnit>('V')
const newEmployeeCode = ref('')
const newEmployeeName = ref('')
const newSoNumber = ref('')
const newSoDescription = ref('')
const detailModalOpen = ref(false)
const scanTarget = ref<ScanTarget>('pallet')
const qrScannerConnected = ref(false)
const qrScannerDeviceName = ref('BLE QR Reader')
const qrReaderStatus = ref('Scanner ready for pallet input')
const bleConnected = ref(false)
const bleDeviceName = ref('BLE Volt Meter')
const isBusy = ref(false)
const isSavingBattery = ref(false)
const isMeasuring = ref(false)
const isConfirming = ref(false)
const loadError = ref('')
const actionMessage = ref('เปิดใบงานหรือเปิด pallet เพื่อเริ่มงานวัดแรงดัน battery')
const measurementPopupOpen = ref(false)
const measurementPopupValue = ref('')
const measurementPopupUnit = ref<'V' | 'MV'>('V')
const workflowActionMode = ref<'battery' | 'voltage'>('battery')

const BLE_SERVICE_UUID = '7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001'
const BLE_DEVICE_NAME_PREFIX = 'PUMA-Voltmeter-'
const QR_SCANNER_PROFILES = [
  {
    service: '6e400001-b5a3-f393-e0a9-e50e24dcca9e',
    characteristic: '6e400003-b5a3-f393-e0a9-e50e24dcca9e',
  },
  {
    service: '0000ffe0-0000-1000-8000-00805f9b34fb',
    characteristic: '0000ffe1-0000-1000-8000-00805f9b34fb',
  },
] as const

const palletInputRef = useTemplateRef<HTMLInputElement>('palletInput')
const batteryInputRef = useTemplateRef<HTMLInputElement>('batteryInput')

let bleDevice: any = null
let qrScannerDevice: BluetoothDevice | null = null
let qrScannerCharacteristic: BluetoothRemoteGATTCharacteristic | null = null
let qrScannerFlushTimer: ReturnType<typeof setTimeout> | null = null
let qrScannerBuffer = ''
let measurementPopupTimer: ReturnType<typeof setTimeout> | null = null
const textDecoder = new TextDecoder()

const phaseOptions = [
  {
    value: 'BEFORE_CHARGE' as const,
    label: 'วัดก่อนชาร์จ',
    title: 'Before Charge',
    detail: 'เปิดใบงานใหม่, scan pallet, scan battery 1-21 และวัดแรงดันก่อนชาร์จ',
    tone: 'bg-lime-700 text-white hover:bg-lime-800 active:bg-lime-900',
    softTone: 'border border-lime-300 bg-lime-100 text-lime-950',
  },
  {
    value: 'AFTER_CHARGE' as const,
    label: 'วัดหลังชาร์จ',
    title: 'After Charge',
    detail: 'เปิด pallet เดิมและบันทึกแรงดันหลังชาร์จตามตำแหน่งเดิม',
    tone: 'bg-sky-700 text-white hover:bg-sky-800 active:bg-sky-900',
    softTone: 'border border-sky-300 bg-sky-100 text-sky-950',
  },
  {
    value: 'DELIVERY' as const,
    label: 'วัด Delivery',
    title: 'Delivery',
    detail: 'ตรวจวัดก่อนส่งมอบและ lock ค่าเมื่อยืนยันพร้อมส่ง',
    tone: 'bg-amber-700 text-white hover:bg-amber-800 active:bg-amber-900',
    softTone: 'border border-amber-300 bg-amber-100 text-amber-950',
  },
] as const

const currentPhaseMeta = computed(() => phaseOptions.find(option => option.value === phase.value) ?? phaseOptions[0])
const slotRows = computed(() => {
  const slots = currentJob.value?.slots ?? Array.from({ length: 21 }, (_, index) => ({
    id: `preview-${index + 1}`,
    slotNumber: index + 1,
    batteryId: '',
    beforeVoltage: null,
    beforeVoltageMv: null,
    beforeMeasuredAt: null,
    afterVoltage: null,
    afterVoltageMv: null,
    afterMeasuredAt: null,
    deliveryVoltage: null,
    deliveryVoltageMv: null,
    deliveryMeasuredAt: null,
  }))

  return [
    slots.slice(0, 7),
    slots.slice(7, 14),
    slots.slice(14, 21),
  ]
})

const selectedSlot = computed(() => currentJob.value?.slots.find(slot => slot.slotNumber === selectedSlotNumber.value) ?? null)
const selectedPhaseMeasuredAt = computed(() => selectedSlot.value ? getPhaseMeasuredAt(selectedSlot.value, phase.value) : null)
const selectedPhaseVoltageLabel = computed(() => selectedSlot.value ? getSlotVoltageLabel(selectedSlot.value, phase.value) : '-')
const hasJob = computed(() => currentJob.value !== null)
const firstIncompleteSlotNumber = computed(() => {
  if (!currentJob.value) {
    return 1
  }

  const slots = currentJob.value?.slots ?? []
  const firstIncomplete = slots.find(slot => !getPhaseVoltage(slot, phase.value))
  return firstIncomplete?.slotNumber ?? 1
})
const progressCount = computed(() => {
  const slots = currentJob.value?.slots ?? []
  return slots.filter(slot => getPhaseVoltage(slot, phase.value) !== null).length
})
const canConfirmPhase = computed(() => currentJob.value !== null && progressCount.value === 21)
const jobStatusLabel = computed(() => {
  if (!currentJob.value) {
    return 'No job opened'
  }

  if (currentJob.value.status === 'READY_FOR_DELIVERY') {
    return 'Ready for Delivery'
  }

  if (currentJob.value.status === 'AFTER_CHARGE_COMPLETED') {
    return 'After Charge Completed'
  }

  if (currentJob.value.status === 'BEFORE_CHARGE_COMPLETED') {
    return 'Before Charge Completed'
  }

  return 'Open'
})
const openButtonLabel = computed(() => phase.value === 'BEFORE_CHARGE' ? 'เปิดใบงาน' : 'เปิด pallet')
const qrScannerLabel = computed(() => qrScannerConnected.value ? qrScannerDeviceName.value : 'Connect QR Reader')
const bleStatusLabel = computed(() => bleConnected.value ? bleDeviceName.value : 'Connect BLE Volt Meter')
const qrScannerStatusLabel = computed(() => qrScannerConnected.value ? `QR Reader Connected: ${qrScannerDeviceName.value}` : 'QR Reader Not Connected')
const bleMeterStatusLabel = computed(() => bleConnected.value ? `BLE Volt Meter Connected: ${bleDeviceName.value}` : 'BLE Volt Meter Not Connected')
const activeSlotCardClass = computed(() => {
  if (phase.value === 'BEFORE_CHARGE') {
    return 'border-lime-400 bg-lime-50 text-lime-950'
  }

  if (phase.value === 'AFTER_CHARGE') {
    return 'border-sky-400 bg-sky-50 text-sky-950'
  }

  return 'border-amber-400 bg-amber-50 text-amber-950'
})
const workflowActionActiveClass = computed(() => {
  if (phase.value === 'BEFORE_CHARGE') {
    return 'bg-emerald-700 text-white hover:bg-emerald-800 active:bg-emerald-950'
  }

  if (phase.value === 'AFTER_CHARGE') {
    return 'bg-sky-700 text-white hover:bg-sky-800 active:bg-sky-950'
  }

  return 'bg-amber-600 text-white hover:bg-amber-700 active:bg-amber-900'
})

const workflowActionIdleClass = computed(() => {
  if (phase.value === 'BEFORE_CHARGE') {
    return 'border border-emerald-400 bg-emerald-50 text-emerald-950 hover:bg-emerald-200 active:bg-emerald-300'
  }

  if (phase.value === 'AFTER_CHARGE') {
    return 'border border-sky-400 bg-sky-50 text-sky-950 hover:bg-sky-200 active:bg-sky-300'
  }

  return 'border border-amber-400 bg-amber-50 text-amber-950 hover:bg-amber-200 active:bg-amber-300'
})
const isBatteryIdWorkflowEnabled = computed(() => phase.value === 'BEFORE_CHARGE')
const phasePrimaryButtonClass = computed(() => {
  if (phase.value === 'BEFORE_CHARGE') {
    return 'bg-lime-50 text-lime-950 border border-lime-400 hover:bg-lime-200 active:bg-lime-300'
  }

  if (phase.value === 'AFTER_CHARGE') {
    return 'bg-sky-50 text-sky-950 border border-sky-400 hover:bg-sky-200 active:bg-sky-300'
  }

  return 'bg-amber-50 text-amber-950 border border-amber-400 hover:bg-amber-200 active:bg-amber-300'
})

const phaseSoftButtonClass = computed(() => {
  if (phase.value === 'BEFORE_CHARGE') {
    return 'border border-lime-400 bg-lime-50 text-lime-950 hover:bg-lime-200 active:bg-lime-300'
  }

  if (phase.value === 'AFTER_CHARGE') {
    return 'border border-sky-400 bg-sky-50 text-sky-950 hover:bg-sky-200 active:bg-sky-300'
  }

  return 'border border-amber-400 bg-amber-50 text-amber-950 hover:bg-amber-200 active:bg-amber-300'
})

function toDateTimeLocalValue(date: Date) {
  const year = date.getFullYear()
  const month = `${date.getMonth() + 1}`.padStart(2, '0')
  const day = `${date.getDate()}`.padStart(2, '0')
  const hours = `${date.getHours()}`.padStart(2, '0')
  const minutes = `${date.getMinutes()}`.padStart(2, '0')
  return `${year}-${month}-${day}T${hours}:${minutes}`
}

function getDefaultWorkflowActionMode(targetPhase: JobPhase) {
  return targetPhase === 'BEFORE_CHARGE' ? 'battery' : 'voltage'
}

function resetCurrentJobState() {
  currentJob.value = null
  selectedSlotNumber.value = 1
  batteryScanInput.value = ''
  voltageInput.value = ''
  detailModalOpen.value = false
}

function clearQrScannerFlushTimer() {
  if (qrScannerFlushTimer) {
    clearTimeout(qrScannerFlushTimer)
    qrScannerFlushTimer = null
  }
}

function clearMeasurementPopupTimer() {
  if (measurementPopupTimer) {
    clearTimeout(measurementPopupTimer)
    measurementPopupTimer = null
  }
}

function showMeasurementPopup(voltage: number) {
  clearMeasurementPopupTimer()
  measurementPopupValue.value = voltage.toFixed(3)
  measurementPopupUnit.value = 'V'
  measurementPopupOpen.value = true
  measurementPopupTimer = setTimeout(() => {
    measurementPopupOpen.value = false
  }, 2000)
}

function wait(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms))
}

async function routeScannedCode(normalized: string) {
  if (scanTarget.value === 'pallet') {
    await handlePalletInput(normalized)
    return
  }

  await handleBatteryInput(normalized)
}

function flushQrScannerBuffer() {
  const normalized = qrScannerBuffer.trim().toUpperCase()
  qrScannerBuffer = ''
  clearQrScannerFlushTimer()

  if (!normalized) {
    return
  }

  void routeScannedCode(normalized)
}

function scheduleQrScannerFlush() {
  clearQrScannerFlushTimer()
  qrScannerFlushTimer = setTimeout(() => {
    flushQrScannerBuffer()
  }, 160)
}

function handleQrScannerChunk(rawChunk: string) {
  const normalizedChunk = rawChunk.replace(/\0/g, '')

  if (!normalizedChunk) {
    return
  }

  qrScannerBuffer += normalizedChunk

  const lines = qrScannerBuffer.split(/\r?\n/)
  qrScannerBuffer = lines.pop() ?? ''

  for (const line of lines) {
    const normalized = line.trim().toUpperCase()
    if (!normalized) {
      continue
    }

    clearQrScannerFlushTimer()
    void routeScannedCode(normalized)
  }

  if (qrScannerBuffer.trim()) {
    scheduleQrScannerFlush()
  }
}

function applyScannedValue(value: string) {
  const normalized = value.trim().toUpperCase()

  if (!normalized) {
    return
  }

  if (scanTarget.value === 'pallet') {
    palletId.value = normalized
    workStartedAt.value = toDateTimeLocalValue(new Date())
    qrReaderStatus.value = `Scanned pallet ${normalized}`
    actionMessage.value = `สแกน pallet ${normalized} แล้ว และตั้งเวลาเริ่มงานให้อัตโนมัติ`
    return
  }

  batteryScanInput.value = normalized
  qrReaderStatus.value = `Scanned battery ${normalized}`
}

async function armQrScanner(target: ScanTarget) {
  scanTarget.value = target
  qrReaderStatus.value = target === 'pallet'
    ? 'QR Reader ready for pallet input'
    : 'QR Reader ready for battery input'
  actionMessage.value = target === 'pallet'
    ? 'พร้อมรับค่า pallet จาก QR Reader แล้ว'
    : 'พร้อมรับค่า Battery ID จาก QR Reader แล้ว'

  await nextTick()

  const targetInput = target === 'pallet' ? palletInputRef.value : batteryInputRef.value
  targetInput?.focus()
  targetInput?.select()
}

function handleQrScannerDisconnect() {
  clearQrScannerFlushTimer()
  qrScannerBuffer = ''

  if (qrScannerCharacteristic) {
    try {
      qrScannerCharacteristic.removeEventListener('characteristicvaluechanged', handleQrScannerNotification)
    }
    catch {
      // noop
    }
  }

  qrScannerCharacteristic = null
  qrScannerDevice = null
  qrScannerConnected.value = false
  qrScannerDeviceName.value = 'BLE QR Reader'
  qrReaderStatus.value = 'QR Reader disconnected'
  actionMessage.value = 'QR Reader disconnected'
}

function disconnectQrScanner() {
  clearQrScannerFlushTimer()

  if (qrScannerDevice?.gatt?.connected) {
    qrScannerDevice.gatt.disconnect()
  }

  handleQrScannerDisconnect()
}

function handleQrScannerNotification(event: Event) {
  const characteristic = event.target as BluetoothRemoteGATTCharacteristic | null
  const value = characteristic?.value

  if (!value) {
    return
  }

  const rawChunk = textDecoder.decode(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength))
  handleQrScannerChunk(rawChunk)
}

async function connectQrScanner() {
  if (typeof window === 'undefined') {
    return
  }

  if (qrScannerConnected.value && qrScannerDevice?.gatt?.connected) {
    disconnectQrScanner()
    return
  }

  const bluetooth = (navigator as any).bluetooth
  if (!bluetooth?.requestDevice) {
    actionMessage.value = 'Web Bluetooth is not supported in this browser'
    return
  }

  try {
    qrReaderStatus.value = 'Select QR Reader device'

    const device = await bluetooth.requestDevice({
      acceptAllDevices: true,
      optionalServices: [...new Set(QR_SCANNER_PROFILES.map(profile => profile.service))],
    })

    qrScannerDevice = device
    qrScannerDeviceName.value = device.name || 'BLE QR Reader'
    qrScannerDevice.addEventListener('gattserverdisconnected', handleQrScannerDisconnect)

    const server = await qrScannerDevice.gatt?.connect()
    if (!server) {
      throw new Error('QR Reader GATT connection failed')
    }

    qrScannerCharacteristic = null

    for (const profile of QR_SCANNER_PROFILES) {
      try {
        const service = await server.getPrimaryService(profile.service)
        qrScannerCharacteristic = await service.getCharacteristic(profile.characteristic)
        break
      }
      catch {
        // try next known scanner profile
      }
    }

    if (!qrScannerCharacteristic) {
      throw new Error('QR Reader BLE service not found. Check reader BLE mode/profile.')
    }

    qrScannerCharacteristic.addEventListener('characteristicvaluechanged', handleQrScannerNotification)
    await qrScannerCharacteristic.startNotifications()

    qrScannerConnected.value = true
    actionMessage.value = `${qrScannerDeviceName.value} connected`
    await armQrScanner(scanTarget.value)
  }
  catch (error) {
    handleQrScannerDisconnect()
    actionMessage.value = error instanceof Error ? error.message : 'Unable to connect QR Reader'
  }
}

async function beginQrScanner(target: ScanTarget) {
  scanTarget.value = target

  if (qrScannerConnected.value) {
    await armQrScanner(target)
    return
  }

  await connectQrScanner()
}

function isAutoScanPalletCode(value: string) {
  return value.startsWith('Z')
}

async function handleScannedPalletWorkflow(scannedPalletId: string) {
  isBusy.value = true
  loadError.value = ''

  try {
    const response = await $fetch<ScanDecisionResponse>('/api/battery-jobs/scan', {
      query: {
        palletId: scannedPalletId,
      },
    })

    phase.value = response.recommendedPhase
    palletId.value = response.palletId
    workStartedAt.value = toDateTimeLocalValue(new Date())

    if (response.action === 'LOAD_EXISTING' && response.job) {
      applyJob(response.job)
      workStartedAt.value = toDateTimeLocalValue(new Date())
      actionMessage.value = response.recommendedPhase === 'AFTER_CHARGE'
        ? `พบ pallet ${response.palletId} แล้ว ระบบเปิด phase 2 ให้อัตโนมัติ`
        : response.recommendedPhase === 'DELIVERY'
          ? `พบ pallet ${response.palletId} แล้ว ระบบเปิด phase Delivery ให้อัตโนมัติ`
          : `พบ pallet ${response.palletId} แล้ว ระบบกลับเข้า phase ก่อนชาร์จต่อให้อัตโนมัติ`
      return
    }

    resetCurrentJobState()
    phase.value = 'BEFORE_CHARGE'
    palletId.value = response.palletId
    workStartedAt.value = toDateTimeLocalValue(new Date())

    if (operatorId.value && salesOrderId.value) {
      await openCurrentJob(true)
      return
    }

    actionMessage.value = response.reason === 'JOB_EXPIRED'
      ? `pallet ${response.palletId} เป็นงานข้ามวัน ระบบเตรียม phase 1 ใหม่แล้ว เลือกเจ้าหน้าที่และ SO เพื่อเปิดใบงาน`
      : response.reason === 'ALL_PHASES_COMPLETED'
        ? `pallet ${response.palletId} ทำครบ 3 phase แล้ว ระบบเตรียม phase 1 ใหม่ไว้ให้`
        : `ไม่พบ pallet ${response.palletId} ในระบบ ระบบเตรียม phase 1 ใหม่ไว้ให้`
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to scan pallet'
  }
  finally {
    isBusy.value = false
  }
}

async function handlePalletInput(value: string) {
  palletId.value = value.toUpperCase()

  if (!palletId.value.trim()) {
    return
  }

  scanTarget.value = 'pallet'
  applyScannedValue(palletId.value)

  if (isAutoScanPalletCode(palletId.value)) {
    await handleScannedPalletWorkflow(palletId.value)
  }
}

async function handleBatteryInput(value: string) {
  batteryScanInput.value = value.toUpperCase()

  if (!batteryScanInput.value.trim()) {
    return
  }

  scanTarget.value = 'battery'
  applyScannedValue(batteryScanInput.value)

  if (currentJob.value && phase.value === 'BEFORE_CHARGE') {
    await saveSelectedBatteryId()
  }
}

async function startBatteryIdScanWorkflow() {
  if (!isBatteryIdWorkflowEnabled.value) {
    actionMessage.value = 'phase นี้ไม่อนุญาตให้แก้ Battery ID'
    return
  }

  if (!currentJob.value) {
    actionMessage.value = 'เปิดใบงานก่อน แล้วค่อยเริ่ม scan Battery ID'
    return
  }

  workflowActionMode.value = 'battery'
  activateNextWorkflowSlot()
  await beginQrScanner('battery')
}

function handleBleDisconnect() {
  bleConnected.value = false
  bleDeviceName.value = 'BLE Volt Meter'
  bleDevice = null
  actionMessage.value = 'BLE Volt Meter disconnected'
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
    actionMessage.value = 'Select BLE Volt Meter device'

    const device = await bluetooth.requestDevice({
      acceptAllDevices: true,
      optionalServices: [BLE_SERVICE_UUID],
    })

    bleDevice = device
    bleDeviceName.value = device.name || 'BLE Volt Meter'
    bleDevice.addEventListener('gattserverdisconnected', handleBleDisconnect)
    const server = await bleDevice.gatt?.connect()
    if (!server) {
      throw new Error('BLE Volt Meter GATT connection failed')
    }

    await server.getPrimaryService(BLE_SERVICE_UUID)
    bleConnected.value = true
    actionMessage.value = bleDeviceName.value.startsWith(BLE_DEVICE_NAME_PREFIX)
      ? `${bleDeviceName.value} connected`
      : `${bleDeviceName.value} connected (service matched)`
  }
  catch (error) {
    if (bleDevice?.gatt?.connected) {
      bleDevice.gatt.disconnect()
    }

    bleDevice = null
    bleConnected.value = false
    bleDeviceName.value = 'BLE Volt Meter'
    actionMessage.value = error instanceof Error ? error.message : 'Unable to connect BLE Volt Meter'
  }
}

function getPhaseVoltage(slot: BatteryJobSlot, targetPhase: JobPhase) {
  if (targetPhase === 'BEFORE_CHARGE') {
    return slot.beforeVoltage
  }

  if (targetPhase === 'AFTER_CHARGE') {
    return slot.afterVoltage
  }

  return slot.deliveryVoltage
}

function getPhaseMeasuredAt(slot: BatteryJobSlot, targetPhase: JobPhase) {
  if (targetPhase === 'BEFORE_CHARGE') {
    return slot.beforeMeasuredAt
  }

  if (targetPhase === 'AFTER_CHARGE') {
    return slot.afterMeasuredAt
  }

  return slot.deliveryMeasuredAt
}

function getSlotVoltageLabel(slot: BatteryJobSlot, targetPhase: JobPhase) {
  const value = getPhaseVoltage(slot, targetPhase)
  return value === null ? '-' : `${value.toFixed(3)} V`
}

function normalizeJob(job: any): BatteryJobRecord {
  return {
    id: String(job.id),
    phase: job.phase as JobPhase,
    status: job.status,
    palletId: String(job.palletId),
    workStartedAt: String(job.workStartedAt),
    operatorId: job.operatorId ? String(job.operatorId) : null,
    operatorName: job.operatorName ? String(job.operatorName) : null,
    salesOrderId: job.salesOrderId ? String(job.salesOrderId) : null,
    salesOrderNumber: job.salesOrderNumber ? String(job.salesOrderNumber) : null,
    beforeChargeCompletedAt: job.beforeChargeCompletedAt ? String(job.beforeChargeCompletedAt) : null,
    afterChargeCompletedAt: job.afterChargeCompletedAt ? String(job.afterChargeCompletedAt) : null,
    deliveryCompletedAt: job.deliveryCompletedAt ? String(job.deliveryCompletedAt) : null,
    lockedAt: job.lockedAt ? String(job.lockedAt) : null,
    slots: Array.isArray(job.slots)
      ? job.slots.map((slot: any) => ({
          id: String(slot.id),
          slotNumber: Number(slot.slotNumber),
          batteryId: String(slot.batteryId ?? ''),
          beforeVoltage: slot.beforeVoltage === null || slot.beforeVoltage === undefined ? null : Number(slot.beforeVoltage),
          beforeVoltageMv: slot.beforeVoltageMv === null || slot.beforeVoltageMv === undefined ? null : Number(slot.beforeVoltageMv),
          beforeMeasuredAt: slot.beforeMeasuredAt ? String(slot.beforeMeasuredAt) : null,
          afterVoltage: slot.afterVoltage === null || slot.afterVoltage === undefined ? null : Number(slot.afterVoltage),
          afterVoltageMv: slot.afterVoltageMv === null || slot.afterVoltageMv === undefined ? null : Number(slot.afterVoltageMv),
          afterMeasuredAt: slot.afterMeasuredAt ? String(slot.afterMeasuredAt) : null,
          deliveryVoltage: slot.deliveryVoltage === null || slot.deliveryVoltage === undefined ? null : Number(slot.deliveryVoltage),
          deliveryVoltageMv: slot.deliveryVoltageMv === null || slot.deliveryVoltageMv === undefined ? null : Number(slot.deliveryVoltageMv),
          deliveryMeasuredAt: slot.deliveryMeasuredAt ? String(slot.deliveryMeasuredAt) : null,
        }))
      : [],
  }
}

function applyJob(job: any) {
  currentJob.value = normalizeJob(job)
  palletId.value = currentJob.value.palletId
  workStartedAt.value = toDateTimeLocalValue(new Date(currentJob.value.workStartedAt))
  operatorId.value = currentJob.value.operatorId ?? ''
  salesOrderId.value = currentJob.value.salesOrderId ?? ''
  workflowActionMode.value = getDefaultWorkflowActionMode(phase.value)
  selectedSlotNumber.value = currentJob.value.slots.find(slot => !getPhaseVoltage(slot, phase.value))?.slotNumber ?? 1
  syncSelectedSlot()
}

function syncSelectedSlot() {
  if (!currentJob.value) {
    selectedSlotNumber.value = 1
    return
  }

  const slots = currentJob.value.slots
  const selectedExists = slots.some(slot => slot.slotNumber === selectedSlotNumber.value)

  if (selectedExists) {
    return
  }

  selectedSlotNumber.value = firstIncompleteSlotNumber.value || 1
}

function selectPhase(nextPhase: JobPhase) {
  phase.value = nextPhase
  workflowActionMode.value = getDefaultWorkflowActionMode(nextPhase)
  selectedSlotNumber.value = 1
  batteryScanInput.value = ''
  voltageInput.value = ''
  actionMessage.value = nextPhase === 'BEFORE_CHARGE'
    ? 'เปิดใบงานใหม่สำหรับ pallet ที่กำลังจัดเตรียมก่อนชาร์จ'
    : 'เปิด pallet เดิมเพื่อวัดแรงดันตามตำแหน่งเดิม'
}

function selectSlot(slotNumber: number) {
  if (!currentJob.value) {
    return
  }

  selectedSlotNumber.value = slotNumber
  const slot = currentJob.value?.slots.find(item => item.slotNumber === slotNumber)
  batteryScanInput.value = slot?.batteryId ?? ''
  detailModalOpen.value = true
}

function activateNextWorkflowSlot() {
  if (!currentJob.value) {
    selectedSlotNumber.value = 1
    return
  }

  const nextSlot = currentJob.value.slots.find(slot => !getPhaseVoltage(slot, phase.value))
  if (!nextSlot) {
    return
  }

  selectedSlotNumber.value = nextSlot.slotNumber
  batteryScanInput.value = nextSlot.batteryId ?? ''
}

function closeSlotDetail() {
  detailModalOpen.value = false
}

async function advanceToSlot(nextSlotNumber: number | null) {
  detailModalOpen.value = false

  if (!nextSlotNumber) {
    return
  }

  await nextTick()
  selectSlot(nextSlotNumber)
}

async function loadMasters() {
  const [employeeResponse, salesOrderResponse] = await Promise.all([
    $fetch<{ employees: EmployeeItem[] }>('/api/employees'),
    $fetch<{ salesOrders: SalesOrderItem[] }>('/api/sales-orders'),
  ])

  employees.value = employeeResponse.employees
  salesOrders.value = salesOrderResponse.salesOrders
}

async function createEmployee() {
  if (!newEmployeeCode.value.trim() || !newEmployeeName.value.trim()) {
    actionMessage.value = 'กรอกรหัสและชื่อพนักงานก่อนเพิ่มข้อมูล'
    return
  }

  const response = await $fetch<{ employee: EmployeeItem }>('/api/employees', {
    method: 'POST',
    body: {
      code: newEmployeeCode.value,
      name: newEmployeeName.value,
    },
  })

  await loadMasters()
  operatorId.value = response.employee.id
  newEmployeeCode.value = ''
  newEmployeeName.value = ''
  actionMessage.value = 'เพิ่มพนักงานใหม่เรียบร้อยแล้ว'
}

async function createSalesOrder() {
  if (!newSoNumber.value.trim()) {
    actionMessage.value = 'กรอก SO Number ก่อนเพิ่มข้อมูล'
    return
  }

  const response = await $fetch<{ salesOrder: SalesOrderItem }>('/api/sales-orders', {
    method: 'POST',
    body: {
      soNumber: newSoNumber.value,
      description: newSoDescription.value,
    },
  })

  await loadMasters()
  salesOrderId.value = response.salesOrder.id
  newSoNumber.value = ''
  newSoDescription.value = ''
  actionMessage.value = 'เพิ่ม SO ใหม่เรียบร้อยแล้ว'
}

async function openCurrentJob(skipRequiredValidation = false) {
  if (!palletId.value.trim()) {
    actionMessage.value = 'กรอกหรือ scan pallet ID ก่อน'
    return
  }

  if (!skipRequiredValidation && !operatorId.value) {
    actionMessage.value = 'เลือกเจ้าหน้าที่ก่อนเปิดงาน'
    return
  }

  if (!skipRequiredValidation && phase.value === 'BEFORE_CHARGE' && !salesOrderId.value) {
    actionMessage.value = 'งานก่อนชาร์จต้องเลือก SO Number ก่อน'
    return
  }

  isBusy.value = true
  loadError.value = ''

  try {
    const response = await $fetch<{ job: BatteryJobRecord }>('/api/battery-jobs', {
      method: 'POST',
      body: {
        phase: phase.value,
        palletId: palletId.value,
        operatorId: operatorId.value || null,
        salesOrderId: salesOrderId.value || null,
        workStartedAt: new Date(workStartedAt.value).toISOString(),
      },
    })

    applyJob(response.job)
    actionMessage.value = phase.value === 'BEFORE_CHARGE'
      ? `เปิดใบงาน pallet ${palletId.value.toUpperCase()} แล้ว เริ่มที่ slot 1`
      : `เปิด pallet ${palletId.value.toUpperCase()} แล้ว เริ่มวัดตามลำดับเดิม`
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to open job'
  }
  finally {
    isBusy.value = false
  }
}

async function loadExistingPallet() {
  if (!palletId.value.trim()) {
    actionMessage.value = 'กรอก pallet ID ก่อนค้นหา'
    return
  }

  isBusy.value = true
  loadError.value = ''

  try {
    const response = await $fetch<{ job: BatteryJobRecord }>('/api/battery-jobs/pallet', {
      query: {
        palletId: palletId.value,
        phase: phase.value,
      },
    })

    applyJob(response.job)
    actionMessage.value = `โหลด pallet ${palletId.value.toUpperCase()} สำเร็จ`
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to load pallet'
  }
  finally {
    isBusy.value = false
  }
}

async function saveSelectedBatteryId() {
  if (!currentJob.value) {
    actionMessage.value = 'เปิดงานก่อนบันทึก battery ID'
    return
  }

  if (!batteryScanInput.value.trim()) {
    actionMessage.value = 'กรอกหรือ scan Battery ID ก่อน'
    return
  }

  isSavingBattery.value = true

  try {
    const response = await $fetch<{ job: BatteryJobRecord }>(`/api/battery-jobs/${currentJob.value.id}/slot-battery`, {
      method: 'PATCH',
      body: {
        slotNumber: selectedSlotNumber.value,
        batteryId: batteryScanInput.value,
      },
    })

    applyJob(response.job)
    actionMessage.value = `บันทึก Battery ID ให้ slot ${selectedSlotNumber.value} แล้ว`
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'Unable to save battery ID'
  }
  finally {
    isSavingBattery.value = false
  }
}

async function fetchMockVoltReading() {
  const payload = await $fetch<{ voltage: number }>('/api/mock/esp32-voltage', {
    query: {
      mode: 'ready',
    },
  })

  return Number(payload.voltage)
}

async function measureActiveSlotFromVoltMeter() {
  if (!currentJob.value) {
    actionMessage.value = 'เปิดงานก่อนอ่านแรงดัน'
    return
  }

  workflowActionMode.value = 'voltage'

  if (!bleConnected.value) {
    actionMessage.value = 'เชื่อมต่อ BLE Volt Meter ก่อนอ่านแรงดัน'
    return
  }

  activateNextWorkflowSlot()

  const activeSlot = currentJob.value.slots.find(slot => slot.slotNumber === selectedSlotNumber.value) ?? null
  if (!activeSlot) {
    actionMessage.value = 'ไม่พบ active slot สำหรับวัดแรงดัน'
    return
  }

  if (phase.value === 'BEFORE_CHARGE' && !(activeSlot.batteryId || batteryScanInput.value.trim())) {
    actionMessage.value = `slot ${selectedSlotNumber.value} ต้องมี Battery ID ก่อนวัดแรงดัน`
    return
  }

  isMeasuring.value = true

  try {
    const measuredSlotNumber = selectedSlotNumber.value
    const measuredVoltage = await fetchMockVoltReading()

    voltageUnit.value = 'V'
    voltageInput.value = measuredVoltage.toFixed(3)

    const response = await $fetch<{ job: BatteryJobRecord }>(`/api/battery-jobs/${currentJob.value.id}/slot`, {
      method: 'PATCH',
      body: {
        phase: phase.value,
        slotNumber: measuredSlotNumber,
        batteryId: phase.value === 'BEFORE_CHARGE' ? (activeSlot.batteryId || batteryScanInput.value || '') : null,
        voltage: measuredVoltage.toFixed(3),
        voltageUnit: 'V',
        measuredAt: new Date(workStartedAt.value).toISOString(),
      },
    })

    applyJob(response.job)
    showMeasurementPopup(measuredVoltage)
    actionMessage.value = `อ่านและบันทึกแรงดัน slot ${measuredSlotNumber} = ${measuredVoltage.toFixed(3)} V`
    await wait(2000)
    measurementPopupOpen.value = false
    activateNextWorkflowSlot()
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'Unable to read voltage from meter'
  }
  finally {
    isMeasuring.value = false
  }
}

function fillMockVoltage() {
  const base = phase.value === 'BEFORE_CHARGE' ? 12.18 : phase.value === 'AFTER_CHARGE' ? 13.42 : 12.95
  const next = Number((base + Math.random() * 0.35).toFixed(3))
  voltageInput.value = voltageUnit.value === 'MV' ? String(Math.round(next * 1000)) : String(next)
}

async function measureSelectedSlot() {
  if (!currentJob.value) {
    actionMessage.value = 'เปิดงานก่อนบันทึกแรงดัน'
    return
  }

  if (!voltageInput.value.trim()) {
    actionMessage.value = 'กรอกค่าแรงดันก่อนกดอ่าน/บันทึก'
    return
  }

  if (phase.value === 'BEFORE_CHARGE' && !(batteryScanInput.value.trim() || selectedSlot.value?.batteryId)) {
    actionMessage.value = 'slot ก่อนชาร์จต้องมี Battery ID ก่อนวัด'
    return
  }

  isMeasuring.value = true

  try {
    const measuredSlotNumber = selectedSlotNumber.value
    const response = await $fetch<{ job: BatteryJobRecord }>(`/api/battery-jobs/${currentJob.value.id}/slot`, {
      method: 'PATCH',
      body: {
        phase: phase.value,
        slotNumber: selectedSlotNumber.value,
        batteryId: phase.value === 'BEFORE_CHARGE' ? (batteryScanInput.value || selectedSlot.value?.batteryId || '') : null,
        voltage: voltageInput.value,
        voltageUnit: voltageUnit.value,
        measuredAt: new Date(workStartedAt.value).toISOString(),
      },
    })

    applyJob(response.job)
    const nextSlot = currentJob.value.slots.find(slot => !getPhaseVoltage(slot, phase.value))
    actionMessage.value = `บันทึกแรงดัน slot ${measuredSlotNumber} สำเร็จ`
    await advanceToSlot(nextSlot?.slotNumber ?? null)
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'Unable to save voltage'
  }
  finally {
    isMeasuring.value = false
  }
}

async function confirmCurrentPhase() {
  if (!currentJob.value) {
    actionMessage.value = 'ยังไม่มีงานให้ยืนยัน'
    return
  }

  isConfirming.value = true

  try {
    const response = await $fetch<{ job: BatteryJobRecord }>(`/api/battery-jobs/${currentJob.value.id}/confirm`, {
      method: 'PATCH',
      body: {
        phase: phase.value,
      },
    })

    applyJob(response.job)
    actionMessage.value = phase.value === 'DELIVERY'
      ? 'ยืนยัน Delivery สำเร็จ ระบบ mark ready for delivery และ lock ค่าแล้ว'
      : `ยืนยันงาน ${currentPhaseMeta.value.label} สำเร็จ`
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'Unable to confirm phase'
  }
  finally {
    isConfirming.value = false
  }
}

function formatDateTime(value: string | null) {
  if (!value) {
    return '-'
  }

  return new Date(value).toLocaleString('th-TH', {
    dateStyle: 'short',
    timeStyle: 'short',
  })
}

onMounted(async () => {
  try {
    await loadMasters()
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to load master data'
  }
})

onBeforeUnmount(() => {
  clearMeasurementPopupTimer()
  disconnectQrScanner()

  if (bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect()
  }
})
</script>

<template>
  <main class="min-h-screen bg-[linear-gradient(135deg,_#eef7e6_0%,_#e8f0ff_50%,_#fff4dc_100%)] px-4 py-3 text-slate-950">
    <section class="mx-auto flex max-w-[1600px] flex-col gap-3">
      <UCard
        :ui="{
          root: 'rounded-[26px] border-0 bg-white/92 shadow-[0_24px_80px_rgba(15,23,42,0.10)] ring-1 ring-slate-950/5',
          body: 'p-4'
        }"
      >
        <div class="flex flex-wrap items-start justify-between gap-3">
          <div>
            <div class="text-sm font-bold uppercase tracking-[0.24em] text-slate-500">Tablet Workspace</div>
            <h1 class="mt-1 text-4xl font-black tracking-tight text-slate-950">
              New Batt Job
            </h1>
          </div>

          <div class="flex flex-col items-end gap-2">
            <div class="flex flex-wrap items-center justify-end gap-3">
              <UButton
                color="neutral"
                :variant="qrScannerConnected ? 'solid' : 'soft'"
                :class="`${qrScannerConnected ? phasePrimaryButtonClass : phaseSoftButtonClass} rounded-2xl px-4 py-2 text-sm font-black`"
                @click="connectQrScanner"
              >
                {{ qrScannerLabel }}
              </UButton>
              <UButton
                color="neutral"
                :variant="bleConnected ? 'solid' : 'soft'"
                :class="`${bleConnected ? phasePrimaryButtonClass : phaseSoftButtonClass} rounded-2xl px-4 py-2 text-sm font-black`"
                @click="connectBleVoltMeter"
              >
                {{ bleStatusLabel }}
              </UButton>
            </div>
            <div class="flex flex-wrap items-center justify-end gap-3">
              <div :class="qrScannerConnected ? 'text-sm font-black text-emerald-800' : 'text-sm font-black text-rose-700'">
                {{ qrScannerStatusLabel }}
              </div>
              <div :class="bleConnected ? 'text-sm font-black text-emerald-800' : 'text-sm font-black text-rose-700'">
                {{ bleMeterStatusLabel }}
              </div>
            </div>
          </div>
        </div>
      </UCard>

      <div class="grid gap-3 lg:grid-cols-3">
        <UButton
          v-for="option in phaseOptions"
          :key="option.value"
          block
          size="xl"
          color="neutral"
          variant="solid"
          :class="phase === option.value ? option.tone : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-100 active:bg-slate-200'"
          :ui="{ base: 'min-h-[86px] justify-start rounded-[22px] px-5 text-left' }"
          @click="selectPhase(option.value)"
        >
          <div>
            <div class="text-2xl font-black">{{ option.label }}</div>
            <div class="mt-1 text-sm font-medium opacity-90">{{ option.detail }}</div>
          </div>
        </UButton>
      </div>

      <UCard
        :ui="{
          root: 'rounded-[22px] border-0 bg-white/92 shadow-[0_18px_48px_rgba(15,23,42,0.08)]',
          body: 'p-4'
        }"
      >
        <div class="flex items-center justify-between gap-3">
          <div>
            <div class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500">ใบงาน</div>
            <div class="mt-1 text-3xl font-black text-slate-950">{{ currentPhaseMeta.label }}</div>
          </div>
          <div class="flex items-center gap-3">
            <UButton
              color="neutral"
              variant="solid"
              :loading="isBusy"
              :class="`${phasePrimaryButtonClass} rounded-2xl px-5 py-3 text-base font-black`"
              @click="openCurrentJob"
            >
              {{ openButtonLabel }}
            </UButton>
            <UButton
              color="neutral"
              variant="soft"
              :class="`${phaseSoftButtonClass} rounded-2xl px-5 py-3 text-base font-black`"
              @click="actionMessage = 'กรุณาเพิ่มพนักงานจากฐานข้อมูลกลางหรือใช้ API สำหรับเพิ่มพนักงาน'"
            >
              เพิ่มพนักงาน
            </UButton>
          </div>
        </div>

        <div class="mt-4 grid gap-3 md:grid-cols-2 xl:grid-cols-[1.2fr_1fr_1fr_1fr]">
            <label class="block">
              <div class="mb-1 text-sm font-bold text-slate-700">Pallet ID</div>
              <input
                ref="palletInput"
                v-model="palletId"
                type="text"
                placeholder="Scan QR / Enter Pallet ID"
                class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm font-semibold uppercase text-slate-950 outline-none"
                @keyup.enter="handlePalletInput(palletId)"
              />
            </label>
            <label class="block">
              <div class="mb-1 text-sm font-bold text-slate-700">วันเวลา</div>
              <input v-model="workStartedAt" type="datetime-local" class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm text-slate-950 outline-none ring-0" />
            </label>

            <label class="block">
              <div class="mb-1 text-sm font-bold text-slate-700">เจ้าหน้าที่</div>
              <select v-model="operatorId" class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm text-slate-950 outline-none">
                <option value="">เลือกเจ้าหน้าที่</option>
                <option v-for="employee in employees" :key="employee.id" :value="employee.id">
                  {{ employee.code }} - {{ employee.name }}
                </option>
              </select>
            </label>

            <label class="block">
              <div class="mb-1 text-sm font-bold text-slate-700">SO Number</div>
              <select v-model="salesOrderId" class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm text-slate-950 outline-none">
                <option value="">เลือก SO</option>
                <option v-for="salesOrder in salesOrders" :key="salesOrder.id" :value="salesOrder.id">
                  {{ salesOrder.soNumber }}{{ salesOrder.description ? ` - ${salesOrder.description}` : '' }}
                </option>
              </select>
            </label>

        </div>
      </UCard>

      <div class="grid gap-3">
        <UCard
          :ui="{
          root: 'rounded-[22px] border-0 bg-white/92 shadow-[0_18px_48px_rgba(15,23,42,0.08)]',
            body: 'p-4'
          }"
        >
          <div class="flex items-center justify-between gap-3">
            <div>
              <div class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500">Pallet Layout</div>
              <div class="mt-1 text-2xl font-black text-slate-950">{{ currentJob?.palletId || 'ยังไม่มีใบงาน' }}</div>
            </div>
            <div class="flex items-center gap-3">
              <UButton
                color="neutral"
                :variant="workflowActionMode === 'battery' ? 'solid' : 'soft'"
                :disabled="!isBatteryIdWorkflowEnabled"
                class="min-w-[180px] justify-center rounded-2xl px-4 py-3 text-center text-sm font-black"
                :class="!isBatteryIdWorkflowEnabled
                  ? 'border border-slate-300 bg-slate-100 text-slate-400'
                  : workflowActionMode === 'battery'
                    ? workflowActionActiveClass
                    : workflowActionIdleClass"
                @click="startBatteryIdScanWorkflow"
              >
                Get Battery ID
              </UButton>
              <UButton
                color="neutral"
                :variant="workflowActionMode === 'voltage' ? 'solid' : 'soft'"
                :loading="isMeasuring"
                class="min-w-[180px] justify-center rounded-2xl px-4 py-3 text-center text-sm font-black"
                :class="workflowActionMode === 'voltage' ? workflowActionActiveClass : workflowActionIdleClass"
                @click="measureActiveSlotFromVoltMeter"
              >
                Get Voltage
              </UButton>
            </div>
          </div>

          <div v-if="hasJob" class="mt-4 space-y-3">
            <div
              v-for="(row, rowIndex) in slotRows"
              :key="rowIndex"
              class="grid grid-cols-7 gap-2"
            >
              <button
                v-for="slot in row"
                :key="slot.slotNumber"
                type="button"
                class="rounded-[5px] border p-3 text-left transition-all duration-150"
                :class="[
                  selectedSlotNumber === slot.slotNumber
                    ? `${activeSlotCardClass} shadow-[0_16px_30px_rgba(15,23,42,0.12)]`
                    : slot.slotNumber === firstIncompleteSlotNumber
                      ? 'border-slate-300 bg-slate-50 text-slate-950'
                      : 'border-slate-200 bg-white text-slate-950 hover:bg-slate-50',
                ]"
                @click="selectSlot(slot.slotNumber)"
              >
                <div class="flex items-center justify-between gap-2">
                  <div class="text-xs font-bold uppercase tracking-[0.18em]" :class="selectedSlotNumber === slot.slotNumber ? 'text-current/75' : 'text-slate-500'">
                    Slot {{ slot.slotNumber }}
                  </div>
                  <div class="rounded-full px-2 py-1 text-[10px] font-bold" :class="selectedSlotNumber === slot.slotNumber ? 'bg-white/70 text-current' : 'bg-slate-100 text-slate-700'">
                    {{
                      getPhaseVoltage(slot, phase) !== null
                        ? 'DONE'
                        : selectedSlotNumber === slot.slotNumber
                          ? 'ACTIVE'
                          : slot.slotNumber === firstIncompleteSlotNumber
                            ? 'NEXT'
                            : 'WAIT'
                    }}
                  </div>
                </div>
                <div class="mt-2 truncate text-sm font-bold">
                  Batt ID: {{ slot.batteryId || '-' }}
                </div>
                <div class="mt-2 space-y-1 text-xs">
                  <div>Before: {{ slot.beforeVoltage !== null ? `${slot.beforeVoltage.toFixed(3)} V` : '-' }}</div>
                  <div>After: {{ slot.afterVoltage !== null ? `${slot.afterVoltage.toFixed(3)} V` : '-' }}</div>
                  <div>Delivery: {{ slot.deliveryVoltage !== null ? `${slot.deliveryVoltage.toFixed(3)} V` : '-' }}</div>
                </div>
              </button>
            </div>
          </div>
          <div v-else class="mt-4 rounded-[22px] border border-dashed border-slate-300 bg-slate-50 px-6 py-12 text-center">
            <div class="text-lg font-black text-slate-900">ยังไม่มีข้อมูล pallet</div>
            <div class="mt-2 text-sm text-slate-600">
              เปิดใบงานใหม่หรือโหลด pallet เดิมก่อน แล้วระบบจะแสดงตำแหน่ง battery ทั้ง 21 slot ที่นี่
            </div>
          </div>
        </UCard>

        <div class="grid gap-4 xl:grid-cols-[minmax(0,0.9fr)_minmax(0,1.1fr)]">
          <div class="rounded-[22px] bg-white/92 px-4 py-4 text-sm text-slate-700 shadow-[0_18px_48px_rgba(15,23,42,0.08)]">
            <div class="text-sm font-bold text-slate-900">Workflow Rules</div>
            <ul class="mt-2 space-y-1 text-sm text-slate-600">
              <li>วัดครั้งแรกต้องเรียงตาม slot 1-21 ห้ามข้าม</li>
              <li>เลือก slot เดิมเพื่อวัดซ้ำหรือเปลี่ยน Battery ID ได้</li>
              <li>Delivery confirm แล้วจะ lock ค่าแรงดันและแก้ไขไม่ได้</li>
            </ul>
          </div>

          <div class="rounded-[22px] bg-slate-100 px-4 py-4 text-sm text-slate-700">
            {{ actionMessage }}
          </div>
        </div>

        <UAlert
          v-if="loadError"
          class="mt-3"
          color="error"
          variant="soft"
          title="Load Error"
          :description="loadError"
        />
      </div>

      <UModal
        v-model:open="detailModalOpen"
        :dismissible="true"
        :close="false"
        :content="{ class: 'w-[min(50vw,740px)] max-w-none rounded-[24px] bg-white shadow-[0_24px_80px_rgba(15,23,42,0.28)]' }"
        :ui="{ body: 'max-h-[88vh] overflow-y-auto p-6', overlay: 'bg-slate-950/35 backdrop-blur-[2px]' }"
      >
        <template #body>
          <div class="space-y-6">
            <div class="space-y-4">
              <div class="grid grid-cols-[1fr_auto_1fr] items-start gap-4">
                <div class="flex items-start gap-3">
                  <div>
                    <div class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500">Active Slot</div>
                    <div class="mt-1 text-3xl font-black text-slate-950">Slot {{ selectedSlotNumber }}</div>
                  </div>
                </div>
                <div class="justify-self-center rounded-[8px] px-3 py-2 text-center" :class="currentPhaseMeta.softTone">
                  <div class="text-sm font-black text-slate-950">
                    Current Phase: {{ currentPhaseMeta.label }}
                  </div>
                </div>
                <div class="flex items-start justify-self-end gap-2">
                  <UButton
                    color="neutral"
                    variant="solid"
                    :loading="isMeasuring"
                    class="h-8 rounded-2xl bg-lime-700 px-3 text-xs font-black text-white hover:bg-lime-800"
                    @click="measureSelectedSlot"
                  >
                    บันทึก
                  </UButton>
                  <UButton color="neutral" variant="ghost" icon="i-lucide-x" class="rounded-2xl" @click="closeSlotDetail" />
                </div>
              </div>
            </div>

            <div class="grid gap-4 xl:grid-cols-2">
              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">Battery ID</div>
                <input
                  ref="batteryInput"
                  v-model="batteryScanInput"
                  type="text"
                  placeholder="Battery ID / QR scan"
                  class="h-14 w-full rounded-lg border border-slate-300 bg-white px-4 text-base font-semibold uppercase text-slate-950 outline-none"
                  @keyup.enter="handleBatteryInput(batteryScanInput)"
                />
              </label>

              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">Voltage (Volt)</div>
                <input
                  v-model="voltageInput"
                  type="number"
                  step="0.001"
                  placeholder="12.650"
                  class="h-14 w-full rounded-lg border border-slate-300 bg-white px-4 text-base font-semibold text-slate-950 outline-none"
                />
              </label>
            </div>
          </div>
        </template>
      </UModal>

      <div
        v-if="measurementPopupOpen"
        class="pointer-events-none fixed inset-0 z-[70] flex items-center justify-center px-6"
      >
        <div class="min-w-[360px] rounded-[18px] bg-slate-950/92 px-12 py-8 text-center text-white shadow-[0_30px_80px_rgba(15,23,42,0.35)]">
          <div class="text-sm font-bold uppercase tracking-[0.22em] text-emerald-200">Voltage Read</div>
          <div class="mt-3 text-6xl font-black tracking-tight">
            {{ measurementPopupValue }}
          </div>
          <div class="mt-2 text-2xl font-bold text-emerald-100">
            {{ measurementPopupUnit }}
          </div>
        </div>
      </div>
    </section>
  </main>
</template>
