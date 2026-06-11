<script setup lang="ts">
type SessionMode = 'PRE_CHARGE' | 'POST_CHARGE'
type ScanTarget = 'source' | 'battery' | 'target'
type WorkflowStep = 'source' | 'battery' | 'voltage' | 'target' | 'finish'
type FeedbackTone = 'source' | 'battery' | 'voltage' | 'save' | 'charger' | 'finish'

interface TransferItem {
  id: string
  batteryCode: string
  voltage: number
  imageDataUrl: string
  capturedAt: string
  sequence: number
}

interface TransferSessionRecord {
  id: string
  mode: SessionMode
  sourcePalletId: string
  chargerPalletId: string
  startedAt: string
  closedAt: string
  itemCount: number
  items: TransferItem[]
}

const STORAGE_KEY = 'battery-transfer-mock-sessions'
const modeOptions = ['PRE_CHARGE', 'POST_CHARGE']

const video = useTemplateRef<HTMLVideoElement>('video')
const canvas = useTemplateRef<HTMLCanvasElement>('canvas')

const mode = ref<SessionMode>('PRE_CHARGE')
const displayTheme = ref<'light' | 'dark'>('light')
const sourcePalletId = ref('')
const chargerPalletId = ref('')
const batteryCode = ref('')
const voltage = ref<number | null>(null)
const manualVoltage = ref('')
const shot = ref('')
const log = ref('Start camera, then scan source pallet to begin the round.')
const scanGuide = ref('Choose what to scan, then hold the code inside the frame.')
const scanTarget = ref<ScanTarget>('source')
const permissionState = ref('unknown')
const feedbackState = ref('Sound feedback ready')
const sessionStartedAt = ref('')
const currentSequence = ref(1)
const items = ref<TransferItem[]>([])
const savedSessions = ref<TransferSessionRecord[]>([])
const isStarting = ref(false)
const hasStarted = ref(false)
const canSaveItem = computed(() => Boolean(
  sourcePalletId.value
  && batteryCode.value
  && voltage.value !== null
  && shot.value,
))

const canFinishRound = computed(() => Boolean(
  sourcePalletId.value
  && chargerPalletId.value
  && items.value.length > 0,
))

const scanTargetLabel = computed(() => {
  if (scanTarget.value === 'source') {
    return 'Source Pallet'
  }

  if (scanTarget.value === 'battery') {
    return 'Battery Code'
  }

  return 'Target Pallet'
})

const workflowStep = computed<WorkflowStep>(() => {
  if (!sourcePalletId.value) {
    return 'source'
  }

  if (batteryCode.value && voltage.value === null) {
    return 'voltage'
  }

  if (batteryCode.value) {
    return 'battery'
  }

  if (!chargerPalletId.value) {
    return items.value.length > 0 ? 'target' : 'battery'
  }

  return 'finish'
})

