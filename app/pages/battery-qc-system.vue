<script setup lang="ts">
type JobPhase = 'BEFORE_CHARGE' | 'AFTER_CHARGE' | 'DELIVERY'
type VoltageUnit = 'V' | 'MV'
type ScanTarget = 'rack' | 'battery'

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

interface InvoiceItem {
  id: string
  invoiceNo: string
  description: string | null
}

interface ChargeChannelItem {
  id: string
  code: string
  name: string
}

interface ChargeProgramItem {
  id: string
  code: string
  name: string
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
  rackId: string
  palletId: string
  openedAt: string
  workStartedAt: string
  operatorId: string | null
  operatorName: string | null
  salesOrderId: string | null
  salesOrderNumber: string | null
  invoiceId: string | null
  invoiceNumber: string | null
  chargeChannelId: string | null
  chargeChannelCode: string | null
  chargeChannelName: string | null
  chargeProgramId: string | null
  chargeProgramCode: string | null
  chargeProgramName: string | null
  beforeChargeCompletedAt: string | null
  afterChargeCompletedAt: string | null
  deliveryCompletedAt: string | null
  lockedAt: string | null
  slots: BatteryJobSlot[]
}

interface ScanDecisionResponse {
  ok: boolean
  rackId: string
  palletId: string
  found: boolean
  action: 'LOAD_EXISTING' | 'OPEN_NEW_BEFORE_CHARGE'
  recommendedPhase: JobPhase
  reason: 'NO_JOB_FOUND' | 'ALL_PHASES_COMPLETED' | 'JOB_EXPIRED' | 'CONTINUE_EXISTING'
  job: BatteryJobRecord | null
}

const phase = ref<JobPhase | null>(null)
const currentJob = ref<BatteryJobRecord | null>(null)
const employees = ref<EmployeeItem[]>([])
const salesOrders = ref<SalesOrderItem[]>([])
const invoices = ref<InvoiceItem[]>([])
const chargeChannels = ref<ChargeChannelItem[]>([])
const chargePrograms = ref<ChargeProgramItem[]>([])
const selectedSlotNumber = ref(1)
const editingSlotNumber = ref<number | null>(null)
const openedAt = ref(toDateTimeLocalValue(new Date()))
const operatorId = ref('')
const employeeScanInput = ref('')
const salesOrderId = ref('')
const rackId = ref('')
const invoiceId = ref('')
const chargeChannelId = ref('')
const chargeProgramId = ref('')
const batteryScanInput = ref('')
const voltageScanInput = ref('')
const voltageInput = ref('')
const voltageUnit = ref<VoltageUnit>('V')
const newEmployeeCode = ref('')
const newEmployeeName = ref('')
const newSoNumber = ref('')
const newSoDescription = ref('')
const detailModalOpen = ref(false)
const scanTarget = ref<ScanTarget>('rack')
const bleConnected = ref(false)
const bleDeviceName = ref('BLE Volt Meter')
const isBusy = ref(false)
const isSavingBattery = ref(false)
const isMeasuring = ref(false)
const isConfirming = ref(false)
const loadError = ref('')
const actionMessage = ref('เปิดใบงานหรือเปิด rack เพื่อเริ่มงานวัดแรงดัน battery')
const measurementPopupOpen = ref(false)
const measurementPopupValue = ref('')
const measurementPopupUnit = ref('')
const measurementPopupLabel = ref('Voltage Read')
const workflowActionMode = ref<'battery' | 'voltage' | null>(null)

const BLE_SERVICE_UUID = '7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001'
const BLE_DEVICE_NAME_PREFIX = 'PUMA-Voltmeter-'

const rackInputRef = useTemplateRef<HTMLInputElement>('rackInput')
const batteryInputRef = useTemplateRef<HTMLInputElement>('batteryInput')
const voltageInputRef = useTemplateRef<HTMLInputElement>('voltageInput')
const keyboardBatteryScanRef = useTemplateRef<HTMLInputElement>('keyboardBatteryScan')
const employeeInputRef = useTemplateRef<HTMLInputElement>('employeeInput')
const keyboardVoltageScanRef = useTemplateRef<HTMLInputElement>('keyboardVoltageScan')

let bleDevice: any = null
let measurementPopupTimer: ReturnType<typeof setTimeout> | null = null

const phaseOptions = [
  {
    value: 'BEFORE_CHARGE' as const,
    label: 'QC Before Charge',
    title: 'QC Before Charge',
    detail: 'บันทึก Battery ID และค่าแรงดันก่อนชาร์จ',
    icon: 'i-lucide-battery-low',
    theme: 'before' as const,
    tone: 'bg-lime-700 text-white hover:bg-lime-800 active:bg-lime-900',
    softTone: 'border border-lime-300 bg-lime-100 text-lime-950',
  },
  {
    value: 'AFTER_CHARGE' as const,
    label: 'QC After Charge',
    title: 'QC After Charge',
    detail: 'อ่านค่าแรงดันหลังชาร์จตามตำแหน่งเดิม',
    icon: 'i-lucide-battery-full',
    theme: 'after' as const,
    tone: 'bg-sky-700 text-white hover:bg-sky-800 active:bg-sky-900',
    softTone: 'border border-sky-300 bg-sky-100 text-sky-950',
  },
  {
    value: 'DELIVERY' as const,
    label: 'QC Before Delivery',
    title: 'QC Before Delivery',
    detail: 'ตรวจวัดก่อนส่งมอบและยืนยันค่าก่อนจัดส่ง',
    icon: 'i-lucide-truck',
    theme: 'delivery' as const,
    tone: 'bg-amber-700 text-white hover:bg-amber-800 active:bg-amber-900',
    softTone: 'border border-amber-300 bg-amber-100 text-amber-950',
  },
] as const

