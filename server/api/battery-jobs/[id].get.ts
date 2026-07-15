import { prisma } from '../../utils/prisma'
import { batteryJobInclude, formatBatteryJob } from '../../utils/battery-jobs'

export default defineEventHandler(async (event) => {
  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const job = await prisma.batteryJob.findUnique({
    where: { id },
    include: batteryJobInclude,
  })

  if (!job) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Job not found',
    })
  }

  return {
    ok: true,
    job: formatBatteryJob(job),
  }
})
