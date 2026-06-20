<script setup lang="ts">
type MovementStage = 'STOCK_TO_CHARGE' | 'CHARGE_TO_DELIVERY' | 'DELIVERY_TRANSFER'
type ScanField = 'from' | 'battery' | 'to'
type WorkflowStep = 'from' | 'voltage' | 'battery' | 'to' | 'save'
type ScanSource = 'camera' | 'scanner' | 'manual'
type VoltageSource = 'esp32' | 'manual' | 'mock'
type Esp32LinkState = 'disconnected' | 'connecting' | 'measuring' | 'ready' | 'error'

interface MovementDraft {
  stage: MovementStage
  fromRack: string
  fromSlot: string
  batterySn: string
  voltage: number | null
  voltageMeasuredAt: string
  toRack: string
  toSlot: string
}

interface MovementRecord extends MovementDraft {
  id: string
  savedAt: string
  firmwareVersion?: string | null
  syncStatus?: 'db' | 'local'
}

const STORAGE_KEY = 'battery-movement-v1-records'
const COUNT_OFFSET_KEY = 'battery-movement-v1-count-offset'
const API_ENDPOINT = '/api/battery-movements'
const BLE_SERVICE_UUID = '7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001'
const BLE_CHARACTERISTIC_UUID = '7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001'
const BLE_DEVICE_NAME = 'PUMA-Voltmeter'
const BLE_DEVICE_NAME_PREFIX = 'PUMA-Voltmeter-'
const BLE_DEVICE_ID_PREFIX = 'PUMA-VoltMeter-'

const video = useTemplateRef<HTMLVideoElement>('video')
const canvas = useTemplateRef<HTMLCanvasElement>('canvas')
const scannerInput = useTemplateRef<HTMLInputElement>('scannerInput')

const stage = ref<MovementStage>('STOCK_TO_CHARGE')
const scanField = ref<ScanField>('from')
const stageOptions = ['STOCK_TO_CHARGE', 'CHARGE_TO_DELIVERY', 'DELIVERY_TRANSFER']

const draft = reactive<MovementDraft>({
  stage: 'STOCK_TO_CHARGE',
  fromRack: '',
  fromSlot: '',
  batterySn: '',
  voltage: null,
  voltageMeasuredAt: '',
  toRack: '',
  toSlot: '',
})

const records = ref<MovementRecord[]>([])
const manualScanValue = ref('')
const manualVoltage = ref('')
const hasStarted = ref(false)
const isStarting = ref(false)
const showSimulateTest = ref(false)
const isSavingRecord = ref(false)
const esp32LinkState = ref<Esp32LinkState>('disconnected')
const esp32StatusMessage = ref('Connect BLE device, then wait for STEP 3')
const lastScanSource = ref<ScanSource | null>(null)
const lastVoltageSource = ref<VoltageSource | null>(null)
const log = ref('Start camera, scan with scanner gun, or use manual scan to begin a movement record.')
const countOffset = ref(0)
const bleDeviceName = ref(BLE_DEVICE_NAME)
const bleConnected = ref(false)

let mediaStream: MediaStream | null = null
let detectorTimer: ReturnType<typeof setInterval> | null = null
let lastDetection = 0
let audioContext: AudioContext | null = null
let scannerBuffer = ''
let scannerLastInputAt = 0
let scannerInputTimer: ReturnType<typeof setTimeout> | null = null
let esp32LastVoltage: number | null = null
let esp32StableHits = 0
let bleDevice: BluetoothDevice | null = null
let bleServer: BluetoothRemoteGATTServer | null = null
let bleCharacteristic: BluetoothRemoteGATTCharacteristic | null = null
let blePayloadDeviceId: string | null = null
let blePayloadFirmwareVersion: string | null = null
const textDecoder = new TextDecoder()

const stageMeta = computed(() => {
  const map: Record<MovementStage, {
    index: string
    label: string
    title: string
    detail: string
    bg: string
    panel: string
    status: string
    badge: string
    action: string
    soft: string
  }> = {
    STOCK_TO_CHARGE: {
      index: 'OPERATION 1',
      label: 'Stock to Charge',
      title: 'Stock To Charge',
      detail: 'Read source rack/slot, voltage, battery S/N, then target charging rack/slot',
      bg: 'bg-lime-200',
      panel: 'bg-white/30',
      status: 'border-lime-800/20 bg-lime-700 text-white',
      badge: 'bg-lime-200 text-lime-950 ring-lime-300/80',
      action: 'bg-lime-900 text-white hover:bg-lime-800',
      soft: 'border border-lime-300/70 bg-lime-100/88 text-lime-950 hover:bg-lime-200/88',
    },
    CHARGE_TO_DELIVERY: {
      index: 'OPERATION 2',
      label: 'Charge to Delivery',
      title: 'Charge To Delivery',
      detail: 'Read charging rack/slot, voltage, battery S/N, then delivery rack/slot',
      bg: 'bg-emerald-200',
      panel: 'bg-white/30',
      status: 'border-emerald-800/20 bg-emerald-700 text-white',
      badge: 'bg-emerald-200 text-emerald-950 ring-emerald-300/80',
      action: 'bg-emerald-900 text-white hover:bg-emerald-800',
      soft: 'border border-emerald-300/70 bg-emerald-100/88 text-emerald-950 hover:bg-emerald-200/88',
    },
    DELIVERY_TRANSFER: {
      index: 'OPERATION 3',
      label: 'Delivery Transfer',
      title: 'Delivery Transfer',
      detail: 'Read delivery rack/slot, voltage, battery S/N, then transfer to the next delivery rack/slot',
      bg: 'bg-sky-200',
      panel: 'bg-white/32',
      status: 'border-sky-800/20 bg-sky-700 text-white',
      badge: 'bg-sky-200 text-sky-950 ring-sky-300/80',
      action: 'bg-sky-900 text-white hover:bg-sky-800',
      soft: 'border border-sky-300/70 bg-sky-100/88 text-sky-950 hover:bg-sky-200/88',
    },
  }

  return map[stage.value]
})

const stageButtonLabels: Record<MovementStage, string> = {
  STOCK_TO_CHARGE: 'Stock to Charge',
  CHARGE_TO_DELIVERY: 'Charge to Delivery',
  DELIVERY_TRANSFER: 'Delivery Transfer',
}

function normalizeOperation(value?: string) {
  const normalized = {
    PRE_CHARGE: 'STOCK_TO_CHARGE',
    AFTER_CHARGE: 'CHARGE_TO_DELIVERY',
    DELIVERY: 'DELIVERY_TRANSFER',
  }[String(value ?? '').trim()] ?? String(value ?? '').trim()

  if (!['STOCK_TO_CHARGE', 'CHARGE_TO_DELIVERY', 'DELIVERY_TRANSFER'].includes(normalized)) {
    return 'STOCK_TO_CHARGE' as MovementStage
  }

  return normalized as MovementStage
}

