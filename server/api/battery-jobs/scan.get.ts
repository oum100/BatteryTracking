import { prisma } from '../../utils/prisma'
import { ensureRequiredText, formatBatteryJob, getScanDecision } from '../../utils/battery-jobs'

export default defineEventHandler(async (event) => {
  const query = getQuery(event)
  const palletId = ensureRequiredText(query.palletId, 'palletId').toUpperCase()

  const job = await prisma.batteryJob.findFirst({
    where: {
      palletId,
    },
    include: {
      operator: true,
      salesOrder: true,
      slots: true,
    },
    orderBy: {
      createdAt: 'desc',
    },
  })

  const decision = getScanDecision(job, new Date())

  return {
    ok: true,
    palletId,
    ...decision,
    job: job ? formatBatteryJob(job) : null,
  }
})
