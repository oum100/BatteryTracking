import { prisma } from '../../utils/prisma'

type MovementOperation = 'STOCK_TO_CHARGE' | 'CHARGE_TO_DELIVERY' | 'DELIVERY_TRANSFER'

interface MovementPayload {
  operation?: MovementOperation
  stage?: string
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
  firmwareVersion?: string
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
  const body = await readBody<MovementPayload>(event)

  const operation = ensureOperation(body.operation ?? body.stage)
  const fromRack = ensureText(body.fromRack, 'fromRack')
  const fromSlot = ensureText(body.fromSlot, 'fromSlot')
  const batterySn = ensureText(body.batterySn, 'batterySn')
  const voltage = ensureVoltage(body.voltage)
  const toRack = ensureText(body.toRack, 'toRack')
  const toSlot = ensureText(body.toSlot, 'toSlot')

  const record = await prisma.batteryMovement.create({
    data: {
      stage: operation,
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
      firmwareVersion: body.firmwareVersion?.trim() || null,
      notes: body.notes?.trim() || null,
    },
  })

  return {
    ok: true,
    record,
  }
})
