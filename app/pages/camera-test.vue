<script setup lang="ts">
const video = useTemplateRef<HTMLVideoElement>('video')
const canvas = useTemplateRef<HTMLCanvasElement>('canvas')

const log = ref('Starting...')
const shot = ref('')
const isStarting = ref(false)
const hasStarted = ref(false)
const permissionState = ref('unknown')
const feedbackState = ref('Sound feedback ready')
const scanGuide = ref('จัดโค้ดให้อยู่ในกรอบกลางจอ แล้วขยับเข้าออกช้า ๆ')

let lastCapture = 0
let detectorTimer: ReturnType<typeof setInterval> | null = null
let mediaStream: MediaStream | null = null
let audioContext: AudioContext | null = null

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
    navigator.vibrate([120, 80, 120])
  }

  if (!audioContext || audioContext.state !== 'running') {
    feedbackState.value = 'Scanned, but sound is unavailable'
    return
  }

  const startAt = audioContext.currentTime
  const oscillator = audioContext.createOscillator()
  const gainNode = audioContext.createGain()

  oscillator.type = 'sine'
  oscillator.frequency.setValueAtTime(880, startAt)
  oscillator.frequency.setValueAtTime(1174, startAt + 0.08)

  gainNode.gain.setValueAtTime(0.0001, startAt)
  gainNode.gain.exponentialRampToValueAtTime(0.18, startAt + 0.02)
  gainNode.gain.exponentialRampToValueAtTime(0.0001, startAt + 0.22)

  oscillator.connect(gainNode)
  gainNode.connect(audioContext.destination)

  oscillator.start(startAt)
  oscillator.stop(startAt + 0.24)
  feedbackState.value = 'Played sound and vibration'
}

function capture(value: string) {
  if (!video.value || !canvas.value) {
    return
  }

  canvas.value.width = video.value.videoWidth
  canvas.value.height = video.value.videoHeight

  const ctx = canvas.value.getContext('2d')

  if (!ctx) {
    log.value = 'ไม่สามารถสร้าง canvas context ได้'
    return
  }

  ctx.drawImage(video.value, 0, 0)
  shot.value = canvas.value.toDataURL('image/jpeg', 0.85)
  log.value = `Captured: ${value}\nTime: ${new Date().toLocaleString()}`
  scanGuide.value = 'สแกนสำเร็จแล้ว ถ้าจะอ่านชิ้นถัดไปให้เลื่อนโค้ดใหม่เข้ากรอบ'
  playSuccessFeedback()
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

async function start() {
  if (!video.value) {
    return
  }

  isStarting.value = true

  if (!window.isSecureContext) {
    log.value = 'Camera ใช้งานไม่ได้: หน้านี้ต้องเปิดผ่าน HTTPS หรือ localhost'
    isStarting.value = false
    return
  }

  if (!navigator.mediaDevices) {
    log.value = 'Camera ใช้งานไม่ได้: browser นี้ไม่มี navigator.mediaDevices'
    isStarting.value = false
    return
  }

  if (!navigator.mediaDevices.getUserMedia) {
    log.value = 'Camera ใช้งานไม่ได้: browser นี้ไม่มี getUserMedia'
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
        height: { ideal: 720 }
      }
    })

    video.value.srcObject = mediaStream
    await video.value.play()
    hasStarted.value = true

    if (!('BarcodeDetector' in window)) {
      log.value = 'Browser ไม่รองรับ BarcodeDetector ให้ใช้ Android Chrome'
      isStarting.value = false
      return
    }

    const detector = new window.BarcodeDetector({
      formats: ['qr_code', 'code_128', 'ean_13', 'code_39']
    })

    log.value = 'Camera ready. Waiting for barcode...'
    scanGuide.value = 'ให้โค้ดอยู่กลางกรอบ ถือให้นิ่ง และหลีกเลี่ยงแสงสะท้อน'

    detectorTimer = window.setInterval(async () => {
      if (!video.value) {
        return
      }

      try {
        const codes = await detector.detect(video.value)

        if (codes.length > 0) {
          const now = Date.now()

          if (now - lastCapture > 2000) {
            lastCapture = now
            capture(codes[0].rawValue ?? 'unknown')
          }
        }
        else {
          scanGuide.value = 'ยังไม่พบโค้ด ลองขยับเข้าใกล้ขึ้นหรือทำให้โค้ดตรงกับกล้องมากขึ้น'
        }
      }
      catch (error) {
        log.value = error instanceof Error ? error.message : 'Barcode detection failed'
      }
    }, 300)
  }
  catch (error) {
    if (error instanceof DOMException) {
      if (error.name === 'NotAllowedError') {
        log.value = 'Camera permission ถูกปฏิเสธ หรือ browser ยังไม่อนุญาตให้ใช้กล้อง ลองกดไอคอน lock/permission ของ browser แล้วอนุญาต Camera'
        permissionState.value = 'denied'
        return
      }

      if (error.name === 'NotFoundError') {
        log.value = 'ไม่พบกล้องบนอุปกรณ์นี้'
        return
      }

      if (error.name === 'NotReadableError') {
        log.value = 'กล้องกำลังถูกใช้งานโดยแอปอื่นอยู่'
        return
      }
    }

    log.value = error instanceof Error ? error.message : 'Unable to start camera'
  }
  finally {
    isStarting.value = false
  }
}

