import { prisma } from '../../utils/prisma'

export default defineEventHandler(async (event) => {
  const query = getQuery(event)
  const deviceId = String(query.deviceId ?? '').trim()

  if (!deviceId) {
    throw createError({
      statusCode: 400,
      statusMessage: 'deviceId is required',
    })
  }

  const calibration = await prisma.voltMeterCalibration.findUnique({
    where: {
      deviceId,
    },
  })

  return {
    ok: true,
    calibration,
  }
})
