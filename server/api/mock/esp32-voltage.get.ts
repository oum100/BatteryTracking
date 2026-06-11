type MockMode = 'auto' | 'idle' | 'unstable' | 'ready'

let autoTick = 0

function parseNumber(value: unknown) {
  const parsed = Number.parseFloat(String(value))
  return Number.isFinite(parsed) ? parsed : null
}

function parseBoolean(value: unknown) {
  if (value === undefined) {
    return null
  }

  const normalized = String(value).trim().toLowerCase()

  if (['1', 'true', 'yes', 'on'].includes(normalized)) {
    return true
  }

  if (['0', 'false', 'no', 'off'].includes(normalized)) {
    return false
  }

  return null
}

function nextAutoPayload() {
  autoTick += 1
  const phase = autoTick % 5

  if (phase === 1) {
    return {
      status: 'idle',
      state: 'idle',
      stable: false,
      triggered: false,
      switchPressed: false,
      contact: false,
      probeContact: false,
      measureEnabled: false,
      voltage: 0,
    }
  }

  if (phase === 2) {
    return {
      status: 'measuring',
      state: 'measuring',
      stable: false,
      triggered: true,
      switchPressed: true,
      contact: true,
      probeContact: true,
      measureEnabled: true,
      voltage: 1.28,
    }
  }

  if (phase === 3) {
    return {
      status: 'measuring',
      state: 'measuring',
      stable: false,
      triggered: true,
      switchPressed: true,
      contact: true,
      probeContact: true,
      measureEnabled: true,
      voltage: 12.41,
    }
  }

  return {
    status: 'ready',
    state: 'ready',
    stable: true,
    triggered: true,
    switchPressed: true,
    contact: true,
    probeContact: true,
    measureEnabled: true,
    voltage: 12.44,
  }
}

export default defineEventHandler((event) => {
  const query = getQuery(event)
  const mode = (String(query.mode || 'auto').toLowerCase() as MockMode)
  const manualVoltage = parseNumber(query.voltage)
  const manualStable = parseBoolean(query.stable)
  const manualContact = parseBoolean(query.contact)

  let payload = nextAutoPayload()

  if (mode === 'idle') {
    payload = {
      status: 'idle',
      state: 'idle',
      stable: false,
      triggered: false,
      switchPressed: false,
      contact: false,
      probeContact: false,
      measureEnabled: false,
      voltage: 0,
    }
  }

  if (mode === 'unstable') {
    payload = {
      status: 'measuring',
      state: 'measuring',
      stable: false,
      triggered: true,
      switchPressed: true,
      contact: true,
      probeContact: true,
      measureEnabled: true,
      voltage: 1.36,
    }
  }

  if (mode === 'ready') {
    payload = {
      status: 'ready',
      state: 'ready',
      stable: true,
      triggered: true,
      switchPressed: true,
      contact: true,
      probeContact: true,
      measureEnabled: true,
      voltage: 12.45,
    }
  }

  if (manualVoltage !== null) {
    payload.voltage = manualVoltage
  }

  if (manualStable !== null) {
    payload.stable = manualStable
    payload.status = manualStable ? 'ready' : 'measuring'
    payload.state = manualStable ? 'ready' : 'measuring'
  }

  if (manualContact !== null) {
    payload.triggered = manualContact
    payload.switchPressed = manualContact
    payload.contact = manualContact
    payload.probeContact = manualContact
    payload.measureEnabled = manualContact
  }

  return {
    ok: true,
    mock: true,
    deviceId: 'ESP32-MOCK-01',
    sampledAt: new Date().toISOString(),
    ...payload,
    hints: {
      examples: [
        '/api/mock/esp32-voltage',
        '/api/mock/esp32-voltage?mode=ready',
        '/api/mock/esp32-voltage?mode=unstable',
        '/api/mock/esp32-voltage?mode=ready&voltage=12.67',
        '/api/mock/esp32-voltage?mode=idle&contact=0',
      ],
    },
  }
})