const currentPhaseMeta = computed(() => phaseOptions.find(option => option.value === phase.value) ?? null)
const hasPhaseSelected = computed(() => phase.value !== null)
const selectedEmployee = computed(() => employees.value.find(employee => employee.id === operatorId.value) ?? null)
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
const modalSelectedSlot = computed(() => {
  if (!editingSlotNumber.value) {
    return null
  }

  return currentJob.value?.slots.find(slot => slot.slotNumber === editingSlotNumber.value) ?? null
})
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
const openButtonLabel = computed(() => 'Rack View')
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

async function armDefaultWorkflow(autoFocus = true) {
  if (!currentJob.value || !phase.value) {
    workflowActionMode.value = null
    return
  }

  workflowActionMode.value = getDefaultWorkflowActionMode(phase.value)
  activateNextModeSlot()

  if (autoFocus) {
    await focusWorkflowInput()
  }
}

function resetCurrentJobState() {
  currentJob.value = null
  selectedSlotNumber.value = 1
  editingSlotNumber.value = null
  batteryScanInput.value = ''
  voltageInput.value = ''
  workflowActionMode.value = null
  detailModalOpen.value = false
}

function syncWorkflowSlotInputs() {
  batteryScanInput.value = selectedSlot.value?.batteryId ?? ''
}

function syncModalSlotInputs() {
  batteryScanInput.value = modalSelectedSlot.value?.batteryId ?? ''
}

function clearMeasurementPopupTimer() {
  if (measurementPopupTimer) {
    clearTimeout(measurementPopupTimer)
    measurementPopupTimer = null
  }
}

function showMeasurementPopup(voltage: number) {
  clearMeasurementPopupTimer()
  measurementPopupLabel.value = 'Voltage'
  measurementPopupValue.value = voltage.toFixed(3)
  measurementPopupUnit.value = 'V'
  measurementPopupOpen.value = true
  measurementPopupTimer = setTimeout(() => {
    measurementPopupOpen.value = false
  }, 2000)
}

function showBatteryIdPopup(batteryId: string) {
  clearMeasurementPopupTimer()
  measurementPopupLabel.value = 'Battery ID'
  measurementPopupValue.value = batteryId
  measurementPopupUnit.value = ''
  measurementPopupOpen.value = true
  measurementPopupTimer = setTimeout(() => {
    measurementPopupOpen.value = false
  }, 2000)
}

function wait(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms))
}

async function focusEmployeeInput() {
  await nextTick()
  employeeInputRef.value?.focus()
  employeeInputRef.value?.select()
}

async function focusRackInput() {
  await nextTick()
  rackInputRef.value?.focus()
  rackInputRef.value?.select()
}

function backToPhaseLanding() {
  phase.value = null
  workflowActionMode.value = null
  detailModalOpen.value = false
  editingSlotNumber.value = null
  actionMessage.value = 'เลือกโหมด QC เพื่อเริ่มงาน'
}

async function focusActiveModalField() {
  await nextTick()

  if (!detailModalOpen.value) {
    return
  }

  const activeMode = phase.value === 'BEFORE_CHARGE' ? 'battery' : 'voltage'

  if (activeMode === 'battery') {
    batteryInputRef.value?.focus()
    batteryInputRef.value?.select()
    return
  }

  voltageInputRef.value?.focus()
  voltageInputRef.value?.select()
}

async function focusWorkflowInput() {
  await nextTick()

  if (detailModalOpen.value) {
    return
  }

  if (!workflowActionMode.value) {
    return
  }

  if (workflowActionMode.value === 'battery') {
    keyboardBatteryScanRef.value?.focus()
    keyboardBatteryScanRef.value?.select()
    return
  }

  keyboardVoltageScanRef.value?.focus()
  keyboardVoltageScanRef.value?.select()
}

async function restoreWorkflowFocus() {
  if (typeof document !== 'undefined') {
    const activeElement = document.activeElement as HTMLElement | null
    activeElement?.blur?.()
  }

  await wait(20)
  await focusWorkflowInput()
  await wait(80)
  await focusWorkflowInput()
}

function activateNextModeSlot() {
  if (!currentJob.value) {
    selectedSlotNumber.value = 1
    editingSlotNumber.value = null
    return
  }

  if (workflowActionMode.value === 'battery') {
    const nextBatterySlot = currentJob.value.slots.find(slot => !slot.batteryId.trim())
    if (nextBatterySlot) {
      selectedSlotNumber.value = nextBatterySlot.slotNumber
      editingSlotNumber.value = null
      syncWorkflowSlotInputs()
      return
    }
  }

  const nextVoltageSlot = currentJob.value.slots.find(slot => getPhaseVoltage(slot, phase.value) === null)
  if (nextVoltageSlot) {
    selectedSlotNumber.value = nextVoltageSlot.slotNumber
    editingSlotNumber.value = null
    syncWorkflowSlotInputs()
  }
}

function applyScannedValue(value: string) {
  const normalized = value.trim().toUpperCase()

  if (!normalized) {
    return
  }

  if (scanTarget.value === 'rack') {
    rackId.value = normalized
    openedAt.value = toDateTimeLocalValue(new Date())
    actionMessage.value = `สแกน rack ${normalized} แล้ว และตั้งเวลาเปิดใบงานให้อัตโนมัติ`
    return
  }

  batteryScanInput.value = normalized
}