function getOperationLabel(value: MovementStage) {
  return stageButtonLabels[value]
}

const currentStep = computed(() => {
  if (!draft.fromRack || !draft.fromSlot) {
    return 'STEP 1 • Scan FROM rack/slot'
  }

  if (!draft.batterySn && draft.voltage === null) {
    return 'STEP 2 • Scan batt SN or read voltage'
  }

  if (!draft.batterySn) {
    return 'STEP 2 • Scan batt SN'
  }

  if (draft.voltage === null) {
    return 'STEP 3 • Read voltage from BLE ESP32'
  }

  if (!draft.toRack || !draft.toSlot) {
    return 'STEP 4 • Scan TO rack/slot'
  }

  return 'STEP 5 • Saving movement'
})

const workflowStep = computed<WorkflowStep>(() => {
  if (!draft.fromRack || !draft.fromSlot) {
    return 'from'
  }

  if (!draft.batterySn && draft.voltage === null) {
    return scanField.value === 'battery' ? 'battery' : 'voltage'
  }

  if (!draft.batterySn) {
    return 'battery'
  }

  if (draft.voltage === null) {
    return 'voltage'
  }

  if (!draft.toRack || !draft.toSlot) {
    return 'to'
  }

  return 'save'
})

const workflowStepMeta = computed(() => {
  const map: Record<WorkflowStep, {
    bg: string
    panel: string
    status: string
  }> = {
    from: {
      bg: 'bg-lime-200',
      panel: 'bg-lime-100/92',
      status: 'border-lime-800/20 bg-lime-700 text-white',
    },
    voltage: {
      bg: 'bg-amber-200',
      panel: 'bg-amber-100/92',
      status: 'border-amber-800/20 bg-amber-700 text-white',
    },
    battery: {
      bg: 'bg-emerald-200',
      panel: 'bg-emerald-100/92',
      status: 'border-emerald-800/20 bg-emerald-700 text-white',
    },
    to: {
      bg: 'bg-sky-200',
      panel: 'bg-sky-100/92',
      status: 'border-sky-800/20 bg-sky-700 text-white',
    },
    save: {
      bg: 'bg-violet-200',
      panel: 'bg-violet-100/92',
      status: 'border-violet-800/20 bg-violet-700 text-white',
    },
  }

  return map[workflowStep.value]
})

const bleSupported = computed(() => {
  if (typeof window === 'undefined') {
    return false
  }

  return 'bluetooth' in navigator
})

const bleConnectionLabel = computed(() => {
  if (bleConnected.value) {
    return bleDeviceName.value
  }

  return bleSupported.value ? 'Not Connected' : 'Web Bluetooth Unsupported'
})

const canSave = computed(() => Boolean(
  draft.fromRack
  && draft.fromSlot
  && draft.batterySn
  && draft.voltage !== null
  && draft.toRack
  && draft.toSlot,
))

const displayCount = computed(() => Math.max(records.value.length - countOffset.value, 0))

const scanTargetLabel = computed(() => {
  if (scanField.value === 'from') {
    return 'FROM Rack/Slot'
  }

  if (scanField.value === 'battery') {
    return 'Battery S/N'
  }

  return 'TO Rack/Slot'
})

const captureDeviceStatus = computed(() => {
  if (hasStarted.value) {
    return 'Camera + Scanner Ready'
  }

  return 'Scanner Ready • Camera Optional'
})

const captureSupportDetail = computed(() => {
  if (hasStarted.value) {
    return 'Camera scan, scanner gun, and manual scan are all available'
  }

  return 'Scanner gun and manual scan are available now'
})

const lastScanSourceLabel = computed(() => {
  const map: Record<ScanSource, string> = {
    camera: 'Camera',
    scanner: 'Scanner Gun',
    manual: 'Manual',
  }

  return lastScanSource.value ? map[lastScanSource.value] : 'Waiting'
})

const lastVoltageSourceLabel = computed(() => {
  const map: Record<VoltageSource, string> = {
    esp32: 'ESP32',
    manual: 'Manual',
    mock: 'Mock',
  }

  return lastVoltageSource.value ? map[lastVoltageSource.value] : 'Waiting'
})

const esp32Status = computed(() => {
  if (bleConnected.value && workflowStep.value !== 'voltage' && draft.voltage === null) {
    return {
      title: 'BLE Connected',
      detail: 'Voltage will be requested from BLE at STEP 3',
      tone: 'text-sky-950',
      chip: 'bg-sky-100 text-sky-950',
    }
  }

  if (esp32LinkState.value === 'ready' && draft.voltage !== null) {
    return {
      title: 'Voltage Ready',
      detail: esp32StatusMessage.value,
      tone: 'text-emerald-950',
      chip: 'bg-emerald-100 text-emerald-950',
    }
  }

  if (esp32LinkState.value === 'connecting') {
    return {
      title: 'Connecting',
      detail: esp32StatusMessage.value,
      tone: 'text-sky-950',
      chip: 'bg-sky-100 text-sky-950',
    }
  }

  if (esp32LinkState.value === 'measuring') {
    return {
      title: 'Waiting for ESP32',
      detail: esp32StatusMessage.value,
      tone: 'text-amber-950',
      chip: 'bg-amber-100 text-amber-950',
    }
  }

  if (esp32LinkState.value === 'error') {
    return {
      title: 'ESP32 Error',
      detail: esp32StatusMessage.value,
      tone: 'text-rose-950',
      chip: 'bg-rose-100 text-rose-950',
    }
  }

  return {
    title: bleConnected.value ? 'BLE Connected' : esp32LinkState.value === 'disconnected' ? 'Disconnected' : 'Standby',
    detail: esp32StatusMessage.value,
    tone: 'text-slate-950',
    chip: 'bg-slate-100 text-slate-950',
  }
})

const bleTransportStatusLabel = computed(() => bleConnected.value ? 'BLE Linked' : 'BLE Offline')
const bleIdentityDetail = computed(() => {
  const details = [
    blePayloadDeviceId || null,
    blePayloadFirmwareVersion ? `FW ${blePayloadFirmwareVersion}` : null,
  ].filter(Boolean)

  return details.length ? details.join(' • ') : 'Waiting for BLE payload'
})

function ensureAudioContext() {
  if (typeof window === 'undefined' || !('AudioContext' in window)) {
    return null
  }

  if (!audioContext) {
    audioContext = new window.AudioContext()
  }

  if (audioContext.state === 'suspended') {
    void audioContext.resume()
  }

  return audioContext
}

