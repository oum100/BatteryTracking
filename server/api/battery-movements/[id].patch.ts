import { prisma } from '../../utils/prisma'

type MovementStage = 'PRE_CHARGE' | 'AFTER_CHARGE' | 'DELIVERY'

interface MovementPayload {
  stage?: MovementStage
  fromRack?: string
  fromSlot?: string
  batterySn?: string
  voltage?: number
  voltageMeasuredAt?: string | null
  toRack?: string
  toSlot?: string
  scanSource?: string | null
  voltageSource?: string | null
  deviceId?: string | null
  notes?: string | null
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
  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const body = await readBody<MovementPayload>(event)

  const record = await prisma.batteryMovement.update({
    where: { id },
    data: {
      stage: ensureStage(body.stage),
      fromRack: ensureText(body.fromRack, 'fromRack'),
      fromSlot: ensureText(body.fromSlot, 'fromSlot'),
      batterySn: ensureText(body.batterySn, 'batterySn'),
      voltage: ensureVoltage(body.voltage),
      voltageMeasuredAt: body.voltageMeasuredAt ? new Date(body.voltageMeasuredAt) : null,
      toRack: ensureText(body.toRack, 'toRack'),
      toSlot: ensureText(body.toSlot, 'toSlot'),
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
