<script setup lang="ts">
type SessionMode = 'PRE_CHARGE' | 'POST_CHARGE'
type ScanTarget = 'source' | 'battery' | 'charger'

interface TransferItem {
  id: string
  batteryCode: string
  voltage: number
  imageDataUrl: string
  capturedAt: string
  fromPalletId: string
  toPalletId: string
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

const video = useTemplateRef<HTMLVideoElement>('video')
const canvas = useTemplateRef<HTMLCanvasElement>('canvas')

const mode = ref<SessionMode>('PRE_CHARGE')
const sourcePalletId = ref('')
const batteryCode = ref('')
const chargerPalletId = ref('')
const voltage = ref<number | null>(null)
const shot = ref('')
const log = ref('Start camera, then scan Source Pallet to begin a round.')
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

let detectorTimer: ReturnType<typeof setInterval> | null = null
let mediaStream: MediaStream | null = null
let audioContext: AudioContext | null = null
let lastCapture = 0

function generateId() {
  return `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
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

function playSuccessFeedback() {
  if (navigator.vibrate) {
    navigator.vibrate([100, 60, 100])
  }

  if (!audioContext || audioContext.state !== 'running') {
    feedbackState.value = 'Action completed, but sound is unavailable'
    return
  }

  const startAt = audioContext.currentTime
  const oscillator = audioContext.createOscillator()
  const gainNode = audioContext.createGain()

  oscillator.type = 'triangle'
  oscillator.frequency.setValueAtTime(880, startAt)
  oscillator.frequency.setValueAtTime(1320, startAt + 0.08)

  gainNode.gain.setValueAtTime(0.0001, startAt)
  gainNode.gain.exponentialRampToValueAtTime(0.2, startAt + 0.02)
  gainNode.gain.exponentialRampToValueAtTime(0.0001, startAt + 0.2)

  oscillator.connect(gainNode)
  gainNode.connect(audioContext.destination)
  oscillator.start(startAt)
  oscillator.stop(startAt + 0.22)
  feedbackState.value = 'Played sound and vibration'
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

function resetCurrentItem() {
  batteryCode.value = ''
  chargerPalletId.value = ''
  voltage.value = null
  shot.value = ''
  scanTarget.value = 'battery'
  scanGuide.value = 'Scan the next battery code, then read voltage and scan charger pallet.'
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
    scanGuide.value = 'Scan source pallet QR to begin or change the round.'
  }
  else if (target === 'battery') {
    scanGuide.value = 'Scan battery QR or barcode, then place the probe on the battery.'
  }
  else {
    scanGuide.value = 'Scan charger pallet QR to pair this battery with its destination pallet.'
  }
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
  return true
}

function applyScannedCode(code: string) {
  if (scanTarget.value === 'source') {
    sourcePalletId.value = code
    startRoundIfNeeded()
    log.value = `Source pallet scanned: ${code}`
    selectScanTarget('battery')
  }
  else if (scanTarget.value === 'battery') {
    batteryCode.value = code
    captureCurrentFrame()
    log.value = `Battery scanned: ${code}`
    scanGuide.value = 'Battery captured. Read voltage, then scan charger pallet.'
    playSuccessFeedback()
  }
  else {
    chargerPalletId.value = code
    log.value = `Charger pallet scanned: ${code}`
    scanGuide.value = 'Destination pallet ready. Save this transfer item when voltage is confirmed.'
    playSuccessFeedback()
  }
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

function readMockVoltage() {
  const mock = Number((12 + Math.random() * 1.6).toFixed(2))
  voltage.value = mock
  log.value = `Mock voltage read: ${mock}V`
  scanGuide.value = 'Voltage ready. Scan charger pallet or save if everything is complete.'
  playSuccessFeedback()
}

function canSaveItem() {
  return Boolean(
    sourcePalletId.value
    && batteryCode.value
    && chargerPalletId.value
    && voltage.value !== null
    && shot.value,
  )
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

  if (!chargerPalletId.value) {
    log.value = 'Please scan charger pallet before saving.'
    return
  }

  if (!shot.value) {
    log.value = 'Please capture battery image before saving.'
    return
  }

  if (items.value.some(item => item.batteryCode === batteryCode.value)) {
    log.value = `Battery ${batteryCode.value} is already saved in this round.`
    return
  }

  const item: TransferItem = {
    id: generateId(),
    batteryCode: batteryCode.value,
    voltage: voltage.value,
    imageDataUrl: shot.value,
    capturedAt: new Date().toISOString(),
    fromPalletId: sourcePalletId.value,
    toPalletId: chargerPalletId.value,
    sequence: currentSequence.value,
  }

  items.value.push(item)
  currentSequence.value += 1
  log.value = `Saved item ${item.sequence}: ${item.batteryCode} (${item.voltage}V)`
  playSuccessFeedback()
  resetCurrentItem()
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

  const session: TransferSessionRecord = {
    id: generateId(),
    mode: mode.value,
    sourcePalletId: sourcePalletId.value,
    chargerPalletId: items.value[items.value.length - 1]?.toPalletId ?? '',
    startedAt: sessionStartedAt.value,
    closedAt: new Date().toISOString(),
    itemCount: items.value.length,
    items: items.value.map(item => ({ ...item })),
  }

  savedSessions.value.unshift(session)
  persistSessions()

  log.value = `Finished round ${session.id} with ${session.itemCount} batteries.`
  playSuccessFeedback()

  sourcePalletId.value = ''
  sessionStartedAt.value = ''
  currentSequence.value = 1
  items.value = []
  resetCurrentItem()
  selectScanTarget('source')
}

function clearSavedSessions() {
  savedSessions.value = []
  persistSessions()
  log.value = 'Cleared locally saved mock sessions.'
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

    mediaStream = await navigator.mediaDevices.getUserMedia({
      video: {
        facingMode: 'environment',
        width: { ideal: 1280 },
        height: { ideal: 720 },
      },
    })

    video.value.srcObject = mediaStream
    await video.value.play()
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
  <main class="mx-auto flex min-h-screen max-w-7xl px-4 py-6 sm:px-8">
    <section class="grid w-full gap-6 lg:grid-cols-[1.1fr_0.9fr]">
      <div class="space-y-6 rounded-[2rem] border border-white/70 bg-white/85 p-5 shadow-[0_24px_80px_rgba(68,87,45,0.14)] backdrop-blur sm:p-7">
        <div class="flex flex-wrap items-center justify-between gap-3">
          <div>
            <p class="text-sm font-semibold uppercase tracking-[0.2em] text-lime-800/80">
              Workflow Mock
            </p>
            <h1 class="mt-2 text-3xl font-semibold tracking-tight text-slate-950">
              Battery Transfer Tracking
            </h1>
            <p class="mt-2 max-w-3xl text-sm leading-7 text-slate-600">
              Mock/local round flow for scanning source pallet, battery code, mock voltage, charger pallet, and saving each transfer item before the real ESP32 integration.
            </p>
          </div>

          <NuxtLink
            to="/camera-test"
            class="rounded-full bg-slate-950 px-4 py-2 text-sm font-medium text-white"
          >
            Back To Camera Test
          </NuxtLink>
        </div>

        <div class="grid gap-3 md:grid-cols-2 xl:grid-cols-4">
          <label class="rounded-2xl border border-slate-200 bg-slate-50 p-4 text-sm">
            <span class="block text-slate-500">Mode</span>
            <select
              v-model="mode"
              class="mt-2 w-full rounded-xl border border-slate-300 bg-white px-3 py-2 text-slate-900"
            >
              <option value="PRE_CHARGE">
                PRE_CHARGE
              </option>
              <option value="POST_CHARGE">
                POST_CHARGE
              </option>
            </select>
          </label>

          <div class="rounded-2xl border border-slate-200 bg-slate-50 p-4 text-sm">
            <span class="block text-slate-500">Permission</span>
            <p class="mt-2 font-semibold text-slate-950">
              {{ permissionState }}
            </p>
          </div>

          <div class="rounded-2xl border border-slate-200 bg-slate-50 p-4 text-sm">
            <span class="block text-slate-500">Current Sequence</span>
            <p class="mt-2 font-semibold text-slate-950">
              {{ currentSequence }}
            </p>
          </div>

          <div class="rounded-2xl border border-slate-200 bg-slate-50 p-4 text-sm">
            <span class="block text-slate-500">Audio Feedback</span>
            <p class="mt-2 font-semibold text-slate-950">
              {{ feedbackState }}
            </p>
          </div>
        </div>

        <div class="flex flex-wrap gap-3">
          <button
            type="button"
            class="rounded-full bg-slate-950 px-4 py-3 text-sm font-medium text-white"
            :disabled="isStarting"
            @click="startCamera"
          >
            {{ isStarting ? 'Starting Camera...' : hasStarted ? 'Restart Camera' : 'Start Camera' }}
          </button>

          <button
            type="button"
            class="rounded-full border border-slate-300 bg-white px-4 py-3 text-sm font-medium text-slate-800"
            @click="readPermissionState"
          >
            Check Permission
          </button>

          <button
            type="button"
            class="rounded-full border border-slate-300 bg-white px-4 py-3 text-sm font-medium text-slate-800"
            @click="readMockVoltage"
          >
            Read Mock Voltage
          </button>

          <button
            type="button"
            class="rounded-full border border-slate-300 bg-white px-4 py-3 text-sm font-medium text-slate-800"
            @click="captureCurrentFrame"
          >
            Capture Image
          </button>
        </div>

        <div class="grid gap-3 md:grid-cols-3">
          <button
            type="button"
            class="rounded-2xl px-4 py-3 text-left text-sm font-medium"
            :class="scanTarget === 'source' ? 'bg-lime-100 text-lime-950 ring-2 ring-lime-300' : 'bg-slate-100 text-slate-700'"
            @click="selectScanTarget('source')"
          >
            Scan Source Pallet
          </button>
          <button
            type="button"
            class="rounded-2xl px-4 py-3 text-left text-sm font-medium"
            :class="scanTarget === 'battery' ? 'bg-lime-100 text-lime-950 ring-2 ring-lime-300' : 'bg-slate-100 text-slate-700'"
            @click="selectScanTarget('battery')"
          >
            Scan Battery
          </button>
          <button
            type="button"
            class="rounded-2xl px-4 py-3 text-left text-sm font-medium"
            :class="scanTarget === 'charger' ? 'bg-lime-100 text-lime-950 ring-2 ring-lime-300' : 'bg-slate-100 text-slate-700'"
            @click="selectScanTarget('charger')"
          >
            Scan Charger Pallet
          </button>
        </div>

        <div
          class="relative overflow-hidden rounded-3xl border border-slate-200 bg-slate-950/90 aspect-[3/4] sm:aspect-video"
          style="position: relative; overflow: hidden; border-radius: 24px; border: 1px solid #cbd5e1; background: #020617; aspect-ratio: 3 / 4;"
        >
          <video
            ref="video"
            autoplay
            playsinline
            style="position: absolute; inset: 0; width: 100%; height: 100%; object-fit: cover;"
          />

          <div style="pointer-events: none; position: absolute; inset: 0;">
            <div style="position: absolute; left: 20px; right: 20px; top: 20px; border-radius: 999px; background: rgba(0, 0, 0, 0.45); padding: 10px 16px; text-align: center; color: white; font-size: 12px; font-weight: 600;">
              {{ scanGuide }}
            </div>

            <div style="position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; padding: 0 24px;">
              <div style="position: relative; width: 100%; max-width: 380px; height: 44%; border-radius: 32px; border: 2px solid rgba(190, 242, 100, 0.95); box-shadow: 0 0 0 9999px rgba(0, 0, 0, 0.28);">
                <div style="position: absolute; left: -2px; top: -2px; width: 40px; height: 40px; border-left: 4px solid white; border-top: 4px solid white; border-top-left-radius: 24px;" />
                <div style="position: absolute; right: -2px; top: -2px; width: 40px; height: 40px; border-right: 4px solid white; border-top: 4px solid white; border-top-right-radius: 24px;" />
                <div style="position: absolute; left: -2px; bottom: -2px; width: 40px; height: 40px; border-left: 4px solid white; border-bottom: 4px solid white; border-bottom-left-radius: 24px;" />
                <div style="position: absolute; right: -2px; bottom: -2px; width: 40px; height: 40px; border-right: 4px solid white; border-bottom: 4px solid white; border-bottom-right-radius: 24px;" />
                <div style="position: absolute; left: 16px; right: 16px; top: 50%; height: 2px; transform: translateY(-50%); background: rgba(190, 242, 100, 0.95);" />
                <div style="position: absolute; left: 50%; bottom: 12px; transform: translateX(-50%); border-radius: 999px; background: rgba(0, 0, 0, 0.55); padding: 4px 12px; font-size: 11px; font-weight: 600; color: white; white-space: nowrap;">
                  {{ scanTarget === 'source' ? 'Source Pallet' : scanTarget === 'battery' ? 'Battery Code' : 'Charger Pallet' }}
                </div>
              </div>
            </div>
          </div>
        </div>

        <canvas ref="canvas" class="hidden" />

        <div class="grid gap-4 xl:grid-cols-2">
          <div class="rounded-3xl border border-slate-200 bg-slate-50 p-5">
            <h2 class="text-lg font-semibold text-slate-950">
              Current Round
            </h2>
            <div class="mt-4 grid gap-3">
              <div class="rounded-2xl bg-white p-4">
                <p class="text-xs uppercase tracking-[0.18em] text-slate-500">
                  Source Pallet
                </p>
                <p class="mt-2 text-lg font-semibold text-slate-950">
                  {{ sourcePalletId || '-' }}
                </p>
              </div>
              <div class="rounded-2xl bg-white p-4">
                <p class="text-xs uppercase tracking-[0.18em] text-slate-500">
                  Battery Code
                </p>
                <p class="mt-2 text-lg font-semibold text-slate-950">
                  {{ batteryCode || '-' }}
                </p>
              </div>
              <div class="rounded-2xl bg-white p-4">
                <p class="text-xs uppercase tracking-[0.18em] text-slate-500">
                  Mock Voltage
                </p>
                <p class="mt-2 text-lg font-semibold text-slate-950">
                  {{ voltage !== null ? `${voltage}V` : '-' }}
                </p>
              </div>
              <div class="rounded-2xl bg-white p-4">
                <p class="text-xs uppercase tracking-[0.18em] text-slate-500">
                  Charger Pallet
                </p>
                <p class="mt-2 text-lg font-semibold text-slate-950">
                  {{ chargerPalletId || '-' }}
                </p>
              </div>
            </div>
          </div>

          <div class="rounded-3xl border border-slate-200 bg-slate-50 p-5">
            <h2 class="text-lg font-semibold text-slate-950">
              Image Preview
            </h2>
            <div class="mt-4 overflow-hidden rounded-2xl border border-slate-200 bg-white">
              <img
                v-if="shot"
                :src="shot"
                alt="Captured battery"
                class="h-72 w-full object-cover"
              >
              <div
                v-else
                class="flex h-72 items-center justify-center text-sm text-slate-400"
              >
                No image captured yet
              </div>
            </div>
          </div>
        </div>

        <div class="flex flex-wrap gap-3">
          <button
            type="button"
            class="rounded-full bg-lime-500 px-5 py-3 text-sm font-semibold text-lime-950 disabled:cursor-not-allowed disabled:opacity-50"
            :disabled="!canSaveItem()"
            @click="saveItem"
          >
            Save Item
          </button>

          <button
            type="button"
            class="rounded-full bg-slate-950 px-5 py-3 text-sm font-semibold text-white"
            :disabled="items.length === 0"
            @click="finishSession"
          >
            Finish Round
          </button>
        </div>

        <pre class="whitespace-pre-wrap rounded-2xl bg-slate-950 px-4 py-3 text-sm leading-6 text-slate-100">{{ log }}</pre>
      </div>

      <div class="space-y-6">
        <section class="rounded-[2rem] border border-white/70 bg-white/85 p-5 shadow-[0_24px_80px_rgba(68,87,45,0.14)] backdrop-blur sm:p-7">
          <div class="flex items-center justify-between gap-3">
            <div>
              <p class="text-sm font-semibold uppercase tracking-[0.2em] text-lime-800/80">
                Active Items
              </p>
              <h2 class="mt-2 text-2xl font-semibold text-slate-950">
                {{ items.length }} item(s) in current round
              </h2>
            </div>
          </div>

          <div class="mt-5 space-y-3">
            <div
              v-for="item in items"
              :key="item.id"
              class="rounded-2xl border border-slate-200 bg-slate-50 p-4"
            >
              <div class="flex items-start justify-between gap-3">
                <div>
                  <p class="text-xs uppercase tracking-[0.18em] text-slate-500">
                    Item {{ item.sequence }}
                  </p>
                  <p class="mt-1 text-lg font-semibold text-slate-950">
                    {{ item.batteryCode }}
                  </p>
                  <p class="mt-2 text-sm text-slate-600">
                    {{ item.voltage }}V · {{ item.fromPalletId }} -> {{ item.toPalletId }}
                  </p>
                </div>
                <img
                  :src="item.imageDataUrl"
                  alt="Saved battery capture"
                  class="h-20 w-20 rounded-2xl object-cover"
                >
              </div>
            </div>

            <div
              v-if="items.length === 0"
              class="rounded-2xl border border-dashed border-slate-300 p-6 text-sm text-slate-500"
            >
              No items saved in this round yet.
            </div>
          </div>
        </section>

        <section class="rounded-[2rem] border border-white/70 bg-white/85 p-5 shadow-[0_24px_80px_rgba(68,87,45,0.14)] backdrop-blur sm:p-7">
          <div class="flex items-center justify-between gap-3">
            <div>
              <p class="text-sm font-semibold uppercase tracking-[0.2em] text-lime-800/80">
                Local History
              </p>
              <h2 class="mt-2 text-2xl font-semibold text-slate-950">
                {{ savedSessions.length }} round(s) saved on this device
              </h2>
            </div>

            <button
              type="button"
              class="rounded-full border border-rose-300 bg-rose-50 px-4 py-2 text-sm font-medium text-rose-700"
              @click="clearSavedSessions"
            >
              Clear Local Data
            </button>
          </div>

          <div class="mt-5 space-y-3">
            <div
              v-for="session in savedSessions"
              :key="session.id"
              class="rounded-2xl border border-slate-200 bg-slate-50 p-4"
            >
              <p class="text-xs uppercase tracking-[0.18em] text-slate-500">
                {{ session.mode }} · {{ session.itemCount }} item(s)
              </p>
              <p class="mt-1 text-lg font-semibold text-slate-950">
                {{ session.sourcePalletId }} -> {{ session.chargerPalletId }}
              </p>
              <p class="mt-2 text-sm text-slate-600">
                Started {{ new Date(session.startedAt).toLocaleString() }}
              </p>
              <p class="text-sm text-slate-600">
                Closed {{ new Date(session.closedAt).toLocaleString() }}
              </p>
            </div>

            <div
              v-if="savedSessions.length === 0"
              class="rounded-2xl border border-dashed border-slate-300 p-6 text-sm text-slate-500"
            >
              No local rounds saved yet.
            </div>
          </div>
        </section>
      </div>
    </section>
  </main>
</template>