function isAutoScanRackCode(value: string) {
  return value.startsWith('Z')
}

async function handleScannedRackWorkflow(scannedRackId: string) {
  isBusy.value = true
  loadError.value = ''

  try {
    const response = await $fetch<ScanDecisionResponse>('/api/battery-jobs/scan', {
      query: {
        rackId: scannedRackId,
      },
    })

    phase.value = response.recommendedPhase
    rackId.value = response.rackId || response.palletId
    openedAt.value = toDateTimeLocalValue(new Date())

    if (response.action === 'LOAD_EXISTING' && response.job) {
      applyJob(response.job)
      await armDefaultWorkflow()
      openedAt.value = toDateTimeLocalValue(new Date())
      actionMessage.value = response.recommendedPhase === 'AFTER_CHARGE'
        ? `พบ rack ${response.rackId || response.palletId} แล้ว ระบบเปิด phase 2 ให้อัตโนมัติ`
        : response.recommendedPhase === 'DELIVERY'
          ? `พบ rack ${response.rackId || response.palletId} แล้ว ระบบเปิด phase Delivery ให้อัตโนมัติ`
          : `พบ rack ${response.rackId || response.palletId} แล้ว ระบบกลับเข้า phase ก่อนชาร์จต่อให้อัตโนมัติ`
      return
    }

    resetCurrentJobState()
    phase.value = 'BEFORE_CHARGE'
    rackId.value = response.rackId || response.palletId
    openedAt.value = toDateTimeLocalValue(new Date())

    if (operatorId.value && salesOrderId.value) {
      await openCurrentJob(true)
      return
    }

    actionMessage.value = response.reason === 'JOB_EXPIRED'
      ? `rack ${response.rackId || response.palletId} เป็นงานข้ามวัน ระบบเตรียม phase 1 ใหม่แล้ว เลือกเจ้าหน้าที่และ SO เพื่อเปิดใบงาน`
      : response.reason === 'ALL_PHASES_COMPLETED'
        ? `rack ${response.rackId || response.palletId} ทำครบ 3 phase แล้ว ระบบเตรียม phase 1 ใหม่ไว้ให้`
        : `ไม่พบ rack ${response.rackId || response.palletId} ในระบบ ระบบเตรียม phase 1 ใหม่ไว้ให้`
    await focusEmployeeInput()
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to scan rack'
  }
  finally {
    isBusy.value = false
  }
}

async function handleRackInput(value: string) {
  rackId.value = value.toUpperCase()

  if (!rackId.value.trim()) {
    return
  }

  scanTarget.value = 'rack'
  applyScannedValue(rackId.value)

  if (isAutoScanRackCode(rackId.value)) {
    await handleScannedRackWorkflow(rackId.value)
    return
  }

  await focusEmployeeInput()
}

function resolveEmployeeId(scannedValue: string) {
  const normalized = scannedValue.trim().toUpperCase()
  const matched = employees.value.find(employee => employee.code.trim().toUpperCase() === normalized)
  if (!matched) {
    actionMessage.value = `ไม่พบ Employee ID ${normalized} ในฐานข้อมูล`
    return
  }

  operatorId.value = matched.id
  employeeScanInput.value = matched.code
  actionMessage.value = `เลือกเจ้าหน้าที่ ${matched.code} - ${matched.name} แล้ว`
}