onMounted(() => {
  log.value = 'Tap "Start Camera" to request permission and begin scanning.'
  readPermissionState()
})

onBeforeUnmount(() => {
  stopCamera()
})
</script>

<template>
  <main class="mx-auto flex min-h-screen max-w-4xl px-6 py-10 sm:px-10">
    <section class="w-full space-y-6 rounded-[2rem] border border-white/70 bg-white/80 p-6 shadow-[0_24px_80px_rgba(68,87,45,0.14)] backdrop-blur sm:p-8">
      <div class="space-y-3">
        <NuxtLink
          to="/"
          class="inline-flex items-center rounded-full bg-slate-950 px-3 py-1.5 text-sm font-medium text-white"
        >
          Back Home
        </NuxtLink>
        <div>
          <p class="text-sm font-semibold uppercase tracking-[0.2em] text-lime-800/80">
            Camera Test
          </p>
          <h1 class="mt-2 text-3xl font-semibold tracking-tight text-slate-950 sm:text-4xl">
            Barcode capture test page
          </h1>
          <p class="mt-3 max-w-2xl text-sm leading-7 text-slate-600 sm:text-base">
            หน้านี้ใช้กล้องหลัง, ตรวจจับ barcode/QR code และ capture ภาพตัวอย่างเมื่อเจอโค้ดใหม่
          </p>
        </div>
      </div>

      <div class="flex flex-wrap items-center gap-3">
        <button
          type="button"
          class="rounded-full bg-slate-950 px-4 py-2 text-sm font-medium text-white disabled:cursor-not-allowed disabled:opacity-60"
          :disabled="isStarting"
          style="min-height: 48px; min-width: 140px; user-select: none; -webkit-user-select: none; touch-action: manipulation;"
          @click="start"
        >
          {{ isStarting ? 'Starting...' : hasStarted ? 'Restart Camera' : 'Start Camera' }}
        </button>

        <button
          type="button"
          class="rounded-full border border-slate-300 bg-white px-4 py-2 text-sm font-medium text-slate-800"
          style="min-height: 48px; min-width: 160px; user-select: none; -webkit-user-select: none; touch-action: manipulation;"
          @click="readPermissionState"
        >
          Check Permission
        </button>

        <span class="rounded-full bg-lime-50 px-3 py-1.5 text-sm text-lime-950">
          Permission: {{ permissionState }}
        </span>

        <span class="rounded-full bg-slate-100 px-3 py-1.5 text-sm text-slate-700">
          {{ feedbackState }}
        </span>
      </div>

      <div
        class="relative overflow-hidden rounded-3xl border border-slate-200 bg-slate-950/90 aspect-[3/4] sm:aspect-video"
        style="position: relative; overflow: hidden; border-radius: 24px; border: 1px solid #cbd5e1; background: #020617; aspect-ratio: 3 / 4;"
      >
        <video
          ref="video"
          autoplay
          playsinline
          class="absolute inset-0 h-full w-full object-cover"
          style="position: absolute; inset: 0; width: 100%; height: 100%; object-fit: cover;"
        />

        <div
          class="pointer-events-none absolute inset-0"
          style="pointer-events: none; position: absolute; inset: 0;"
        >
          <div
            class="absolute inset-x-5 top-5 rounded-full bg-black/45 px-4 py-2 text-center text-xs font-medium text-white backdrop-blur sm:inset-x-8 sm:text-sm"
            style="position: absolute; left: 20px; right: 20px; top: 20px; border-radius: 999px; background: rgba(0, 0, 0, 0.45); padding: 10px 16px; text-align: center; color: white; font-size: 12px; font-weight: 600;"
          >
            {{ scanGuide }}
          </div>

          <div
            class="absolute inset-0 flex items-center justify-center px-6"
            style="position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; padding: 0 24px;"
          >
            <div
              class="relative h-[44%] w-full max-w-sm rounded-[2rem] border-2 border-lime-300/90 shadow-[0_0_0_9999px_rgba(0,0,0,0.28)]"
              style="position: relative; width: 100%; max-width: 380px; height: 44%; border-radius: 32px; border: 2px solid rgba(190, 242, 100, 0.95); box-shadow: 0 0 0 9999px rgba(0, 0, 0, 0.28);"
            >
              <div style="position: absolute; left: -2px; top: -2px; width: 40px; height: 40px; border-left: 4px solid white; border-top: 4px solid white; border-top-left-radius: 24px;" />
              <div style="position: absolute; right: -2px; top: -2px; width: 40px; height: 40px; border-right: 4px solid white; border-top: 4px solid white; border-top-right-radius: 24px;" />
              <div style="position: absolute; left: -2px; bottom: -2px; width: 40px; height: 40px; border-left: 4px solid white; border-bottom: 4px solid white; border-bottom-left-radius: 24px;" />
              <div style="position: absolute; right: -2px; bottom: -2px; width: 40px; height: 40px; border-right: 4px solid white; border-bottom: 4px solid white; border-bottom-right-radius: 24px;" />

              <div style="position: absolute; left: 16px; right: 16px; top: 50%; height: 2px; transform: translateY(-50%); background: rgba(190, 242, 100, 0.95);" />
              <div
                class="absolute bottom-3 left-1/2 -translate-x-1/2 rounded-full bg-black/55 px-3 py-1 text-[11px] font-medium text-white"
                style="position: absolute; left: 50%; bottom: 12px; transform: translateX(-50%); border-radius: 999px; background: rgba(0, 0, 0, 0.55); padding: 4px 12px; font-size: 11px; font-weight: 600; color: white; white-space: nowrap;"
              >
                Center barcode or QR code here
              </div>
            </div>
          </div>

          <div
            class="absolute inset-x-5 bottom-5 grid gap-2 sm:grid-cols-3 sm:gap-3"
            style="position: absolute; left: 20px; right: 20px; bottom: 20px; display: grid; gap: 8px;"
          >
            <div
              class="rounded-2xl bg-black/45 px-3 py-2 text-xs text-white backdrop-blur"
              style="border-radius: 16px; background: rgba(0, 0, 0, 0.45); padding: 8px 12px; font-size: 12px; color: white;"
            >
              ระยะ: 10-20 ซม.
            </div>
            <div
              class="rounded-2xl bg-black/45 px-3 py-2 text-xs text-white backdrop-blur"
              style="border-radius: 16px; background: rgba(0, 0, 0, 0.45); padding: 8px 12px; font-size: 12px; color: white;"
            >
              มุม: ให้โค้ดตรงกับกล้อง
            </div>
            <div
              class="rounded-2xl bg-black/45 px-3 py-2 text-xs text-white backdrop-blur"
              style="border-radius: 16px; background: rgba(0, 0, 0, 0.45); padding: 8px 12px; font-size: 12px; color: white;"
            >
              แสง: หลีกเลี่ยงเงาและสะท้อน
            </div>
          </div>
        </div>
      </div>

      <canvas
        ref="canvas"
        class="hidden"
      />

      <pre
        class="whitespace-pre-wrap rounded-2xl bg-lime-50 px-4 py-3 text-sm leading-6 text-lime-950"
        style="white-space: pre-wrap;"
      >{{ log }}</pre>

      <img
        v-if="shot"
        :src="shot"
        alt="Captured frame"
        class="w-full rounded-3xl border border-slate-200 bg-white"
      >
    </section>
  </main>
</template>