const stepMeta = computed(() => {
  const map: Record<WorkflowStep, {
    index: string
    title: string
    detail: string
    bg: string
    panel: string
    accent: string
    stepBadgeClass: string
    statusClass: string
    button: string
    buttonClass: string
    softButtonClass: string
    utilityButtonClass: string
    inputWrapClass: string
    inputClass: string
  }> = {
    source: {
      index: 'STEP 1',
      title: 'Scan Source Pallet',
      detail: 'เริ่มรอบใหม่ด้วย pallet ต้นทาง',
      bg: 'bg-lime-200',
      panel: 'bg-lime-100/95',
      accent: 'text-lime-950',
      stepBadgeClass: 'bg-lime-200/90 text-lime-950 ring-lime-300/80',
      statusClass: 'border-lime-800/20 bg-lime-700 text-white',
      button: 'primary',
      buttonClass: 'bg-lime-900 text-white hover:bg-lime-800',
      softButtonClass: 'border border-lime-300/70 bg-lime-200/75 text-lime-950 hover:bg-lime-300/75',
      utilityButtonClass: 'border border-lime-300/70 bg-lime-100/88 text-lime-950 hover:bg-lime-200/88',
      inputWrapClass: 'border border-lime-300/70 bg-white/92',
      inputClass: '[&_input]:text-slate-950 [&_input]:placeholder:text-slate-400',
    },
    battery: {
      index: 'STEP 2',
      title: 'Scan Battery',
      detail: 'สแกนแบตก้อนถัดไป',
      bg: 'bg-emerald-200',
      panel: 'bg-emerald-100/95',
      accent: 'text-emerald-950',
      stepBadgeClass: 'bg-emerald-200/90 text-emerald-950 ring-emerald-300/80',
      statusClass: 'border-emerald-800/20 bg-emerald-700 text-white',
      button: 'success',
      buttonClass: 'bg-emerald-900 text-white hover:bg-emerald-800',
      softButtonClass: 'border border-emerald-300/70 bg-emerald-200/75 text-emerald-950 hover:bg-emerald-300/75',
      utilityButtonClass: 'border border-emerald-300/70 bg-emerald-100/88 text-emerald-950 hover:bg-emerald-200/88',
      inputWrapClass: 'border border-emerald-300/70 bg-white/92',
      inputClass: '[&_input]:text-slate-950 [&_input]:placeholder:text-slate-400',
    },
    voltage: {
      index: 'STEP 3',
      title: 'Read Voltage',
      detail: 'วัดแรงดันของแบตก้อนนี้',
      bg: 'bg-amber-200',
      panel: 'bg-amber-100/95',
      accent: 'text-yellow-950',
      stepBadgeClass: 'bg-amber-200/92 text-amber-950 ring-amber-300/80',
      statusClass: 'border-amber-800/20 bg-amber-700 text-white',
      button: 'warning',
      buttonClass: 'bg-amber-900 text-white hover:bg-amber-800',
      softButtonClass: 'border border-amber-300/75 bg-amber-200/78 text-amber-950 hover:bg-amber-300/78',
      utilityButtonClass: 'border border-amber-300/70 bg-amber-100/90 text-amber-950 hover:bg-amber-200/88',
      inputWrapClass: 'border border-amber-300/75 bg-white/94',
      inputClass: '[&_input]:text-slate-950 [&_input]:placeholder:text-slate-400',
    },
    target: {
      index: 'STEP 4',
      title: 'Scan Target Pallet',
      detail: 'สแกน pallet ปลายทาง แล้วระบบจะจบรอบให้อัตโนมัติ',
      bg: 'bg-violet-200',
      panel: 'bg-violet-100/95',
      accent: 'text-violet-950',
      stepBadgeClass: 'bg-violet-200/90 text-violet-950 ring-violet-300/80',
      statusClass: 'border-violet-800/20 bg-violet-700 text-white',
      button: 'secondary',
      buttonClass: 'bg-violet-900 text-white hover:bg-violet-800',
      softButtonClass: 'border border-violet-300/70 bg-violet-200/75 text-violet-950 hover:bg-violet-300/75',
      utilityButtonClass: 'border border-violet-300/70 bg-violet-100/88 text-violet-950 hover:bg-violet-200/88',
      inputWrapClass: 'border border-violet-300/70 bg-white/92',
      inputClass: '[&_input]:text-slate-950 [&_input]:placeholder:text-slate-400',
    },
    finish: {
      index: 'STEP 5',
      title: 'Round Finished',
      detail: 'บันทึกรอบนี้ลงเครื่องแล้ว',
      bg: 'bg-rose-200',
      panel: 'bg-rose-100/95',
      accent: 'text-rose-950',
      stepBadgeClass: 'bg-rose-200/90 text-rose-950 ring-rose-300/80',
      statusClass: 'border-rose-800/20 bg-rose-700 text-white',
      button: 'error',
      buttonClass: 'bg-rose-900 text-white hover:bg-rose-800',
      softButtonClass: 'border border-rose-300/70 bg-rose-200/75 text-rose-950 hover:bg-rose-300/75',
      utilityButtonClass: 'border border-rose-300/70 bg-rose-100/88 text-rose-950 hover:bg-rose-200/88',
      inputWrapClass: 'border border-rose-300/70 bg-white/92',
      inputClass: '[&_input]:text-slate-950 [&_input]:placeholder:text-slate-400',
    },
  }

  return map[workflowStep.value]
})

