import { prisma } from '../../utils/prisma'
import { requireUserRole } from '../../utils/user-auth'

export default defineEventHandler(async (event) => {
  await requireUserRole(event, ['ADMIN', 'CALIBRATOR'])
  const query = getQuery(event)
  const deviceId = String(query.deviceId ?? '').trim()
  const requestedSource = String(query.source ?? 'INA226').trim().toUpperCase()

  if (!deviceId) {
    throw createError({
      statusCode: 400,
      statusMessage: 'deviceId is required',
    })
  }

  if (requestedSource !== 'INA226' && requestedSource !== 'INA228') {
    throw createError({
      statusCode: 400,
      statusMessage: 'source must be INA226 or INA228',
    })
  }

  const calibration = await prisma.voltMeterCalibration.findUnique({
    where: {
      deviceId_source: {
        deviceId,
        source: requestedSource,
      },
    },
  })

  return {
    ok: true,
    calibration,
  }
})
