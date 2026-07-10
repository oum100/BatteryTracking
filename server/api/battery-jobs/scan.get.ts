import { prisma } from '../../utils/prisma'
import { ensureRequiredText, formatBatteryJob, getScanDecision } from '../../utils/battery-jobs'

export default defineEventHandler(async (event) => {
  const query = getQuery(event)
  const rackId = ensureRequiredText(query.rackId ?? query.palletId, 'rackId').toUpperCase()

  const job = await prisma.batteryJob.findFirst({
    where: {
      rackId,
    },
    include: {
      operator: true,
      salesOrder: true,
      invoice: true,
      chargeChannel: true,
      chargeProgram: true,
      slots: true,
    },
    orderBy: {
      createdAt: 'desc',
    },
  })

  const decision = getScanDecision(job, new Date())

  return {
    ok: true,
    rackId,
    palletId: rackId,
    ...decision,
    job: job ? formatBatteryJob(job) : null,
  }
})