const themeUi = computed(() => {
  if (displayTheme.value === 'dark') {
    return {
      mainBg: 'bg-slate-950 text-slate-50',
      subtitle: 'text-slate-300/90',
      countCard: 'bg-slate-800 text-white ring-slate-700 shadow-[0_8px_20px_rgba(0,0,0,0.28)]',
      countLabel: 'text-slate-300',
      modeBadge: 'border border-slate-700 bg-slate-800 text-white',
      infoPanel: 'border-slate-700/80 bg-slate-900/72 shadow-[0_14px_28px_rgba(0,0,0,0.32)]',
      infoTile: 'border-slate-700 bg-slate-800 text-white shadow-[inset_0_1px_0_rgba(255,255,255,0.04)]',
      infoLabel: 'text-slate-300',
      infoValue: 'text-white',
      sectionCard: 'border-slate-700 bg-slate-900/58',
      testCard: 'border-slate-700 bg-slate-900/68',
      logCard: 'bg-slate-900/72 text-slate-200',
      themeWrap: 'border-slate-700 bg-slate-900/72',
      themeOff: 'border-slate-700 bg-slate-800 text-slate-200 hover:bg-slate-700',
      themeOn: 'border-slate-500 bg-white text-slate-950',
    }
  }

  return {
    mainBg: stepMeta.value.bg,
    subtitle: 'text-slate-700/85',
    countCard: 'bg-white/92 text-slate-950 ring-slate-200 shadow-[0_8px_20px_rgba(15,23,42,0.08)]',
    countLabel: 'text-slate-500',
    modeBadge: 'border border-white/85 bg-white/96 text-slate-950',
    infoPanel: 'border-white/90 bg-white/24 shadow-[0_12px_24px_rgba(15,23,42,0.1)]',
    infoTile: 'border-white/90 bg-white text-slate-950 shadow-[0_4px_12px_rgba(15,23,42,0.05)]',
    infoLabel: 'text-slate-600',
    infoValue: 'text-slate-950',
    sectionCard: 'border-white/80 bg-white/50',
    testCard: 'border-slate-300/65 bg-white/48',
    logCard: 'bg-white/56 text-slate-700',
    themeWrap: 'border-white/80 bg-white/50',
    themeOff: 'border-lime-300/70 bg-white/90 text-slate-700 hover:bg-white',
    themeOn: 'border-lime-400 bg-lime-700 text-white',
  }
})

const heroActionLabel = computed(() => {
  if (!hasStarted.value) {
    return 'Waiting For Camera'
  }

  if (workflowStep.value === 'source') {
    return 'Ready For Source Pallet'
  }

  if (workflowStep.value === 'battery') {
    return 'Ready For Battery Scan'
  }

  if (workflowStep.value === 'voltage') {
    return 'Read Voltage'
  }

  if (workflowStep.value === 'target') {
    return 'Ready For Target Pallet'
  }

  return 'Round Completed'
})

let detectorTimer: ReturnType<typeof setInterval> | null = null
let mediaStream: MediaStream | null = null
let audioContext: AudioContext | null = null
let lastCapture = 0

