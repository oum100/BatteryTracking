import { prisma } from '../../../utils/prisma'
import {
  allSlotsMeasured,
  batteryJobInclude,
  ensureOptionalDate,
  ensureOptionalShipTo,
  formatBatteryJob,
  isBatteryJobLocked,
} from '../../../utils/battery-jobs'
import { requireAdminSession } from '../../../utils/admin-auth'

interface ReadyPayload {
  plannedDeliveryDate?: string | null
  shipTo?: string | null
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const body = await readBody<ReadyPayload>(event)
  const plannedDeliveryDate = ensureOptionalDate(body.plannedDeliveryDate)
  const shipTo = ensureOptionalShipTo(body.shipTo)

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

  if (isBatteryJobLocked(job)) {
    return {
      ok: true,
      job: formatBatteryJob(job),
    }
  }

  if (!allSlotsMeasured(job.slots, 'BEFORE_CHARGE') || !allSlotsMeasured(job.slots, 'AFTER_CHARGE') || !allSlotsMeasured(job.slots, 'DELIVERY')) {
    throw createError({
      statusCode: 409,
      statusMessage: 'All three QC phases must be completed before setting Ready for Delivery',
    })
  }

  const updatedJob = await prisma.batteryJob.update({
    where: { id },
    data: {
      status: 'SHIPPED',
      lockedAt: new Date(),
      plannedDeliveryDate: plannedDeliveryDate ?? job.plannedDeliveryDate,
      shipTo: shipTo ?? job.shipTo,
    },
    include: batteryJobInclude,
  })

  return {
    ok: true,
    job: formatBatteryJob(updatedJob),
  }
})
