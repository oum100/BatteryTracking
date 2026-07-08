import { prisma } from '../../utils/prisma'
import { ensureBatteryJobPhase, ensureRequiredText, formatBatteryJob } from '../../utils/battery-jobs'

export default defineEventHandler(async (event) => {
  const query = getQuery(event)
  const phase = query.phase ? ensureBatteryJobPhase(query.phase) : null
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

  if (!job) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Pallet job not found',
    })
  }

  const normalizedJob = formatBatteryJob(job)

  return {
    ok: true,
    phase,
    job: normalizedJob,
  }
})