function generateId() {
  return `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
}

function waitForVideoReady(element: HTMLVideoElement) {
  return new Promise<void>((resolve) => {
    if (element.readyState >= HTMLMediaElement.HAVE_METADATA && element.videoWidth > 0) {
      resolve()
      return
    }

    const done = () => {
      element.removeEventListener('loadedmetadata', done)
      element.removeEventListener('canplay', done)
      resolve()
    }

    element.addEventListener('loadedmetadata', done, { once: true })
    element.addEventListener('canplay', done, { once: true })
  })
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
    const fallback = await navigator.mediaDevices.getUserMedia({ video: true })
    const devices = await navigator.mediaDevices.enumerateDevices()
    const rearCamera = devices.find(device =>
      device.kind === 'videoinput'
      && /(back|rear|environment)/i.test(device.label),
    )

    if (!rearCamera?.deviceId) {
      return fallback
    }

    fallback.getTracks().forEach(track => track.stop())

    return navigator.mediaDevices.getUserMedia({
      video: {
        deviceId: { exact: rearCamera.deviceId },
        width: { ideal: 1280 },
        height: { ideal: 720 },
      },
    })
  }
}

function stopCamera() {
  if (detectorTimer) {
    clearInterval(detectorTimer)
    detectorTimer = null
  }

  if (mediaStream) {
    for (const track of mediaStream.getTracks()) {
      track.stop()
    }

    mediaStream = null
  }
}

async function ensureAudioReady() {
  if (typeof window === 'undefined' || !('AudioContext' in window)) {
    feedbackState.value = 'Sound feedback unavailable on this browser'
    return
  }

  if (!audioContext) {
    audioContext = new window.AudioContext()
  }

  if (audioContext.state === 'suspended') {
    await audioContext.resume()
  }

  feedbackState.value = audioContext.state === 'running'
    ? 'Sound feedback ready'
    : 'Sound feedback blocked'
}

function playStepFeedback(tone: FeedbackTone) {
  if (navigator.vibrate) {
    const vibrationMap: Record<FeedbackTone, number[]> = {
      source: [80, 40, 80],
      battery: [50, 30, 50],
      voltage: [120],
      save: [70, 35, 70, 35, 70],
      charger: [120, 50, 120],
      finish: [140, 60, 140, 60, 220],
    }

    navigator.vibrate(vibrationMap[tone])
  }

  if (!audioContext || audioContext.state !== 'running') {
    feedbackState.value = 'Action completed, but sound is unavailable'
    return
  }

  const toneMap: Record<FeedbackTone, { type: OscillatorType, notes: Array<{ freq: number, at: number, duration: number }> }> = {
    source: {
      type: 'triangle',
      notes: [
        { freq: 540, at: 0, duration: 0.18 },
      ],
    },
    battery: {
      type: 'triangle',
      notes: [
        { freq: 820, at: 0, duration: 0.08 },
        { freq: 960, at: 0.14, duration: 0.08 },
      ],
    },
    voltage: {
      type: 'square',
      notes: [
        { freq: 760, at: 0, duration: 0.07 },
        { freq: 760, at: 0.14, duration: 0.07 },
        { freq: 1120, at: 0.29, duration: 0.05 },
      ],
    },
    save: {
      type: 'triangle',
      notes: [
        { freq: 659, at: 0, duration: 0.08 },
        { freq: 784, at: 0.1, duration: 0.08 },
        { freq: 988, at: 0.2, duration: 0.14 },
      ],
    },
    charger: {
      type: 'sawtooth',
      notes: [
        { freq: 660, at: 0, duration: 0.12 },
        { freq: 554, at: 0.15, duration: 0.12 },
      ],
    },
    finish: {
      type: 'sawtooth',
      notes: [
        { freq: 523, at: 0, duration: 0.12 },
        { freq: 659, at: 0.14, duration: 0.12 },
        { freq: 784, at: 0.28, duration: 0.14 },
        { freq: 988, at: 0.45, duration: 0.16 },
        { freq: 1319, at: 0.64, duration: 0.3 },
      ],
    },
  }

  const profile = toneMap[tone]
  const baseGain = tone === 'finish' ? 0.22 : 0.14

  for (const note of profile.notes) {
    const oscillator = audioContext.createOscillator()
    const gainNode = audioContext.createGain()
    const startAt = audioContext.currentTime + note.at
    const endAt = startAt + note.duration

    oscillator.type = profile.type
    oscillator.frequency.setValueAtTime(note.freq, startAt)

    gainNode.gain.setValueAtTime(0.0001, startAt)
    gainNode.gain.exponentialRampToValueAtTime(baseGain, startAt + 0.02)
    gainNode.gain.exponentialRampToValueAtTime(0.0001, endAt)

    oscillator.connect(gainNode)
    gainNode.connect(audioContext.destination)
    oscillator.start(startAt)
    oscillator.stop(endAt)
  }

  feedbackState.value = `Played ${tone} feedback`
}

function playSavedCountFeedback(count: number) {
  if (navigator.vibrate) {
    const pulses = Math.min(Math.max(count, 1), 4)
    const pattern: number[] = []

    for (let index = 0; index < pulses; index += 1) {
      pattern.push(60)
      if (index < pulses - 1) {
        pattern.push(40)
      }
    }

    navigator.vibrate(pattern)
  }

  if (!audioContext || audioContext.state !== 'running') {
    feedbackState.value = 'Saved battery, but count sound is unavailable'
    return
  }

  const pulses = Math.min(Math.max(count, 1), 4)
  const startBase = audioContext.currentTime

  for (let index = 0; index < pulses; index += 1) {
    const oscillator = audioContext.createOscillator()
    const gainNode = audioContext.createGain()
    const startAt = startBase + (index * 0.16)
    const endAt = startAt + 0.1
    const freq = 720 + (index * 110)

    oscillator.type = 'triangle'
    oscillator.frequency.setValueAtTime(freq, startAt)

    gainNode.gain.setValueAtTime(0.0001, startAt)
    gainNode.gain.exponentialRampToValueAtTime(0.16, startAt + 0.02)
    gainNode.gain.exponentialRampToValueAtTime(0.0001, endAt)

    oscillator.connect(gainNode)
    gainNode.connect(audioContext.destination)
    oscillator.start(startAt)
    oscillator.stop(endAt)
  }

  feedbackState.value = `Saved count feedback x${pulses}`
}

function persistSessions() {
  if (typeof window === 'undefined') {
    return
  }

  localStorage.setItem(STORAGE_KEY, JSON.stringify(savedSessions.value))
}

function loadSessions() {
  if (typeof window === 'undefined') {
    return
  }

  const raw = localStorage.getItem(STORAGE_KEY)

  if (!raw) {
    return
  }

  try {
    savedSessions.value = JSON.parse(raw) as TransferSessionRecord[]
  }
  catch {
    savedSessions.value = []
  }
}

function resetDraftItem() {
  batteryCode.value = ''
  voltage.value = null
  manualVoltage.value = ''
  shot.value = ''
  scanTarget.value = 'battery'
  scanGuide.value = 'Scan the next battery code, then read voltage and capture image.'
  log.value = 'Ready for the next battery.'
}

function resetWholeRound() {
  sourcePalletId.value = ''
  chargerPalletId.value = ''
  sessionStartedAt.value = ''
  currentSequence.value = 1
  items.value = []
  resetDraftItem()
  selectScanTarget('source')
}

function startRoundIfNeeded() {
  if (!sessionStartedAt.value) {
    sessionStartedAt.value = new Date().toISOString()
    log.value = `Round started for source pallet ${sourcePalletId.value}`
  }
}

function selectScanTarget(target: ScanTarget) {
  scanTarget.value = target

  if (target === 'source') {
    scanGuide.value = 'Scan source pallet QR to begin a new round.'
    return
  }

  if (target === 'battery') {
    scanGuide.value = 'Scan battery QR or barcode, then measure voltage on the current battery.'
    return
  }

  scanGuide.value = 'Scan target pallet QR to close this round automatically.'
}

function looksLikePalletCode(code: string) {
  return /(pallet|plt|charger|target|rack|zone|src|pst)/i.test(code)
}

function captureCurrentFrame() {
  if (!video.value || !canvas.value) {
    return false
  }

  canvas.value.width = video.value.videoWidth
  canvas.value.height = video.value.videoHeight

  const ctx = canvas.value.getContext('2d')

  if (!ctx) {
    log.value = 'Unable to create canvas context.'
    return false
  }

  ctx.drawImage(video.value, 0, 0)
  shot.value = canvas.value.toDataURL('image/jpeg', 0.85)
  log.value = batteryCode.value
    ? `Captured image for battery ${batteryCode.value}`
    : 'Captured image from current frame'
  return true
}

function applyScannedCode(code: string) {
  if (scanTarget.value === 'source') {
    if (items.value.length > 0 && sourcePalletId.value && sourcePalletId.value !== code) {
      log.value = 'Finish or clear the current round before starting a different source pallet.'
      return
    }

    sourcePalletId.value = code
    chargerPalletId.value = ''
    startRoundIfNeeded()
    log.value = `Source pallet scanned: ${code}`
    selectScanTarget('battery')
    playStepFeedback('source')
    return
  }

  if (scanTarget.value === 'battery') {
    if (!sourcePalletId.value) {
      log.value = 'Scan source pallet first.'
      selectScanTarget('source')
      return
    }

    if (!batteryCode.value && items.value.length > 0 && looksLikePalletCode(code)) {
      chargerPalletId.value = code
      log.value = `Target pallet scanned: ${code}`
      scanGuide.value = 'Target pallet captured. Closing round automatically.'
      playStepFeedback('charger')
      finishSession()
      return
    }

    if (items.value.some(item => item.batteryCode === code) || batteryCode.value === code) {
      log.value = `Battery ${code} already exists in this round.`
      return
    }

    batteryCode.value = code
    chargerPalletId.value = ''
    captureCurrentFrame()
    log.value = `Battery scanned: ${code}`
    scanGuide.value = 'Battery captured. Read voltage and the system will save automatically.'
    playStepFeedback('battery')
    return
  }

  if (!sourcePalletId.value) {
    log.value = 'Scan source pallet first.'
    selectScanTarget('source')
    return
  }

  if (batteryCode.value) {
    log.value = 'Finish the current battery before scanning target pallet.'
    return
  }

  chargerPalletId.value = code
  log.value = `Target pallet scanned: ${code}`
  scanGuide.value = 'Target pallet captured. Closing round automatically.'
  playStepFeedback('charger')
  finishSession()
}

async function readPermissionState() {
  if (!('permissions' in navigator) || !navigator.permissions?.query) {
    permissionState.value = 'unsupported'
    return
  }

  try {
    const status = await navigator.permissions.query({ name: 'camera' as PermissionName })
    permissionState.value = status.state
  }
  catch {
    permissionState.value = 'unsupported'
  }
}

function setVoltage(value: number) {
  voltage.value = Number(value.toFixed(2))
  manualVoltage.value = `${voltage.value}`
  log.value = `Voltage ready: ${voltage.value}V`
  scanGuide.value = 'Voltage ready. Saving this battery automatically.'
  playStepFeedback('voltage')

  if (canSaveItem.value) {
    saveItem()
  }
}

function readMockVoltage() {
  const mock = 12 + Math.random() * 1.6
  setVoltage(mock)
}

function applyManualVoltage() {
  const parsed = Number(manualVoltage.value)

  if (!Number.isFinite(parsed) || parsed <= 0) {
    log.value = 'Please enter a valid voltage value.'
    return
  }

  setVoltage(parsed)
}

function saveItem() {
  if (!sourcePalletId.value) {
    log.value = 'Please scan source pallet first.'
    return
  }

  if (!batteryCode.value) {
    log.value = 'Please scan battery code before saving.'
    return
  }

  if (voltage.value === null) {
    log.value = 'Please read voltage before saving.'
    return
  }

  if (!shot.value) {
    log.value = 'Please capture battery image before saving.'
    return
  }

  const item: TransferItem = {
    id: generateId(),
    batteryCode: batteryCode.value,
    voltage: voltage.value,
    imageDataUrl: shot.value,
    capturedAt: new Date().toISOString(),
    sequence: currentSequence.value,
  }

  items.value.push(item)
  currentSequence.value += 1
  log.value = `Saved item ${item.sequence}: ${item.batteryCode} (${item.voltage}V)`
  playSavedCountFeedback(item.sequence)
  resetDraftItem()
}
function finishSession() {
  if (!sourcePalletId.value || !sessionStartedAt.value) {
    log.value = 'No active round to finish.'
    return
  }

  if (items.value.length === 0) {
    log.value = 'Add at least one battery transfer item before finishing the round.'
    return
  }

  if (!chargerPalletId.value) {
    log.value = 'Scan target pallet before finishing the round.'
    selectScanTarget('target')
    return
  }

  const session: TransferSessionRecord = {
    id: generateId(),
    mode: mode.value,
    sourcePalletId: sourcePalletId.value,
    chargerPalletId: chargerPalletId.value,
    startedAt: sessionStartedAt.value,
    closedAt: new Date().toISOString(),
    itemCount: items.value.length,
    items: items.value.map(item => ({ ...item })),
  }

  savedSessions.value.unshift(session)
  persistSessions()

  log.value = `Finished round ${session.id} with ${session.itemCount} batteries from ${session.sourcePalletId} to ${session.chargerPalletId}.`
  playStepFeedback('finish')
  resetWholeRound()
}

async function runPrimaryAction() {
  if (!hasStarted.value) {
    await startCamera()
    return
  }

  if (workflowStep.value === 'voltage') {
    readMockVoltage()
    return
  }

  if (workflowStep.value === 'target') {
    selectScanTarget('target')
  }
}

async function startCamera() {
  if (!video.value) {
    return
  }

  isStarting.value = true

  if (!window.isSecureContext) {
    log.value = 'Camera requires HTTPS or localhost.'
    isStarting.value = false
    return
  }

  if (!navigator.mediaDevices?.getUserMedia) {
    log.value = 'This browser does not support getUserMedia.'
    isStarting.value = false
    return
  }

  try {
    await ensureAudioReady()
    stopCamera()

    mediaStream = await getPreferredCameraStream()
    video.value.setAttribute('playsinline', 'true')
    video.value.setAttribute('muted', 'true')
    video.value.muted = true
    video.value.autoplay = true
    video.value.srcObject = mediaStream
    await waitForVideoReady(video.value)
    await video.value.play()

    if (video.value.videoWidth === 0 || video.value.videoHeight === 0) {
      log.value = 'Camera permission granted, but preview is not rendering yet. Try tapping Restart Camera once.'
      return
    }

    hasStarted.value = true

    if (!('BarcodeDetector' in window)) {
      log.value = 'BarcodeDetector is not available. Use Android Chrome.'
      return
    }

    const detector = new window.BarcodeDetector({
      formats: ['qr_code', 'code_128', 'ean_13', 'code_39'],
    })

    detectorTimer = window.setInterval(async () => {
      if (!video.value) {
        return
      }

      try {
        const codes = await detector.detect(video.value)

        if (codes.length === 0) {
          return
        }

        const now = Date.now()

        if (now - lastCapture < 1800) {
          return
        }

        lastCapture = now
        applyScannedCode(codes[0].rawValue ?? 'unknown')
      }
      catch (error) {
        log.value = error instanceof Error ? error.message : 'Barcode detection failed'
      }
    }, 250)

    log.value = 'Camera ready. Select a scan target and scan a code.'
  }
  catch (error) {
    log.value = error instanceof Error ? error.message : 'Unable to start camera'
  }
  finally {
    isStarting.value = false
  }
}

onMounted(() => {
  loadSessions()
  readPermissionState()
  selectScanTarget('source')
})

onBeforeUnmount(() => {
  stopCamera()
})
</script>

<template>
  <main
    class="min-h-[100dvh] overflow-hidden px-3 py-1.5 transition-colors duration-300"
    :class="themeUi.mainBg"
  >
    <section class="mx-auto flex h-[calc(100dvh-0.75rem)] max-w-md flex-col gap-0.5">
      <header class="flex items-start justify-between gap-2 px-1">
        <div class="min-w-0">
          <div class="inline-flex items-center rounded-full px-3 py-1 text-sm font-bold uppercase tracking-[0.18em] shadow-sm ring-1" :class="stepMeta.stepBadgeClass">
            {{ stepMeta.index }}
          </div>
          <h1 class="mt-1 text-[1rem] font-bold leading-none tracking-tight" :class="displayTheme === 'dark' ? 'text-white' : 'text-slate-950'">
            {{ stepMeta.title }}
          </h1>
          <p class="mt-0.5 text-[12px] leading-tight" :class="themeUi.subtitle">
            {{ stepMeta.detail }}
          </p>
        </div>

        <div class="flex flex-col items-end gap-1">
          <div class="rounded-2xl px-3 py-1.5 text-right ring-1" :class="themeUi.countCard">
            <div class="flex items-baseline justify-end gap-2">
              <div class="text-[10px] font-semibold uppercase tracking-[0.2em]" :class="themeUi.countLabel">Count</div>
              <div class="text-lg font-black leading-none">{{ items.length }}</div>
            </div>
          </div>
          <UBadge
            color="neutral"
            variant="solid"
            class="px-3 py-1 text-sm font-bold"
            :class="themeUi.modeBadge"
          >
            {{ mode }}
          </UBadge>
        </div>
      </header>

      <div class="flex min-h-10 items-center justify-center rounded-xl border px-3 text-center text-lg font-semibold tracking-tight shadow-sm" :class="stepMeta.statusClass">
        {{ heroActionLabel }}
      </div>

      <div class="-mt-0.5 rounded-[1.2rem] border border-slate-600/35 bg-slate-700 p-2 shadow-[0_14px_36px_rgba(15,23,42,0.16)]">
        <div
          class="relative overflow-hidden rounded-[1rem] bg-slate-700"
          style="position: relative; overflow: hidden; background: #334155; height: 24dvh; min-height: 165px; max-height: 220px;"
        >
          <video
            ref="video"
            autoplay
            muted
            playsinline
            style="position: absolute; inset: 0; width: 100%; height: 100%; object-fit: cover;"
          />

          <div style="pointer-events: none; position: absolute; inset: 0;">
            <div style="position: absolute; left: 12px; right: 12px; top: 12px; border-radius: 999px; background: rgba(51, 65, 85, 0.82); padding: 8px 12px; text-align: center; color: white; font-size: 11px; font-weight: 600;">
              {{ scanGuide }}
            </div>

            <div style="position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; padding: 0 16px;">
              <div style="position: relative; width: 100%; max-width: 290px; height: 28%; border-radius: 22px; border: 2px solid rgba(190, 242, 100, 0.95); box-shadow: 0 0 0 9999px rgba(51, 65, 85, 0.24);">
                <div style="position: absolute; left: -2px; top: -2px; width: 28px; height: 28px; border-left: 4px solid white; border-top: 4px solid white; border-top-left-radius: 18px;" />
                <div style="position: absolute; right: -2px; top: -2px; width: 28px; height: 28px; border-right: 4px solid white; border-top: 4px solid white; border-top-right-radius: 18px;" />
                <div style="position: absolute; left: -2px; bottom: -2px; width: 28px; height: 28px; border-left: 4px solid white; border-bottom: 4px solid white; border-bottom-left-radius: 18px;" />
                <div style="position: absolute; right: -2px; bottom: -2px; width: 28px; height: 28px; border-right: 4px solid white; border-bottom: 4px solid white; border-bottom-right-radius: 18px;" />
                <div style="position: absolute; left: 16px; right: 16px; top: 50%; height: 2px; transform: translateY(-50%); background: rgba(190, 242, 100, 0.95);" />
                <div style="position: absolute; left: 50%; bottom: 8px; transform: translateX(-50%); border-radius: 999px; background: rgba(30, 41, 59, 0.62); padding: 3px 10px; font-size: 10px; font-weight: 600; color: white; white-space: nowrap;">
                  {{ scanTargetLabel }}
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <canvas ref="canvas" class="hidden" />

      <div class="-mt-1 flex-1 rounded-[1.2rem] border p-2.5 backdrop-blur" :class="[stepMeta.panel, themeUi.infoPanel]">
        <div class="flex h-full flex-col gap-1.5">
          <div class="grid grid-cols-2 gap-2 text-[11px]">
            <div class="rounded-2xl border p-2.5" :class="themeUi.infoTile">
              <div :class="themeUi.infoLabel">Source Pallet</div>
              <div class="mt-1 truncate font-semibold" :class="themeUi.infoValue">{{ sourcePalletId || '-' }}</div>
            </div>
            <div class="rounded-2xl border p-2.5" :class="themeUi.infoTile">
              <div :class="themeUi.infoLabel">Battery ID</div>
              <div class="mt-1 truncate font-semibold" :class="themeUi.infoValue">{{ batteryCode || '-' }}</div>
            </div>
            <div class="rounded-2xl border p-2.5" :class="themeUi.infoTile">
              <div :class="themeUi.infoLabel">Batt Voltage</div>
              <div class="mt-1 truncate font-semibold" :class="themeUi.infoValue">{{ voltage !== null ? `${voltage}V` : '-' }}</div>
            </div>
            <div class="rounded-2xl border p-2.5" :class="themeUi.infoTile">
              <div :class="themeUi.infoLabel">Target Pallet</div>
              <div class="mt-1 truncate font-semibold" :class="themeUi.infoValue">{{ chargerPalletId || '-' }}</div>
            </div>
          </div>

          <UButton
            block
            size="lg"
            color="neutral"
            variant="solid"
            icon="i-lucide-camera"
            :loading="isStarting"
            :class="hasStarted
              ? 'min-h-10 border border-white/45 bg-white/45 text-slate-700 hover:bg-white/60'
              : `min-h-10 ${stepMeta.softButtonClass}`"
            @click="startCamera"
          >
            {{ hasStarted ? 'Restart Camera' : 'Start Camera' }}
          </UButton>

          <div class="rounded-2xl border p-2" :class="themeUi.sectionCard">
            <div class="mb-2 px-1 text-[10px] font-semibold uppercase tracking-[0.18em]" :class="themeUi.countLabel">
              Scan Mode
            </div>
            <div class="grid grid-cols-3 gap-2">
              <UButton
                block
                size="md"
                :color="scanTarget === 'source' ? 'primary' : 'neutral'"
                :variant="scanTarget === 'source' ? 'solid' : 'soft'"
                :class="scanTarget === 'source' ? `border-none ${stepMeta.buttonClass}` : stepMeta.softButtonClass"
                @click="selectScanTarget('source')"
              >
                Source
              </UButton>
              <UButton
                block
                size="md"
                :color="scanTarget === 'battery' ? 'primary' : 'neutral'"
                :variant="scanTarget === 'battery' ? 'solid' : 'soft'"
                :class="scanTarget === 'battery' ? `border-none ${stepMeta.buttonClass}` : stepMeta.softButtonClass"
                @click="selectScanTarget('battery')"
              >
                Battery
              </UButton>
              <UButton
                block
                size="md"
                :color="scanTarget === 'target' ? 'primary' : 'neutral'"
                :variant="scanTarget === 'target' ? 'solid' : 'soft'"
                :class="scanTarget === 'target' ? `border-none ${stepMeta.buttonClass}` : stepMeta.softButtonClass"
                @click="selectScanTarget('target')"
              >
                Target
              </UButton>
            </div>
          </div>

          <div class="rounded-2xl border p-2" :class="themeUi.testCard">
            <div class="mb-2 px-1 text-[10px] font-semibold uppercase tracking-[0.18em]" :class="themeUi.countLabel">
              Test Tools
            </div>

            <div class="grid grid-cols-2 gap-2">
              <UButton
                block
                size="md"
                color="neutral"
                variant="soft"
                icon="i-lucide-zap"
                :class="stepMeta.utilityButtonClass"
                @click="readMockVoltage"
              >
                Mock Volt
              </UButton>
              <UButton
                block
                size="md"
                color="neutral"
                variant="soft"
                icon="i-lucide-image"
                :class="stepMeta.utilityButtonClass"
                @click="captureCurrentFrame"
              >
                Capture
              </UButton>
            </div>

            <div class="mt-2 flex items-center gap-2">
              <UInput
                v-model="manualVoltage"
                type="number"
                step="0.01"
                inputmode="decimal"
                placeholder="12.45"
                :ui="{ root: 'w-full', base: `!bg-white !text-slate-950 !placeholder:text-slate-400 ${stepMeta.inputClass}` }"
                :class="['w-full rounded-full bg-white', stepMeta.inputWrapClass]"
              />
              <UButton
                color="neutral"
                variant="soft"
                :class="stepMeta.buttonClass"
                @click="applyManualVoltage"
              >
                Apply
              </UButton>
            </div>
          </div>

          <div class="rounded-2xl border p-1.5" :class="themeUi.themeWrap">
            <div class="grid grid-cols-2 gap-2">
              <UButton
                block
                size="sm"
                color="neutral"
                variant="soft"
                :class="displayTheme === 'light' ? themeUi.themeOn : themeUi.themeOff"
                @click="displayTheme = 'light'"
              >
                Light
              </UButton>
              <UButton
                block
                size="sm"
                color="neutral"
                variant="soft"
                :class="displayTheme === 'dark' ? 'border-slate-500 bg-white text-slate-950' : themeUi.themeOff"
                @click="displayTheme = 'dark'"
              >
                Dark
              </UButton>
            </div>
          </div>

          <div class="mt-auto rounded-2xl px-3 py-2 text-[11px]" :class="themeUi.logCard">
            {{ log }}
          </div>
        </div>
      </div>
    </section>
  </main>
</template>
