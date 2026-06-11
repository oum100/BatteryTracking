import { prisma } from '../../utils/prisma'

type MovementStage = 'PRE_CHARGE' | 'AFTER_CHARGE' | 'DELIVERY'

interface MovementPayload {
  stage?: MovementStage
  fromRack?: string
  fromSlot?: string
  batterySn?: string
  voltage?: number
  voltageMeasuredAt?: string
  toRack?: string
  toSlot?: string
  scanSource?: string
  voltageSource?: string
  deviceId?: string
  notes?: string
}

function ensureText(value: unknown, field: string) {
  const text = String(value ?? '').trim()

  if (!text) {
    throw createError({
      statusCode: 400,
      statusMessage: `${field} is required`,
    })
  }

  return text
}

function ensureStage(value: unknown) {
  const stage = String(value ?? '').trim() as MovementStage

  if (!['PRE_CHARGE', 'AFTER_CHARGE', 'DELIVERY'].includes(stage)) {
    throw createError({
      statusCode: 400,
      statusMessage: 'stage is invalid',
    })
  }

  return stage
}

function ensureVoltage(value: unknown) {
  const voltage = typeof value === 'number' ? value : Number.parseFloat(String(value))

  if (Number.isNaN(voltage)) {
    throw createError({
      statusCode: 400,
      statusMessage: 'voltage is invalid',
    })
  }

  return voltage
}

export default defineEventHandler(async (event) => {
  const body = await readBody<MovementPayload>(event)

  const stage = ensureStage(body.stage)
  const fromRack = ensureText(body.fromRack, 'fromRack')
  const fromSlot = ensureText(body.fromSlot, 'fromSlot')
  const batterySn = ensureText(body.batterySn, 'batterySn')
  const voltage = ensureVoltage(body.voltage)
  const toRack = ensureText(body.toRack, 'toRack')
  const toSlot = ensureText(body.toSlot, 'toSlot')

  const record = await prisma.batteryMovement.create({
    data: {
      stage,
      fromRack,
      fromSlot,
      batterySn,
      voltage,
      voltageMeasuredAt: body.voltageMeasuredAt ? new Date(body.voltageMeasuredAt) : null,
      toRack,
      toSlot,
      scanSource: body.scanSource?.trim() || null,
      voltageSource: body.voltageSource?.trim() || null,
      deviceId: body.deviceId?.trim() || null,
      notes: body.notes?.trim() || null,
    },
  })

  return {
    ok: true,
    record,
  }
})
