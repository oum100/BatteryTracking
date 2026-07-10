import { prisma } from '../../utils/prisma'
import { ensureBatteryJobPhase, ensureRequiredText, formatBatteryJob } from '../../utils/battery-jobs'

export default defineEventHandler(async (event) => {
  const query = getQuery(event)
  const phase = query.phase ? ensureBatteryJobPhase(query.phase) : null
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

  if (!job) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Rack job not found',
    })
  }

  const normalizedJob = formatBatteryJob(job)

  return {
    ok: true,
    phase,
    rackId,
    palletId: rackId,
    job: normalizedJob,
  }
})
