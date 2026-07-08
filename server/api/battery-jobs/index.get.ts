import { prisma } from '../../utils/prisma'
import { formatBatteryJob } from '../../utils/battery-jobs'

export default defineEventHandler(async () => {
  const jobs = await prisma.batteryJob.findMany({
    include: {
      operator: true,
      salesOrder: true,
      slots: true,
    },
    orderBy: {
      createdAt: 'desc',
    },
    take: 100,
  })

  return {
    ok: true,
    jobs: jobs.map(formatBatteryJob),
  }
})