function selectEmployeeById(employeeId: string) {
  operatorId.value = employeeId

  const matched = employees.value.find(employee => employee.id === employeeId)
  if (!matched) {
    employeeScanInput.value = ''
    return
  }

  employeeScanInput.value = matched.code
  actionMessage.value = `เลือกเจ้าหน้าที่ ${matched.code} - ${matched.name} แล้ว`
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

async function handleVoltageReaderInput(value: string) {
  const normalized = value.trim()
  if (!normalized) {
    return
  }

  voltageScanInput.value = ''
  workflowActionMode.value = 'voltage'
  voltageInput.value = normalized
  await saveActiveSlotVoltage(normalized)
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
  activateNextModeSlot()
  scanTarget.value = 'battery'
  actionMessage.value = `พร้อมรับค่า Battery ID สำหรับ slot ${selectedSlotNumber.value}`
  await focusWorkflowInput()
}

async function startVoltageWorkflow() {
  if (!currentJob.value) {
    actionMessage.value = 'เปิดงานก่อน แล้วค่อยเริ่มอ่านค่า Voltage'
    return
  }

  workflowActionMode.value = 'voltage'
  activateNextModeSlot()
  actionMessage.value = `พร้อมรับค่า Voltage สำหรับ slot ${selectedSlotNumber.value}`
  await focusWorkflowInput()
}

function handleBleDisconnect() {
  bleConnected.value = false
  bleDeviceName.value = 'BLE Volt Meter'
  bleDevice = null
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

  await server.getPrimaryService(BLE_SERVICE_UUID)
  bleConnected.value = true

  if (showConnectedMessage) {
    actionMessage.value = bleDeviceName.value.startsWith(BLE_DEVICE_NAME_PREFIX)
      ? `${bleDeviceName.value} connected`
      : `${bleDeviceName.value} connected (service matched)`
  }
}

async function tryAutoReconnectBleVoltMeter() {
  if (typeof window === 'undefined') {
    return
  }

  const bluetooth = (navigator as any).bluetooth
  if (!bluetooth?.getDevices) {
    return
  }

  try {
    const devices = await bluetooth.getDevices()
    const matchedDevice = devices.find((device: BluetoothDevice) => {
      const name = device.name || ''
      return name.startsWith(BLE_DEVICE_NAME_PREFIX) || name.includes('Voltmeter')
    })

    if (!matchedDevice) {
      return
    }

    await connectToBleVoltMeterDevice(matchedDevice, false)
    actionMessage.value = `${bleDeviceName.value} reconnected automatically`
  }
  catch {
    bleConnected.value = false
    bleDevice = null
    bleDeviceName.value = 'BLE Volt Meter'
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
    actionMessage.value = 'Select BLE Volt Meter device'

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

function getSlotActiveMetric(targetPhase: JobPhase | null) {
  if (targetPhase === 'AFTER_CHARGE') {
    return 'after' as const
  }

  if (targetPhase === 'DELIVERY') {
    return 'delivery' as const
  }

  return 'before' as const
}

function formatSlotCardVoltage(value: number | null) {
  return value === null ? null : value.toFixed(2)
}

function getSlotCardUi(slot: BatteryJobSlot) {
  const targetPhase = phase.value ?? 'BEFORE_CHARGE'
  const isSelected = selectedSlotNumber.value === slot.slotNumber
  const isDone = getPhaseVoltage(slot, targetPhase) !== null

  if (isSelected) {
    if (targetPhase === 'AFTER_CHARGE') {
      return {
        titleClass: 'bg-sky-700 text-white',
        bodyClass: 'bg-sky-50 text-sky-950',
        borderClass: 'border-sky-700',
        statusLabel: 'HERE',
        statusClass: 'bg-white/20 text-white',
        activeMetricClass: 'bg-sky-700 text-white',
      }
    }

    if (targetPhase === 'DELIVERY') {
      return {
        titleClass: 'bg-amber-600 text-white',
        bodyClass: 'bg-amber-50 text-amber-950',
        borderClass: 'border-amber-600',
        statusLabel: 'HERE',
        statusClass: 'bg-white/20 text-white',
        activeMetricClass: 'bg-amber-600 text-white',
      }
    }

    return {
      titleClass: 'bg-emerald-800 text-white',
      bodyClass: 'bg-emerald-50 text-emerald-950',
      borderClass: 'border-emerald-800',
      statusLabel: 'HERE',
      statusClass: 'bg-white/20 text-white',
      activeMetricClass: 'bg-emerald-800 text-white',
    }
  }

  if (isDone) {
    return {
      titleClass: 'bg-slate-500 text-white',
      bodyClass: 'bg-slate-100 text-slate-900',
      borderClass: 'border-slate-400',
      statusLabel: 'DONE',
      statusClass: 'bg-white/20 text-white',
      activeMetricClass: 'bg-slate-500 text-white',
    }
  }

  return {
    titleClass: 'bg-slate-300 text-slate-800',
    bodyClass: 'bg-white text-slate-900',
    borderClass: 'border-slate-200',
    statusLabel: 'WAIT',
    statusClass: 'bg-slate-100 text-slate-700',
    activeMetricClass: 'bg-slate-300 text-slate-800',
  }
}

function normalizeJob(job: any): BatteryJobRecord {
  const normalizedRackId = String(job.rackId ?? job.palletId ?? '')
  const normalizedOpenedAt = String(job.openedAt ?? job.workStartedAt ?? new Date().toISOString())

  return {
    id: String(job.id),
    phase: job.phase as JobPhase,
    status: job.status,
    rackId: normalizedRackId,
    palletId: normalizedRackId,
    openedAt: normalizedOpenedAt,
    workStartedAt: normalizedOpenedAt,
    operatorId: job.operatorId ? String(job.operatorId) : null,
    operatorName: job.operatorName ? String(job.operatorName) : null,
    salesOrderId: job.salesOrderId ? String(job.salesOrderId) : null,
    salesOrderNumber: job.salesOrderNumber ? String(job.salesOrderNumber) : null,
    invoiceId: job.invoiceId ? String(job.invoiceId) : null,
    invoiceNumber: job.invoiceNumber ? String(job.invoiceNumber) : null,
    chargeChannelId: job.chargeChannelId ? String(job.chargeChannelId) : null,
    chargeChannelCode: job.chargeChannelCode ? String(job.chargeChannelCode) : null,
    chargeChannelName: job.chargeChannelName ? String(job.chargeChannelName) : null,
    chargeProgramId: job.chargeProgramId ? String(job.chargeProgramId) : null,
    chargeProgramCode: job.chargeProgramCode ? String(job.chargeProgramCode) : null,
    chargeProgramName: job.chargeProgramName ? String(job.chargeProgramName) : null,
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
  rackId.value = currentJob.value.rackId
  openedAt.value = toDateTimeLocalValue(new Date(currentJob.value.openedAt))
  operatorId.value = currentJob.value.operatorId ?? ''
  employeeScanInput.value = selectedEmployee.value?.code ?? ''
  salesOrderId.value = currentJob.value.salesOrderId ?? ''
  invoiceId.value = currentJob.value.invoiceId ?? ''
  chargeChannelId.value = currentJob.value.chargeChannelId ?? ''
  chargeProgramId.value = currentJob.value.chargeProgramId ?? ''
  selectedSlotNumber.value = currentJob.value.slots.find(slot => !getPhaseVoltage(slot, phase.value))?.slotNumber ?? 1
  syncSelectedSlot()
  if (detailModalOpen.value && editingSlotNumber.value) {
    syncModalSlotInputs()
    return
  }

  syncWorkflowSlotInputs()
}

function syncSelectedSlot() {
  if (!currentJob.value) {
    selectedSlotNumber.value = 1
    editingSlotNumber.value = null
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
  workflowActionMode.value = null
  selectedSlotNumber.value = 1
  editingSlotNumber.value = null
  batteryScanInput.value = ''
  voltageInput.value = ''
  actionMessage.value = nextPhase === 'BEFORE_CHARGE'
    ? 'เปิดใบงานใหม่สำหรับ rack ที่กำลังจัดเตรียมก่อนชาร์จ'
    : 'เปิด rack เดิมเพื่อวัดแรงดันตามตำแหน่งเดิม'
  void focusRackInput()
}

function selectSlot(slotNumber: number) {
  if (!currentJob.value) {
    return
  }

  workflowActionMode.value = null
  editingSlotNumber.value = slotNumber
  syncModalSlotInputs()
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
  editingSlotNumber.value = null
  activateNextModeSlot()
  syncWorkflowSlotInputs()
  void restoreWorkflowFocus()
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
  const [employeeResponse, salesOrderResponse, invoiceResponse, chargeChannelResponse, chargeProgramResponse] = await Promise.all([
    $fetch<{ employees: EmployeeItem[] }>('/api/employees'),
    $fetch<{ salesOrders: SalesOrderItem[] }>('/api/sales-orders'),
    $fetch<{ invoices: InvoiceItem[] }>('/api/invoices'),
    $fetch<{ chargeChannels: ChargeChannelItem[] }>('/api/charge-channels'),
    $fetch<{ chargePrograms: ChargeProgramItem[] }>('/api/charge-programs'),
  ])

  employees.value = employeeResponse.employees
  salesOrders.value = salesOrderResponse.salesOrders
  invoices.value = invoiceResponse.invoices
  chargeChannels.value = chargeChannelResponse.chargeChannels
  chargePrograms.value = chargeProgramResponse.chargePrograms
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
  selectEmployeeById(response.employee.id)
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
  if (!phase.value) {
    actionMessage.value = 'เลือก phase ก่อนเปิดงาน'
    return
  }

  if (!rackId.value.trim()) {
    actionMessage.value = 'กรอกหรือ scan rack ID ก่อน'
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
        rackId: rackId.value,
        operatorId: operatorId.value || null,
        salesOrderId: salesOrderId.value || null,
        invoiceId: invoiceId.value || null,
        chargeChannelId: chargeChannelId.value || null,
        chargeProgramId: chargeProgramId.value || null,
        openedAt: new Date(openedAt.value).toISOString(),
      },
    })

    applyJob(response.job)
    await armDefaultWorkflow()
    actionMessage.value = phase.value === 'BEFORE_CHARGE'
      ? `เปิดใบงาน rack ${rackId.value.toUpperCase()} แล้ว เริ่มที่ slot 1`
      : `เปิด rack ${rackId.value.toUpperCase()} แล้ว เริ่มวัดตามลำดับเดิม`
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to open job'
  }
  finally {
    isBusy.value = false
  }
}

async function loadExistingRack() {
  if (!phase.value) {
    actionMessage.value = 'เลือก phase ก่อนโหลด rack'
    return
  }

  if (!rackId.value.trim()) {
    actionMessage.value = 'กรอก rack ID ก่อนค้นหา'
    return
  }

  isBusy.value = true
  loadError.value = ''

  try {
    const response = await $fetch<{ job: BatteryJobRecord }>('/api/battery-jobs/pallet', {
      query: {
        rackId: rackId.value,
        phase: phase.value,
      },
    })

    applyJob(response.job)
    await armDefaultWorkflow()
    actionMessage.value = `โหลด rack ${rackId.value.toUpperCase()} สำเร็จ`
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to load rack'
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
    const savedSlotNumber = detailModalOpen.value ? (editingSlotNumber.value ?? selectedSlotNumber.value) : selectedSlotNumber.value
    const response = await $fetch<{ job: BatteryJobRecord }>(`/api/battery-jobs/${currentJob.value.id}/slot-battery`, {
      method: 'PATCH',
      body: {
        slotNumber: savedSlotNumber,
        batteryId: batteryScanInput.value,
      },
    })

    applyJob(response.job)

    if (!detailModalOpen.value) {
      showBatteryIdPopup(batteryScanInput.value)
      const nextSlot = currentJob.value?.slots.find(slot => !slot.batteryId.trim())
      if (nextSlot) {
        selectedSlotNumber.value = nextSlot.slotNumber
        syncWorkflowSlotInputs()
        actionMessage.value = `บันทึก Battery ID ให้ slot ${savedSlotNumber} แล้ว เลื่อนไป slot ${nextSlot.slotNumber}`
      }
      else {
        batteryScanInput.value = ''
        actionMessage.value = `บันทึก Battery ID ครบทุก slot แล้ว`
      }
      return
    }

    actionMessage.value = `บันทึก Battery ID ให้ slot ${savedSlotNumber} แล้ว`
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

async function saveActiveSlotVoltage(voltageValue: string) {
  if (!phase.value) {
    actionMessage.value = 'เลือก phase ก่อนบันทึกแรงดัน'
    return
  }

  if (!currentJob.value) {
    actionMessage.value = 'เปิดงานก่อนบันทึกแรงดัน'
    return
  }

  if (!voltageValue.trim()) {
    actionMessage.value = 'ไม่มีค่า Voltage ที่อ่านเข้ามา'
    return
  }

  if (!detailModalOpen.value) {
    activateNextWorkflowSlot()
  }

  const targetSlotNumber = detailModalOpen.value ? (editingSlotNumber.value ?? selectedSlotNumber.value) : selectedSlotNumber.value
  const activeSlot = currentJob.value.slots.find(slot => slot.slotNumber === targetSlotNumber) ?? null
  if (!activeSlot) {
    actionMessage.value = 'ไม่พบ active slot สำหรับบันทึกแรงดัน'
    return
  }

  if (phase.value === 'BEFORE_CHARGE' && !(activeSlot.batteryId || batteryScanInput.value.trim())) {
    actionMessage.value = `slot ${targetSlotNumber} ต้องมี Battery ID ก่อนวัดแรงดัน`
    return
  }

  isMeasuring.value = true

  try {
    const measuredSlotNumber = targetSlotNumber
    const response = await $fetch<{ job: BatteryJobRecord }>(`/api/battery-jobs/${currentJob.value.id}/slot`, {
      method: 'PATCH',
      body: {
        phase: phase.value,
        slotNumber: measuredSlotNumber,
        batteryId: phase.value === 'BEFORE_CHARGE' ? (activeSlot.batteryId || batteryScanInput.value || '') : null,
        voltage: voltageValue,
        voltageUnit: 'V',
        measuredAt: new Date().toISOString(),
      },
    })

    applyJob(response.job)
    showMeasurementPopup(Number(voltageValue))

    if (!detailModalOpen.value) {
      const nextSlot = currentJob.value?.slots.find(slot => getPhaseVoltage(slot, phase.value) === null)
      if (nextSlot) {
        selectedSlotNumber.value = nextSlot.slotNumber
        syncWorkflowSlotInputs()
        actionMessage.value = `บันทึก Voltage slot ${measuredSlotNumber} แล้ว เลื่อนไป slot ${nextSlot.slotNumber}`
      }
      else {
        actionMessage.value = `บันทึก Voltage ครบทุก slot แล้ว`
      }
      return
    }

    actionMessage.value = `บันทึกแรงดัน slot ${measuredSlotNumber} สำเร็จ`
  }
  catch (error) {
    actionMessage.value = error instanceof Error ? error.message : 'Unable to save voltage'
  }
  finally {
    isMeasuring.value = false
  }
}

async function measureActiveSlotFromVoltMeter() {
  if (!phase.value) {
    actionMessage.value = 'เลือก phase ก่อนอ่านแรงดัน'
    return
  }

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
        measuredAt: new Date().toISOString(),
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
  if (!phase.value) {
    actionMessage.value = 'เลือก phase ก่อนบันทึกแรงดัน'
    return
  }

  if (!currentJob.value) {
    actionMessage.value = 'เปิดงานก่อนบันทึกแรงดัน'
    return
  }

  if (!voltageInput.value.trim()) {
    actionMessage.value = 'กรอกค่าแรงดันก่อนกดอ่าน/บันทึก'
    return
  }

  if (phase.value === 'BEFORE_CHARGE' && !(batteryScanInput.value.trim() || modalSelectedSlot.value?.batteryId)) {
    actionMessage.value = 'slot ก่อนชาร์จต้องมี Battery ID ก่อนวัด'
    return
  }

  isMeasuring.value = true

  try {
    const measuredSlotNumber = editingSlotNumber.value ?? selectedSlotNumber.value
    const response = await $fetch<{ job: BatteryJobRecord }>(`/api/battery-jobs/${currentJob.value.id}/slot`, {
      method: 'PATCH',
      body: {
        phase: phase.value,
        slotNumber: measuredSlotNumber,
        batteryId: phase.value === 'BEFORE_CHARGE' ? (batteryScanInput.value || modalSelectedSlot.value?.batteryId || '') : null,
        voltage: voltageInput.value,
        voltageUnit: voltageUnit.value,
        measuredAt: new Date().toISOString(),
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

  if (!phase.value) {
    actionMessage.value = 'เลือก phase ก่อนยืนยันงาน'
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
    await tryAutoReconnectBleVoltMeter()
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to load master data'
  }
})

watch([detailModalOpen, workflowActionMode], async ([isOpen]) => {
  if (!isOpen) {
    await restoreWorkflowFocus()
    return
  }

  await focusActiveModalField()
})

onBeforeUnmount(() => {
  clearMeasurementPopupTimer()

  if (bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect()
  }
})
</script>

<template>
  <main class="min-h-screen bg-[linear-gradient(135deg,_#eef7e6_0%,_#e8f0ff_50%,_#fff4dc_100%)] px-4 py-3 text-slate-950">
    <section class="mx-auto flex max-w-[1600px] flex-col gap-3">
      <UCard
        v-if="!hasPhaseSelected"
        :ui="{
          root: 'rounded-[24px] border-0 bg-[#2f2f32] shadow-[0_24px_80px_rgba(15,23,42,0.16)] ring-1 ring-slate-950/10',
          body: 'px-5 py-3'
        }"
      >
        <div class="grid items-center gap-3 lg:grid-cols-[minmax(0,1fr)_auto_minmax(0,1fr)]">
          <div>
            <div class="text-[11px] text-white/80 sm:text-xs">MF Auto Workspace</div>
            <h1 class="mt-1.5 text-[1.9rem] font-black leading-none tracking-tight text-white sm:text-[2.35rem]">
              Battery QC System
            </h1>
          </div>

          <div class="flex justify-center lg:justify-self-center">
            <img src="/branding/logo-puma-battery.png" alt="PUMA Battery" class="h-12 w-auto object-contain sm:h-16" />
          </div>

          <div />
        </div>
      </UCard>

      <div v-if="!hasPhaseSelected" class="flex min-h-[52vh] items-center justify-center">
        <div class="grid w-full max-w-[1180px] gap-5 lg:grid-cols-3">
          <BatteryQcModeCard
            v-for="option in phaseOptions"
            :key="option.value"
            :label="option.label"
            :detail="option.detail"
            :icon="option.icon"
            :theme="option.theme"
            @select="selectPhase(option.value)"
          />
        </div>
      </div>

      <div
        v-else
        class="overflow-hidden rounded-[22px] border shadow-[0_18px_48px_rgba(15,23,42,0.08)]"
        :class="phase === 'BEFORE_CHARGE'
          ? 'border-lime-500 bg-[linear-gradient(180deg,_#4d8f00_0%,_#65a30d_100%)] text-white'
          : phase === 'AFTER_CHARGE'
            ? 'border-sky-500 bg-[linear-gradient(180deg,_#1874b8_0%,_#0ea5e9_100%)] text-white'
            : 'border-amber-600 bg-[linear-gradient(180deg,_#d97706_0%,_#f59e0b_100%)] text-white'"
      >
        <div class="grid items-center gap-4 px-5 py-4 lg:grid-cols-[minmax(0,1fr)_auto_minmax(0,1fr)] lg:px-6 lg:py-4">
          <div class="flex items-center gap-4">
            <div
              class="flex h-12 w-12 items-center justify-center rounded-xl bg-white/18 ring-1 ring-white/20 lg:h-14 lg:w-14"
            >
              <UIcon
                :name="currentPhaseMeta?.icon ?? 'i-lucide-battery'"
                class="size-6 lg:size-7"
                :class="phase === 'DELIVERY' ? 'text-white' : 'text-white'"
              />
            </div>
            <div>
              <div class="text-[1.55rem] font-black leading-none lg:text-[1.8rem]">{{ currentPhaseMeta?.label }}</div>
              <div
                class="mt-1 text-xs font-semibold lg:text-sm"
                :class="phase === 'DELIVERY' ? 'text-white/85' : 'text-white/85'"
              >
                {{ currentPhaseMeta?.detail }}
              </div>
            </div>
          </div>

          <div class="flex justify-center lg:justify-self-center">
            <img src="/branding/logo-puma-battery.png" alt="PUMA Battery" class="h-14 w-auto object-contain lg:h-16" />
          </div>

          <div class="flex items-center justify-end">
            <UButton
              color="neutral"
              variant="solid"
              class="rounded-xl px-4 py-2.5 text-sm font-black"
              :class="phase === 'DELIVERY'
                ? 'border border-white/35 bg-white/12 text-white hover:bg-white/20 active:bg-white/25'
                : 'border border-white/30 bg-white/12 text-white hover:bg-white/20 active:bg-white/25'"
              @click="backToPhaseLanding"
            >
              Change Mode
            </UButton>
          </div>
        </div>
      </div>

      <template v-if="hasPhaseSelected">
        <UCard
          :ui="{
            root: 'rounded-[22px] border-0 bg-white/92 shadow-[0_18px_48px_rgba(15,23,42,0.08)]',
            body: 'p-4'
          }"
        >
          <div class="flex items-center justify-between gap-3">
            <div>
              <div class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500">QC Header</div>
              <div class="mt-1 text-3xl font-black text-slate-950">{{ currentPhaseMeta?.label ?? 'Select QC Phase' }}</div>
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

          <div class="mt-4 grid gap-3 md:grid-cols-2 xl:grid-cols-6">
              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">Rack No</div>
                <input
                  ref="rackInput"
                  v-model="rackId"
                  type="text"
                  placeholder="Scan Rack QR"
                  class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm font-semibold uppercase text-slate-950 outline-none"
                  @keyup.enter="handleRackInput(rackId)"
                />
              </label>

              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">Employee / เจ้าหน้าที่</div>
                <input
                  ref="employeeInput"
                  v-model="employeeScanInput"
                  type="text"
                  placeholder="Scan / Enter Employee ID"
                  list="employee-code-options"
                  class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm font-semibold uppercase text-slate-950 outline-none"
                  @change="resolveEmployeeId(employeeScanInput)"
                  @keyup.enter="resolveEmployeeId(employeeScanInput)"
                />
                <datalist id="employee-code-options">
                  <option v-for="employee in employees" :key="employee.id" :value="employee.code">
                    {{ employee.code }} - {{ employee.name }}
                  </option>
                </datalist>
                <div v-if="selectedEmployee" class="mt-1 min-h-[20px] text-xs font-semibold text-slate-500">
                  {{ `${selectedEmployee.code} - ${selectedEmployee.name}` }}
                </div>
              </label>

              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">Sale Order #</div>
                <select v-model="salesOrderId" class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm text-slate-950 outline-none">
                  <option value="">เลือก SO</option>
                  <option v-for="salesOrder in salesOrders" :key="salesOrder.id" :value="salesOrder.id">
                    {{ salesOrder.soNumber }}{{ salesOrder.description ? ` - ${salesOrder.description}` : '' }}
                  </option>
                </select>
              </label>

              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">Invoice #</div>
                <select v-model="invoiceId" class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm text-slate-950 outline-none">
                  <option value="">เลือก Invoice</option>
                  <option v-for="invoice in invoices" :key="invoice.id" :value="invoice.id">
                    {{ invoice.invoiceNo }}{{ invoice.description ? ` - ${invoice.description}` : '' }}
                  </option>
                </select>
              </label>

              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">Charge Channel</div>
                <select v-model="chargeChannelId" class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm text-slate-950 outline-none">
                  <option value="">เลือก Channel</option>
                  <option v-for="channel in chargeChannels" :key="channel.id" :value="channel.id">
                    {{ channel.name }}
                  </option>
                </select>
              </label>

              <label class="block">
                <div class="mb-1 text-sm font-bold text-slate-700">โปรแกรม Charge</div>
                <select v-model="chargeProgramId" class="h-12 w-full rounded-lg border border-slate-300 bg-white px-3 text-sm text-slate-950 outline-none">
                  <option value="">เลือก Program</option>
                  <option v-for="program in chargePrograms" :key="program.id" :value="program.id">
                    {{ program.name }}
                  </option>
                </select>
              </label>

          </div>
        </UCard>

        <div class="grid gap-3">
        <input
          ref="keyboardBatteryScan"
          v-model="batteryScanInput"
          type="text"
          inputmode="text"
          autocomplete="off"
          class="pointer-events-none absolute left-[-9999px] top-0 h-0 w-0 opacity-0"
          @keyup.enter="handleBatteryInput(batteryScanInput)"
        />
        <input
          ref="keyboardVoltageScan"
          v-model="voltageScanInput"
          type="text"
          inputmode="decimal"
          autocomplete="off"
          class="pointer-events-none absolute left-[-9999px] top-0 h-0 w-0 opacity-0"
          @keyup.enter="handleVoltageReaderInput(voltageScanInput)"
        />

        <UCard
          :ui="{
          root: 'rounded-[22px] border-0 bg-white/92 shadow-[0_18px_48px_rgba(15,23,42,0.08)]',
            body: 'p-4'
          }"
        >
          <div class="flex items-center justify-between gap-3">
            <div>
              <div class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500">Rack Layout</div>
              <div class="mt-1 text-2xl font-black text-slate-950">{{ currentJob?.rackId || 'ยังไม่มี Rack' }}</div>
            </div>
            <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-3">
              <div class="flex items-center gap-4">
                <div class="text-sm font-black text-slate-950">Mode</div>
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
                  Battery ID
                </UButton>
                <UButton
                  color="neutral"
                  :variant="workflowActionMode === 'voltage' ? 'solid' : 'soft'"
                  class="min-w-[180px] justify-center rounded-2xl px-4 py-3 text-center text-sm font-black"
                  :class="workflowActionMode === 'voltage' ? workflowActionActiveClass : workflowActionIdleClass"
                  @click="startVoltageWorkflow"
                >
                  Voltage
                </UButton>
              </div>
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
                class="block w-full text-left transition-all duration-150 hover:-translate-y-0.5"
                @click="selectSlot(slot.slotNumber)"
              >
                <BatteryQcSlotCard
                  size="compact"
                  :slot-id="slot.slotNumber"
                  :batt-id="slot.batteryId || '-'"
                  :volt-b="formatSlotCardVoltage(slot.beforeVoltage)"
                  :volt-a="formatSlotCardVoltage(slot.afterVoltage)"
                  :volt-d="formatSlotCardVoltage(slot.deliveryVoltage)"
                  header-icon="ph:car-battery"
                  :title-class="getSlotCardUi(slot).titleClass"
                  :body-class="getSlotCardUi(slot).bodyClass"
                  :border-class="getSlotCardUi(slot).borderClass"
                  :status-label="getSlotCardUi(slot).statusLabel"
                  :status-class="getSlotCardUi(slot).statusClass"
                  :active-metric="getSlotActiveMetric(phase)"
                  :active-metric-class="getSlotCardUi(slot).activeMetricClass"
                />
              </button>
            </div>
          </div>
          <div v-else class="mt-4 rounded-[22px] border border-dashed border-slate-300 bg-slate-50 px-6 py-12 text-center">
            <div class="text-lg font-black text-slate-900">ยังไม่มีข้อมูล rack</div>
            <div class="mt-2 text-sm text-slate-600">
              เปิดใบงานใหม่หรือโหลด rack เดิมก่อน แล้วระบบจะแสดงตำแหน่ง battery ทั้ง 21 slot ที่นี่
            </div>
          </div>
        </UCard>

        <UAlert
          v-if="loadError"
          class="mt-3"
          color="error"
          variant="soft"
          title="Load Error"
          :description="loadError"
        />
        </div>
      </template>

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
                    <div class="mt-1 text-3xl font-black text-slate-950">Slot {{ editingSlotNumber ?? selectedSlotNumber }}</div>
                  </div>
                </div>
                    <div class="justify-self-center rounded-[8px] px-3 py-2 text-center" :class="currentPhaseMeta?.softTone ?? 'border border-slate-300 bg-slate-100 text-slate-950'">
                  <div class="text-sm font-black text-slate-950">
                    Current Phase: {{ currentPhaseMeta?.label ?? '-' }}
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
                  ref="voltageInput"
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
          <div class="text-sm font-bold uppercase tracking-[0.22em] text-emerald-200">{{ measurementPopupLabel }}</div>
          <div class="mt-3 text-6xl font-black tracking-tight">
            {{ measurementPopupValue }}
          </div>
          <div v-if="measurementPopupUnit" class="mt-2 text-2xl font-bold text-emerald-100">
            {{ measurementPopupUnit }}
          </div>
        </div>
      </div>
    </section>
  </main>
</template>