function playToneSequence(notes: Array<{ freq: number, at: number, duration: number }>, gainValue = 0.12) {
  const context = ensureAudioContext()

  if (!context) {
    return
  }

  const startAt = context.currentTime + 0.02

  notes.forEach((note) => {
    const oscillator = context.createOscillator()
    const gain = context.createGain()

    oscillator.type = 'sine'
    oscillator.frequency.setValueAtTime(note.freq, startAt + note.at)

    gain.gain.setValueAtTime(0.0001, startAt + note.at)
    gain.gain.linearRampToValueAtTime(gainValue, startAt + note.at + 0.01)
    gain.gain.exponentialRampToValueAtTime(0.0001, startAt + note.at + note.duration)

    oscillator.connect(gain)
    gain.connect(context.destination)

    oscillator.start(startAt + note.at)
    oscillator.stop(startAt + note.at + note.duration + 0.02)
  })
}

function playStepFeedback(step: WorkflowStep) {
  const sequenceMap: Record<WorkflowStep, Array<{ freq: number, at: number, duration: number }>> = {
    from: [
      { freq: 620, at: 0, duration: 0.12 },
    ],
    voltage: [
      { freq: 520, at: 0, duration: 0.07 },
      { freq: 520, at: 0.14, duration: 0.07 },
      { freq: 980, at: 0.31, duration: 0.14 },
    ],
    battery: [
      { freq: 820, at: 0, duration: 0.08 },
      { freq: 1120, at: 0.13, duration: 0.11 },
    ],
    to: [
      { freq: 480, at: 0, duration: 0.06 },
      { freq: 620, at: 0.1, duration: 0.06 },
      { freq: 780, at: 0.2, duration: 0.06 },
      { freq: 1180, at: 0.31, duration: 0.12 },
    ],
    save: [
      { freq: 660, at: 0, duration: 0.08 },
      { freq: 880, at: 0.11, duration: 0.08 },
      { freq: 1174, at: 0.23, duration: 0.12 },
      { freq: 1568, at: 0.38, duration: 0.26 },
    ],
  }

  const vibrationMap: Record<WorkflowStep, number | number[]> = {
    from: 60,
    battery: [45, 70, 45],
    voltage: [40, 60, 40, 100, 90],
    to: [35, 45, 35, 45, 35, 90, 70],
    save: [60, 70, 60, 70, 120],
  }

  if (typeof navigator !== 'undefined' && 'vibrate' in navigator) {
    navigator.vibrate(vibrationMap[step])
  }

  playToneSequence(sequenceMap[step], step === 'save' ? 0.18 : 0.12)
}

function playFlowCompleteFeedback() {
  if (typeof navigator !== 'undefined' && 'vibrate' in navigator) {
    navigator.vibrate([100, 80, 120, 80, 180])
  }

  playToneSequence([
    { freq: 784, at: 0, duration: 0.12 },
    { freq: 988, at: 0.14, duration: 0.12 },
    { freq: 1174, at: 0.28, duration: 0.14 },
    { freq: 1568, at: 0.44, duration: 0.34 },
  ], 0.2)
}

function loadLocalFallbackRecords() {
  if (typeof localStorage === 'undefined') {
    return []
  }

  const raw = localStorage.getItem(STORAGE_KEY)

  if (!raw) {
    return []
  }

  try {
    return JSON.parse(raw) as MovementRecord[]
  }
  catch {
    return []
  }
}

function saveLocalFallbackRecords(nextRecords: MovementRecord[]) {
  if (typeof localStorage === 'undefined') {
    return
  }

  localStorage.setItem(STORAGE_KEY, JSON.stringify(nextRecords))
}

function normalizeRecord(record: Record<string, any>): MovementRecord {
  return {
    id: String(record.id),
    stage: normalizeOperation(record.operation ?? record.stage),
    fromRack: String(record.fromRack ?? ''),
    fromSlot: String(record.fromSlot ?? ''),
    batterySn: String(record.batterySn ?? ''),
    voltage: typeof record.voltage === 'number' ? record.voltage : Number.parseFloat(String(record.voltage)),
    voltageMeasuredAt: String(record.voltageMeasuredAt ?? ''),
    toRack: String(record.toRack ?? ''),
    toSlot: String(record.toSlot ?? ''),
    savedAt: String(record.createdAt ?? record.savedAt ?? new Date().toISOString()),
    firmwareVersion: record.firmwareVersion ? String(record.firmwareVersion) : null,
    syncStatus: record.syncStatus === 'local' ? 'local' : 'db',
  }
}

function sortRecordsDescending(nextRecords: MovementRecord[]) {
  return [...nextRecords].sort((left, right) => {
    return new Date(right.savedAt).getTime() - new Date(left.savedAt).getTime()
  })
}

async function loadRecords() {
  const fallbackRecords = loadLocalFallbackRecords()

  try {
    const response = await $fetch<{ ok: boolean, records: Array<Record<string, any>> }>(API_ENDPOINT)
    records.value = sortRecordsDescending([
      ...fallbackRecords,
      ...response.records.map(normalizeRecord),
    ])
    log.value = `Loaded ${response.records.length} DB records${fallbackRecords.length ? ` + ${fallbackRecords.length} local fallback` : ''}`
  }
  catch {
    records.value = fallbackRecords

    if (fallbackRecords.length) {
      log.value = `DB unavailable. Showing ${fallbackRecords.length} local fallback records`
    }
  }
}

function loadCountOffset() {
  if (typeof localStorage === 'undefined') {
    return
  }

  const raw = localStorage.getItem(COUNT_OFFSET_KEY)
  const value = Number.parseInt(raw ?? '0', 10)
  countOffset.value = Number.isFinite(value) ? value : 0
}

function saveCountOffset() {
  if (typeof localStorage === 'undefined') {
    return
  }

  localStorage.setItem(COUNT_OFFSET_KEY, String(countOffset.value))
}

function resetCount() {
  countOffset.value = records.value.length
  saveCountOffset()
  log.value = 'Count reset for this device session'
}

function resetDraft() {
  draft.fromRack = ''
  draft.fromSlot = ''
  draft.batterySn = ''
  draft.voltage = null
  draft.voltageMeasuredAt = ''
  draft.toRack = ''
  draft.toSlot = ''
  manualVoltage.value = ''
  manualScanValue.value = ''
  scanField.value = 'from'
  lastScanSource.value = null
  lastVoltageSource.value = null
}

function clearDraft() {
  resetDraft()
  esp32LinkState.value = 'disconnected'
  esp32StatusMessage.value = bleConnected.value ? 'Draft cleared. Waiting for STEP 3' : 'Draft cleared. Connect BLE device first'
  log.value = `Current ${getOperationLabel(stage.value)} draft cleared`
  focusScannerCapture()
}

function setStage(nextStage: MovementStage) {
  ensureAudioContext()
  stage.value = nextStage
  draft.stage = nextStage
  resetDraft()
  log.value = `Switched to ${getOperationLabel(nextStage)}. Start with FROM rack/slot.`
  focusScannerCapture()
}

function resetEsp32ReadingState() {
  esp32LastVoltage = null
  esp32StableHits = 0
  blePayloadDeviceId = null
  blePayloadFirmwareVersion = null
}

