import { prisma } from '../../utils/prisma'
import { batteryJobInclude, formatBatteryJob } from '../../utils/battery-jobs'

export default defineEventHandler(async () => {
  const jobs = await prisma.batteryJob.findMany({
    include: batteryJobInclude,
    orderBy: {
      createdAt: 'desc',
    },
    take: 500,
  })

  return {
    ok: true,
    jobs: jobs.map(formatBatteryJob),
  }
})
