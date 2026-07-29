import { prisma } from '../../utils/prisma'
import { requireUserRole } from '../../utils/user-auth'

interface CalibrationPayload {
  deviceId?: string
  deviceName?: string | null
  source?: 'INA226' | 'INA228'
  inaGain?: number
  inaOffset?: number
  pointOneMeter?: number
  pointOneEsp?: number
  pointTwoMeter?: number
  pointTwoEsp?: number
  syncedAt?: string | null
}

function ensureText(value: unknown, field: string) {
  const normalized = String(value ?? '').trim()
  if (!normalized) {
    throw createError({
      statusCode: 400,
      statusMessage: `${field} is required`,
    })
  }

  return normalized
}

function ensureNumber(value: unknown, field: string) {
  const normalized = typeof value === 'number' ? value : Number.parseFloat(String(value ?? ''))
  if (!Number.isFinite(normalized)) {
    throw createError({
      statusCode: 400,
      statusMessage: `${field} must be a valid number`,
    })
  }

  return normalized
}

function ensureSource(value: unknown) {
  const source = String(value ?? 'INA226').trim().toUpperCase()
  if (source === 'INA226' || source === 'INA228') {
    return source
  }

  throw createError({
    statusCode: 400,
    statusMessage: 'source must be INA226 or INA228',
  })
}

export default defineEventHandler(async (event) => {
  await requireUserRole(event, ['ADMIN', 'CALIBRATOR'])
  const body = await readBody<CalibrationPayload>(event)

  const deviceId = ensureText(body.deviceId, 'deviceId')
  const deviceName = String(body.deviceName ?? '').trim() || null
  const source = ensureSource(body.source)
  const inaGain = ensureNumber(body.inaGain, 'inaGain')
  const inaOffset = ensureNumber(body.inaOffset, 'inaOffset')
  const pointOneMeter = ensureNumber(body.pointOneMeter, 'pointOneMeter')
  const pointOneEsp = ensureNumber(body.pointOneEsp, 'pointOneEsp')
  const pointTwoMeter = ensureNumber(body.pointTwoMeter, 'pointTwoMeter')
  const pointTwoEsp = ensureNumber(body.pointTwoEsp, 'pointTwoEsp')
  const syncedAt = body.syncedAt ? new Date(body.syncedAt) : null

  const calibration = await prisma.voltMeterCalibration.upsert({
    where: {
      deviceId_source: {
        deviceId,
        source,
      },
    },
    create: {
      deviceId,
      deviceName,
      source,
      inaGain,
      inaOffset,
      pointOneMeter,
      pointOneEsp,
      pointTwoMeter,
      pointTwoEsp,
      syncedAt,
    },
    update: {
      deviceName,
      inaGain,
      inaOffset,
      pointOneMeter,
      pointOneEsp,
      pointTwoMeter,
      pointTwoEsp,
      syncedAt,
    },
  })

  return {
    ok: true,
    calibration,
  }
})