function extractFirmwareVersion(payload: Record<string, any>) {
  const candidates = [
    payload.fw,
    payload.firmwareVersion,
    payload.data?.fw,
  ]

  for (const candidate of candidates) {
    const normalized = String(candidate ?? '').trim()

    if (normalized) {
      return normalized
    }
  }

  return null
}

function extractVoltageValue(payload: Record<string, any>) {
  const candidates = [
    payload.v,
    payload.voltage,
    payload.value,
    payload.reading,
    payload.data?.voltage,
    payload.data?.value,
  ]

  for (const candidate of candidates) {
    const parsed = typeof candidate === 'number' ? candidate : Number.parseFloat(String(candidate))

    if (!Number.isNaN(parsed)) {
      return parsed
    }
  }

  return null
}

function payloadIndicatesStable(payload: Record<string, any>, voltage: number | null) {
  if (
    payload.stable === true
    || payload.st === 1
    || payload.ready === true
    || payload.state === 'ready'
    || payload.status === 'ready'
    || payload.s === 'ready'
  ) {
    return true
  }

  if (
    payload.triggered === false
    || payload.tr === 0
    || payload.switchPressed === false
    || payload.contact === false
    || payload.probeContact === false
  ) {
    return false
  }

  if (
    voltage !== null
    && voltage >= 2
    && esp32StableHits >= 2
    && (
      payload.tr === 1
      || payload.triggered !== false
      || payload.switchPressed === true
      || payload.contact === true
      || payload.probeContact === true
      || payload.measureEnabled === true
    )
  ) {
    return true
  }

  return false
}

function disconnectBleDevice() {
  if (bleCharacteristic) {
    try {
      bleCharacteristic.removeEventListener('characteristicvaluechanged', handleBleMeasurementEvent)
    }
    catch {
      // noop
    }
  }

  if (bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect()
  }

  bleCharacteristic = null
  bleServer = null
  bleDevice = null
  bleConnected.value = false
  bleDeviceName.value = BLE_DEVICE_NAME
  resetEsp32ReadingState()
}

function handleBleDisconnect() {
  disconnectBleDevice()
  esp32LinkState.value = 'disconnected'
  esp32StatusMessage.value = 'BLE device disconnected'
}

function consumeBleMeasurementPayload(payload: Record<string, any>) {
  if (workflowStep.value !== 'voltage' || draft.voltage !== null) {
    return
  }

  const payloadDeviceId = String(payload.id ?? '').trim()

  if (payloadDeviceId && !payloadDeviceId.startsWith(BLE_DEVICE_ID_PREFIX)) {
    esp32LinkState.value = 'error'
    esp32StatusMessage.value = `Unexpected device id: ${payloadDeviceId}`
    return
  }

  if (payloadDeviceId) {
    blePayloadDeviceId = payloadDeviceId
  }

  blePayloadFirmwareVersion = extractFirmwareVersion(payload)

  const voltage = extractVoltageValue(payload)

  if (voltage === null) {
    resetEsp32ReadingState()
    esp32LinkState.value = 'measuring'
    esp32StatusMessage.value = 'BLE connected, waiting for voltage payload'
    return
  }

  if (esp32LastVoltage !== null && Math.abs(esp32LastVoltage - voltage) <= 0.12) {
    esp32StableHits += 1
  }
  else {
    esp32StableHits = 1
  }

  esp32LastVoltage = voltage

  if (!payloadIndicatesStable(payload, voltage)) {
    esp32LinkState.value = 'measuring'
    esp32StatusMessage.value = `Waiting stable BLE value (${voltage.toFixed(2)}V • hit ${esp32StableHits}/2)`
    return
  }

  esp32LinkState.value = 'ready'
  esp32StatusMessage.value = `${voltage.toFixed(2)}V captured from ${blePayloadDeviceId || BLE_DEVICE_ID_PREFIX}`
  setVoltage(voltage, 'esp32')
}

function parseBleMeasurement(rawValue: string) {
  try {
    const normalized = rawValue.trim()
    const start = normalized.indexOf('{')
    const end = normalized.lastIndexOf('}')
    const candidate = start >= 0 && end > start ? normalized.slice(start, end + 1) : normalized
    const payload = JSON.parse(candidate) as Record<string, any>
    consumeBleMeasurementPayload(payload)
  }
  catch {
    if (workflowStep.value !== 'voltage' || draft.voltage !== null) {
      return
    }

    esp32LinkState.value = 'error'
    esp32StatusMessage.value = 'BLE payload is not valid JSON'
  }
}

function handleBleMeasurementEvent(event: Event) {
  if (workflowStep.value !== 'voltage' || draft.voltage !== null) {
    return
  }

  void readBleMeasurementOnce()
}

async function readBleMeasurementOnce() {
  if (!bleCharacteristic) {
    return
  }

  const value = await bleCharacteristic.readValue()
  const raw = textDecoder.decode(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength))
  parseBleMeasurement(raw)
}

async function connectBleDevice() {
  if (typeof window === 'undefined') {
    return
  }

  if (!bleSupported.value) {
    esp32LinkState.value = 'error'
    esp32StatusMessage.value = 'Web Bluetooth is not supported in this browser'
    return
  }

  try {
    esp32LinkState.value = 'connecting'
    esp32StatusMessage.value = 'Select BLE device'

    bleDevice = await navigator.bluetooth.requestDevice({
      filters: [
        { services: [BLE_SERVICE_UUID] },
        { namePrefix: BLE_DEVICE_NAME_PREFIX },
      ],
      optionalServices: [BLE_SERVICE_UUID],
    })

    bleDeviceName.value = bleDevice.name || BLE_DEVICE_NAME
    bleDevice.addEventListener('gattserverdisconnected', handleBleDisconnect)

    bleServer = await bleDevice.gatt?.connect() ?? null
    if (!bleServer) {
      throw new Error('BLE GATT connection failed')
    }

    const service = await bleServer.getPrimaryService(BLE_SERVICE_UUID)
    bleCharacteristic = await service.getCharacteristic(BLE_CHARACTERISTIC_UUID)
    bleCharacteristic.addEventListener('characteristicvaluechanged', handleBleMeasurementEvent)
    await bleCharacteristic.startNotifications()

    bleConnected.value = true
    resetEsp32ReadingState()
    esp32LinkState.value = workflowStep.value === 'voltage' ? 'measuring' : 'disconnected'
    esp32StatusMessage.value = `${bleDeviceName.value} connected`
    log.value = `Connected BLE device ${bleDeviceName.value}`
    await readBleMeasurementOnce()
  }
  catch (error) {
    disconnectBleDevice()
    const message = error instanceof Error ? error.message : 'Unable to connect BLE device'
    esp32LinkState.value = 'error'
    esp32StatusMessage.value = message
  }
}

