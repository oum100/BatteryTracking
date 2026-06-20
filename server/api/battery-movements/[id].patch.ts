import { prisma } from '../../utils/prisma'

type MovementOperation = 'STOCK_TO_CHARGE' | 'CHARGE_TO_DELIVERY' | 'DELIVERY_TRANSFER'

interface MovementPayload {
  operation?: MovementOperation
  stage?: string
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
  firmwareVersion?: string | null
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

function ensureOperation(value: unknown) {
  const operation = String(value ?? '').trim()
  const normalized = {
    PRE_CHARGE: 'STOCK_TO_CHARGE',
    AFTER_CHARGE: 'CHARGE_TO_DELIVERY',
    DELIVERY: 'DELIVERY_TRANSFER',
  }[operation] ?? operation

  if (!['STOCK_TO_CHARGE', 'CHARGE_TO_DELIVERY', 'DELIVERY_TRANSFER'].includes(normalized)) {
    throw createError({
      statusCode: 400,
      statusMessage: 'operation is invalid',
    })
  }

  return normalized as MovementOperation
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
      stage: ensureOperation(body.operation ?? body.stage),
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
      firmwareVersion: body.firmwareVersion?.trim() || null,
      notes: body.notes?.trim() || null,
    },
  })

  return {
    ok: true,
    record,
  }
})