function parseRackSlot(value: string) {
  const trimmed = value.trim().toUpperCase()
  const normalized = trimmed.replace(/\s+/g, '')

  const separators = [':', '|', '/', '@', '#']

  for (const separator of separators) {
    const separatorIndex = normalized.lastIndexOf(separator)

    if (separatorIndex > 0 && separatorIndex < normalized.length - 1) {
      const rack = normalized.slice(0, separatorIndex)
      const slot = normalized.slice(separatorIndex + 1)

      if (rack && slot) {
        return {
          rack,
          slot,
        }
      }
    }
  }

  const fallbackParts = normalized.split(/[:|/@#]+/).filter(Boolean)

  if (fallbackParts.length >= 2) {
    return {
      rack: fallbackParts.slice(0, -1).join('-'),
      slot: fallbackParts.at(-1) ?? 'S01',
    }
  }

  return {
    rack: normalized,
    slot: 'S01',
  }
}

function resolveScanField(): ScanField {
  if (workflowStep.value === 'from') {
    return 'from'
  }

  if (workflowStep.value === 'battery') {
    return 'battery'
  }

  if (workflowStep.value === 'to') {
    return 'to'
  }

  return scanField.value
}

async function finalizeIfReady() {
  if (!canSave.value) {
    return
  }

  if (isSavingRecord.value) {
    return
  }

  isSavingRecord.value = true

  const payload = {
    operation: stage.value,
    fromRack: draft.fromRack,
    fromSlot: draft.fromSlot,
    batterySn: draft.batterySn,
    voltage: draft.voltage,
    voltageMeasuredAt: draft.voltageMeasuredAt || null,
    toRack: draft.toRack,
    toSlot: draft.toSlot,
    scanSource: lastScanSource.value,
    voltageSource: lastVoltageSource.value,
    deviceId: lastVoltageSource.value === 'esp32' ? blePayloadDeviceId : null,
    firmwareVersion: lastVoltageSource.value === 'esp32' ? blePayloadFirmwareVersion : null,
  }

  try {
    const response = await $fetch<{ ok: boolean, record: Record<string, any> }>(API_ENDPOINT, {
      method: 'POST',
      body: payload,
    })

    records.value.unshift(normalizeRecord(response.record))
    log.value = `Auto-saved ${getOperationLabel(stage.value)} movement to DB for battery ${draft.batterySn}`
  }
  catch (error) {
    const message = error instanceof Error ? error.message : 'Unknown DB save error'

    const fallbackRecord: MovementRecord = {
      id: `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
      stage: stage.value,
      fromRack: draft.fromRack,
      fromSlot: draft.fromSlot,
      batterySn: draft.batterySn,
      voltage: draft.voltage,
      voltageMeasuredAt: draft.voltageMeasuredAt,
      toRack: draft.toRack,
      toSlot: draft.toSlot,
      savedAt: new Date().toISOString(),
      firmwareVersion: blePayloadFirmwareVersion,
      syncStatus: 'local',
    }

    records.value.unshift(fallbackRecord)
    saveLocalFallbackRecords(records.value.filter(record => record.syncStatus === 'local'))
    log.value = `DB save failed for ${getOperationLabel(stage.value)} battery ${draft.batterySn}: ${message}. Stored local fallback instead.`
  }

  playFlowCompleteFeedback()
  resetDraft()
  isSavingRecord.value = false
}

function applyScan(value: string, source: ScanSource = 'manual') {
  const normalized = value.trim()

  if (!normalized) {
    return
  }

  lastScanSource.value = source

  const targetField = resolveScanField()
  scanField.value = targetField

  if (targetField === 'battery') {
    draft.batterySn = normalized
    log.value = `Battery S/N set to ${normalized}`
    playStepFeedback('battery')

    if (!draft.toRack || !draft.toSlot) {
      scanField.value = 'to'
    }

    return
  }

  const parsed = parseRackSlot(normalized)

  if (targetField === 'from') {
    draft.fromRack = parsed.rack
    draft.fromSlot = parsed.slot
    log.value = `FROM location set to ${parsed.rack} / ${parsed.slot}`
    playStepFeedback('from')
    scanField.value = 'battery'
    return
  }

  draft.toRack = parsed.rack
  draft.toSlot = parsed.slot
  log.value = `TO location set to ${parsed.rack} / ${parsed.slot}`
  playStepFeedback('to')
  void finalizeIfReady()
}

function applyManualScan() {
  applyScan(manualScanValue.value, 'manual')
  manualScanValue.value = ''
}

function shouldKeepScannerFocus(target?: EventTarget | null) {
  const element = target as HTMLElement | null

  if (!element) {
    return true
  }

  const tagName = element.tagName

  if (tagName === 'INPUT' || tagName === 'TEXTAREA' || tagName === 'SELECT') {
    return false
  }

  if (element.isContentEditable) {
    return false
  }

  return true
}

function focusScannerCapture() {
  if (typeof window === 'undefined') {
    return
  }

  window.setTimeout(() => {
    scannerInput.value?.focus()
  }, 0)
}

function flushScannerInputValue(rawValue: string) {
  const value = rawValue.trim()

  if (value.length < 3) {
    return
  }

  applyScan(value, 'scanner')
}

function clearScannerInputBuffer() {
  if (scannerInputTimer) {
    clearTimeout(scannerInputTimer)
    scannerInputTimer = null
  }

  if (scannerInput.value) {
    scannerInput.value.value = ''
  }
}

function handleScannerInput(event: Event) {
  const target = event.target as HTMLInputElement | null
  const value = target?.value ?? ''

  if (scannerInputTimer) {
    clearTimeout(scannerInputTimer)
  }

  scannerInputTimer = window.setTimeout(() => {
    flushScannerInputValue(value)
    clearScannerInputBuffer()
  }, 120)
}

function handleScannerInputEnter(event: KeyboardEvent) {
  const target = event.target as HTMLInputElement | null
  const value = target?.value ?? ''

  if (!value.trim()) {
    return
  }

  event.preventDefault()
  flushScannerInputValue(value)
  clearScannerInputBuffer()
}

function handleWindowPointerFocus(event: Event) {
  if (!shouldKeepScannerFocus(event.target)) {
    return
  }

  focusScannerCapture()
}

function handleScannerKeydown(event: KeyboardEvent) {
  const target = event.target as HTMLElement | null
  const tagName = target?.tagName

  if (target === scannerInput.value) {
    return
  }

  if (tagName === 'INPUT' || tagName === 'TEXTAREA' || tagName === 'SELECT') {
    return
  }

  const now = Date.now()

  if (now - scannerLastInputAt > 140) {
    scannerBuffer = ''
  }

  scannerLastInputAt = now

  if (event.key === 'Enter') {
    const value = scannerBuffer.trim()
    scannerBuffer = ''

    if (value.length >= 3) {
      applyScan(value, 'scanner')
    }

    return
  }

  if (event.key.length === 1) {
    scannerBuffer += event.key
  }
}

function setVoltage(value: number, source: VoltageSource = 'manual') {
  draft.voltage = value
  draft.voltageMeasuredAt = new Date().toISOString()
  manualVoltage.value = value.toFixed(2)
  lastVoltageSource.value = source
  log.value = `Voltage captured: ${value.toFixed(2)}V`
  playStepFeedback('voltage')

  if (draft.batterySn && (!draft.toRack || !draft.toSlot)) {
    scanField.value = 'to'
  }
  else if (!draft.batterySn) {
    scanField.value = 'battery'
  }

  void finalizeIfReady()
}

function applyManualVoltage() {
  const value = Number.parseFloat(manualVoltage.value)

  if (Number.isNaN(value)) {
    log.value = 'Manual voltage is invalid'
    return
  }

  setVoltage(value, 'manual')
}

function readMockVoltage() {
  const base = stage.value === 'STOCK_TO_CHARGE' ? 11.8 : stage.value === 'CHARGE_TO_DELIVERY' ? 13.1 : 12.7
  const value = Number((base + Math.random() * 0.8).toFixed(2))
  setVoltage(value, 'mock')
}

function saveMovement() {
  if (!canSave.value) {
    log.value = 'Movement is incomplete'
    return
  }

  void finalizeIfReady()
}

async function clearRecords() {
  try {
    await $fetch(API_ENDPOINT, {
      method: 'DELETE',
    })
  }
  catch {
    // Keep going so test data can still be cleared locally.
  }

  records.value = []
  saveLocalFallbackRecords([])
  log.value = 'Movement history cleared'
}

async function getPreferredCameraStream() {
  const preferred = {
    video: {
      facingMode: { ideal: 'environment' },
      width: { ideal: 1280 },
      height: { ideal: 720 },
    },
  } satisfies MediaStreamConstraints

  try {
    return await navigator.mediaDevices.getUserMedia(preferred)
  }
  catch {
    return await navigator.mediaDevices.getUserMedia({ video: true })
  }
}

function stopCamera() {
  if (detectorTimer) {
    clearInterval(detectorTimer)
    detectorTimer = null
  }

  if (mediaStream) {
    mediaStream.getTracks().forEach(track => track.stop())
    mediaStream = null
  }
}

async function startCamera() {
  if (typeof window === 'undefined') {
    return
  }

  if (!navigator.mediaDevices?.getUserMedia) {
    log.value = 'Camera is not available in this browser'
    return
  }

  isStarting.value = true

  try {
    ensureAudioContext()
    stopCamera()
    mediaStream = await getPreferredCameraStream()

    if (video.value) {
      video.value.srcObject = mediaStream
      await video.value.play()
    }

    hasStarted.value = true
    log.value = 'Camera ready for scan'
    focusScannerCapture()

    if ('BarcodeDetector' in window) {
      const detector = new window.BarcodeDetector({
        formats: ['qr_code', 'code_128', 'ean_13', 'code_39'],
      })

      detectorTimer = window.setInterval(async () => {
        if (!video.value) {
          return
        }

        const now = Date.now()

        if (now - lastDetection < 1800) {
          return
        }

        try {
          const codes = await detector.detect(video.value)

          if (!codes.length) {
            return
          }

          const rawValue = codes[0]?.rawValue?.trim()

          if (!rawValue) {
            return
          }

          lastDetection = now
          applyScan(rawValue, 'camera')
        }
        catch {
          log.value = 'Barcode detector is running, but no code was accepted'
        }
      }, 350)
    }
    else {
      log.value = 'Camera ready. BarcodeDetector not supported, use manual scan input.'
    }
  }
  catch (error) {
    const message = error instanceof Error ? error.message : 'Unable to start camera'
    log.value = message
  }
  finally {
    isStarting.value = false
  }
}

onMounted(() => {
  void loadRecords()
  loadCountOffset()
  draft.stage = stage.value
  focusScannerCapture()
  window.addEventListener('keydown', handleScannerKeydown)
  window.addEventListener('pointerdown', handleWindowPointerFocus)
  window.addEventListener('focus', focusScannerCapture)
})

watch(records, () => {
  if (countOffset.value > records.value.length) {
    countOffset.value = records.value.length
    saveCountOffset()
  }
}, { deep: true })

watch(workflowStep, (nextStep) => {
  if (nextStep === 'voltage' && draft.voltage === null) {
    if (!bleConnected.value) {
      esp32LinkState.value = 'disconnected'
      esp32StatusMessage.value = 'Tap Connect BLE before reading voltage'
      return
    }

    esp32LinkState.value = 'measuring'
    esp32StatusMessage.value = 'Waiting for BLE measurement'
    void readBleMeasurementOnce()
    return
  }

  if (nextStep === 'save') {
    esp32LinkState.value = 'ready'
    return
  }

  if (nextStep === 'voltage') {
    return
  }

  esp32LinkState.value = 'disconnected'
  esp32StatusMessage.value = bleConnected.value ? 'Voltage will be requested from BLE at STEP 3' : 'Connect BLE device, then wait for STEP 3'
}, { immediate: true })

onBeforeUnmount(() => {
  stopCamera()
  disconnectBleDevice()
  window.removeEventListener('keydown', handleScannerKeydown)
  window.removeEventListener('pointerdown', handleWindowPointerFocus)
  window.removeEventListener('focus', focusScannerCapture)

  if (scannerInputTimer) {
    clearTimeout(scannerInputTimer)
  }
})
</script>

<template>
  <main :class="['min-h-[100dvh] px-3 py-1 text-slate-950 transition-colors duration-300', workflowStepMeta.bg]">
    <input
      ref="scannerInput"
      type="text"
      autocomplete="off"
      autocapitalize="off"
      spellcheck="false"
      inputmode="none"
      class="pointer-events-none fixed -left-[9999px] top-0 h-0 w-0 opacity-0"
      @input="handleScannerInput"
      @keydown.enter="handleScannerInputEnter"
    />

    <section class="mx-auto flex max-w-md flex-col gap-2">
      <UCard
        :ui="{
          root: 'rounded-sm ring-0 bg-white/88 shadow-[0_18px_40px_rgba(15,23,42,0.08)]',
          body: 'px-2.5 py-3.5 sm:px-2.5 sm:py-3.5',
        }"
      >
        <div class="flex items-start justify-between gap-2">
          <div class="min-w-0 flex-1">
            <UBadge variant="soft" class="rounded-md px-3 py-1 text-sm font-bold uppercase tracking-[0.18em] ring-1" :class="stageMeta.badge">
              {{ stageMeta.index }}
            </UBadge>
            <h1 class="mt-1.5 text-[1.35rem] leading-none font-black tracking-tight whitespace-nowrap sm:text-[1.5rem]">
              Batt Movement BLE
            </h1>
          </div>

          <UBadge variant="soft" class="shrink-0 inline-flex items-center rounded-md border border-slate-200 bg-slate-50 px-3 py-1 text-right shadow-sm">
            <div class="flex items-center gap-2">
              <div class="text-md font-bold uppercase tracking-[0.18em] text-slate-500">Count</div>
              <div class="text-lg font-bold leading-none text-slate-950">{{ displayCount }}</div>
            </div>
          </UBadge>
        </div>
      </UCard>

      <div class="grid grid-cols-3 gap-1">
        <UButton
          v-for="option in stageOptions"
          :key="option"
          block
          size="md"
          color="neutral"
          variant="soft"
          :class="[
            'text-[11px] font-bold tracking-[0.08em]',
            stage === option ? stageMeta.action : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-50',
          ]"
          @click="setStage(option as MovementStage)"
        >
          {{ stageButtonLabels[option as MovementStage] }}
        </UButton>
      </div>

      <UCard
        class="text-center text-base font-bold shadow-sm"
        :class="workflowStepMeta.status"
        :ui="{ root: 'rounded-md ring-0', body: 'px-3 py-2 sm:px-3 sm:py-2' }"
      >
        {{ currentStep }}
      </UCard>

      <UCard
        :ui="{
          root: 'rounded-md ring-0 bg-slate-700 shadow-[0_16px_32px_rgba(15,23,42,0.18)]',
          body: 'p-1 sm:p-1',
        }"
      >
        <div
          class="relative overflow-hidden rounded-md bg-slate-700"
          style="position: relative; overflow: hidden; background: #334155; height: 24dvh; min-height: 170px; max-height: 220px;"
        >
          <video
            ref="video"
            autoplay
            muted
            playsinline
            style="position:absolute; inset:0; width:100%; height:100%; object-fit:cover;"
          />

          <div style="pointer-events:none; position:absolute; inset:0;">
            <div style="position:absolute; left:12px; right:12px; top:12px; border-radius:10px; background:rgba(30,41,59,0.75); padding:8px 12px; text-align:center; color:white; font-size:11px; font-weight:600;">
              Scan target: {{ scanTargetLabel }}
            </div>

            <div style="position:absolute; inset:0; display:flex; align-items:center; justify-content:center; padding:0 16px;">
              <div style="position:relative; width:100%; max-width:280px; height:30%; border-radius:20px; border:2px solid rgba(190,242,100,0.95); box-shadow:0 0 0 9999px rgba(51,65,85,0.24);">
                <div style="position:absolute; left:-2px; top:-2px; width:26px; height:26px; border-left:4px solid white; border-top:4px solid white; border-top-left-radius:16px;" />
                <div style="position:absolute; right:-2px; top:-2px; width:26px; height:26px; border-right:4px solid white; border-top:4px solid white; border-top-right-radius:16px;" />
                <div style="position:absolute; left:-2px; bottom:-2px; width:26px; height:26px; border-left:4px solid white; border-bottom:4px solid white; border-bottom-left-radius:16px;" />
                <div style="position:absolute; right:-2px; bottom:-2px; width:26px; height:26px; border-right:4px solid white; border-bottom:4px solid white; border-bottom-right-radius:16px;" />
              </div>
            </div>
          </div>
        </div>
      </UCard>

      <canvas ref="canvas" class="hidden" />

      <UCard
        :ui="{
          root: `rounded-md ring-0 shadow-[0_18px_36px_rgba(15,23,42,0.08)] ${workflowStepMeta.panel}`,
          body: 'p-2 sm:p-2',
        }"
      >
        <div class="grid grid-cols-2 gap-1 text-[11px]">
          <UCard :ui="{ root: 'rounded-sm ring-0 bg-white shadow-[0_2px_8px_rgba(15,23,42,0.06)]', body: 'px-2.5 py-2 sm:px-2.5 sm:py-2' }">
            <div class="text-slate-500">From Rack</div>
            <div class="mt-0.5 truncate text-sm font-bold text-slate-950">{{ draft.fromRack || '-' }}</div>
          </UCard>
          <UCard :ui="{ root: 'rounded-sm ring-0 bg-white shadow-[0_2px_8px_rgba(15,23,42,0.06)]', body: 'px-2.5 py-2 sm:px-2.5 sm:py-2' }">
            <div class="text-slate-500">From Slot</div>
            <div class="mt-0.5 truncate text-sm font-bold text-slate-950">{{ draft.fromSlot || '-' }}</div>
          </UCard>
          <UCard :ui="{ root: 'rounded-sm ring-0 bg-white shadow-[0_2px_8px_rgba(15,23,42,0.06)]', body: 'px-2.5 py-2 sm:px-2.5 sm:py-2' }">
            <div class="text-slate-500">Battery S/N</div>
            <div class="mt-0.5 truncate text-sm font-bold text-slate-950">{{ draft.batterySn || '-' }}</div>
          </UCard>
          <UCard :ui="{ root: 'rounded-sm ring-0 bg-white shadow-[0_2px_8px_rgba(15,23,42,0.06)]', body: 'px-2.5 py-2 sm:px-2.5 sm:py-2' }">
            <div class="text-slate-500">Voltage</div>
            <div class="mt-0.5 truncate text-sm font-bold text-slate-950">{{ draft.voltage !== null ? `${draft.voltage}V` : '-' }}</div>
          </UCard>
          <UCard :ui="{ root: 'rounded-sm ring-0 bg-white shadow-[0_2px_8px_rgba(15,23,42,0.06)]', body: 'px-2.5 py-2 sm:px-2.5 sm:py-2' }">
            <div class="text-slate-500">To Rack</div>
            <div class="mt-0.5 truncate text-sm font-bold text-slate-950">{{ draft.toRack || '-' }}</div>
          </UCard>
          <UCard :ui="{ root: 'rounded-sm ring-0 bg-white shadow-[0_2px_8px_rgba(15,23,42,0.06)]', body: 'px-2.5 py-2 sm:px-2.5 sm:py-2' }">
            <div class="text-slate-500">To Slot</div>
            <div class="mt-0.5 truncate text-sm font-bold text-slate-950">{{ draft.toSlot || '-' }}</div>
          </UCard>
        </div>

        <UCard :ui="{ root: 'mt-2 rounded-md bg-white/85 text-slate-700 shadow-[0_2px_8px_rgba(15,23,42,0.05)]', body: 'px-3 py-2 sm:px-3 sm:py-2' }">
          <div class="flex items-center justify-between gap-2 text-[11px] font-semibold">
            <span class="uppercase tracking-[0.18em] text-slate-500">Input Mode</span>
            <span class="text-right text-slate-950">{{ captureDeviceStatus }}</span>
          </div>
          <div class="mt-1 flex items-center justify-between gap-2 text-[11px] text-slate-600">
            <span>{{ captureSupportDetail }}</span>
            <span class="rounded-md bg-slate-100 px-2 py-1 font-semibold text-slate-950">Last Scan: {{ lastScanSourceLabel }}</span>
          </div>
        </UCard>

        <UCard :ui="{ root: 'mt-2 rounded-md bg-white/85 text-slate-700 shadow-[0_2px_8px_rgba(15,23,42,0.05)]', body: 'px-3 py-2 sm:px-3 sm:py-2' }">
          <div class="flex items-start justify-between gap-2">
            <div class="min-w-0">
              <div class="text-[11px] font-semibold uppercase tracking-[0.18em] text-slate-500">BLE ESP32 Status</div>
              <div class="mt-0.5 text-sm font-bold" :class="esp32Status.tone">{{ esp32Status.title }}</div>
              <div class="text-[11px] text-slate-600">{{ esp32Status.detail }}</div>
            </div>
            <span class="shrink-0 rounded-md px-2 py-1 text-[11px] font-bold" :class="esp32Status.chip">
              {{ bleTransportStatusLabel }}
            </span>
          </div>
          <div class="mt-1 text-[11px] text-slate-600">
            Browser support:
            <span class="font-semibold text-slate-950">{{ bleSupported ? 'Web Bluetooth Ready' : 'Web Bluetooth Missing' }}</span>
          </div>
          <div class="mt-1 text-[11px] text-slate-600">
            BLE device:
            <span class="font-semibold text-slate-950">{{ bleConnectionLabel }}</span>
          </div>
          <div class="mt-1 text-[11px] text-slate-600">
            BLE identity:
            <span class="font-semibold text-slate-950">{{ bleIdentityDetail }}</span>
          </div>
          <div class="mt-2 grid grid-cols-2 gap-2">
              <UButton
                size="sm"
                color="neutral"
                :disabled="bleConnected"
                class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-50"
                @click="connectBleDevice"
              >
                {{ bleConnected ? 'BLE Connected' : 'Connect BLE' }}
              </UButton>
              <UButton
                size="sm"
                color="neutral"
                :disabled="!bleConnected"
                class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-50"
                @click="disconnectBleDevice"
              >
                Disconnect
              </UButton>
            </div>
          <div class="mt-2 text-[11px] text-slate-600">
            Last voltage source:
            <span class="font-semibold text-slate-950">{{ lastVoltageSourceLabel }}</span>
          </div>
          <div class="mt-2 text-[11px] text-slate-600">
            BLE requirement:
            <span class="font-semibold text-slate-950">
              {{ bleSupported ? 'Use Chrome/Android and grant Bluetooth permission' : 'Browser may not support Web Bluetooth or page is not in a secure context' }}
            </span>
          </div>
        </UCard>

        <div class="mt-2">
          <UButton
            block
            size="lg"
            color="neutral"
            variant="solid"
            icon="i-lucide-camera"
            :loading="isStarting"
            :class="hasStarted
              ? 'border border-slate-800 bg-slate-800 text-white shadow-[0_10px_24px_rgba(15,23,42,0.18)] hover:bg-slate-700'
              : 'border border-slate-900 bg-slate-900 text-white shadow-[0_10px_24px_rgba(15,23,42,0.18)] hover:bg-slate-800'"
            @click="startCamera"
          >
            {{ hasStarted ? 'Restart Camera' : 'Start Camera' }}
          </UButton>
        </div>

        <div class="mt-2 grid grid-cols-1 gap-1">
          <UButton
            block
            size="lg"
            color="neutral"
            variant="soft"
            class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50"
            @click="resetCount"
          >
            Reset Count
          </UButton>

          <UButton
            block
            size="lg"
            color="neutral"
            variant="soft"
            class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50"
            @click="clearDraft"
          >
            Clear Draft
          </UButton>

          <UButton
            block
            size="lg"
            color="neutral"
            variant="soft"
            class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50"
            @click="clearRecords"
          >
            Clear History
          </UButton>
        </div>

        <UCard :ui="{ root: 'mt-2 rounded-md bg-slate-100 text-[11px] text-slate-700', body: 'px-3 py-2 sm:px-3 sm:py-2' }">
          {{ log }}
        </UCard>
      </UCard>

      <UCard
        :ui="{
          root: `rounded-md ring-0 shadow-[0_14px_30px_rgba(15,23,42,0.06)] ${workflowStepMeta.panel}`,
          body: 'p-2 sm:p-2',
        }"
      >
        <div class="flex items-center justify-between gap-2">
          <div class="text-[10px] font-semibold uppercase tracking-[0.2em] text-slate-500">
            Simulate Test
          </div>
          <UButton
            size="sm"
            color="neutral"
            variant="soft"
            class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50"
            @click="showSimulateTest = !showSimulateTest"
          >
            {{ showSimulateTest ? 'Hide' : 'Show' }}
          </UButton>
        </div>

        <div v-if="showSimulateTest" class="mt-2">
          <UCard :ui="{ root: 'rounded-md ring-0 bg-slate-50 shadow-[0_2px_8px_rgba(15,23,42,0.05)]', body: 'p-2.5 sm:p-2.5' }">
            <div class="mb-2 text-[10px] font-semibold uppercase tracking-[0.18em] text-slate-500">
              Manual Scan
            </div>
            <div class="flex items-center gap-2">
              <UInput
                v-model="manualScanValue"
                placeholder="RACK-A01:S02 or BAT-000123"
                class="w-full"
                :ui="{ base: '!min-h-10 !bg-white !px-3 !py-2 !text-sm !text-slate-950 !placeholder:text-slate-400' }"
              />
              <UButton
                color="neutral"
                variant="soft"
                :class="stageMeta.action"
                @click="applyManualScan"
              >
                Apply
              </UButton>
            </div>
          </UCard>

          <UCard :ui="{ root: 'mt-2 rounded-md ring-0 bg-slate-50 shadow-[0_2px_8px_rgba(15,23,42,0.05)]', body: 'p-2.5 sm:p-2.5' }">
            <div class="mb-2 text-[10px] font-semibold uppercase tracking-[0.18em] text-slate-500">
              Voltage Input
            </div>
            <div class="grid grid-cols-[1fr_auto_auto] gap-2">
              <UInput
                v-model="manualVoltage"
                type="number"
                step="0.01"
                placeholder="12.45"
                :ui="{ base: '!min-h-10 !bg-white !px-3 !py-2 !text-sm !text-slate-950 !placeholder:text-slate-400' }"
              />
              <UButton
                color="neutral"
                variant="soft"
                :class="stageMeta.soft"
                @click="readMockVoltage"
              >
                Mock
              </UButton>
              <UButton
                color="neutral"
                variant="soft"
                :class="stageMeta.action"
                @click="applyManualVoltage"
              >
                Save V
              </UButton>
            </div>
          </UCard>
        </div>
      </UCard>
    </section>
  </main>
</template>
