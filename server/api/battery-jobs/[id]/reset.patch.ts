import { prisma } from '../../../utils/prisma'
import { batteryJobInclude, formatBatteryJob } from '../../../utils/battery-jobs'
import { requireAdminSession } from '../../../utils/admin-auth'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = getRouterParam(event, 'id')
  if (!id) {
    throw createError({ statusCode: 400, statusMessage: 'id is required' })
  }

  const job = await prisma.batteryJob.findUnique({ where: { id }, select: { id: true } })
  if (!job) {
    throw createError({ statusCode: 404, statusMessage: 'Job not found' })
  }

  // Keep the order and delivery setup, but clear every value produced by QC work.
  await prisma.$transaction([
    prisma.batteryJobSlot.updateMany({
      where: { jobId: id },
      data: {
        batteryId: null,
        beforeVoltage: null,
        beforeVoltageMv: null,
        beforeMeasuredAt: null,
        afterVoltage: null,
        afterVoltageMv: null,
        afterMeasuredAt: null,
        deliveryVoltage: null,
        deliveryVoltageMv: null,
        deliveryMeasuredAt: null,
      },
    }),
    prisma.batteryJob.update({
      where: { id },
      data: {
        phase: 'BEFORE_CHARGE',
        status: 'NEW_JOB',
        operatorId: null,
        beforeChargeOperatorId: null,
        afterChargeOperatorId: null,
        beforeDeliveryOperatorId: null,
        beforeChargeCompletedAt: null,
        afterChargeCompletedAt: null,
        deliveryCompletedAt: null,
        lockedAt: null,
      },
    }),
  ])

  const resetJob = await prisma.batteryJob.findUnique({
    where: { id },
    include: batteryJobInclude,
  })

  if (!resetJob) {
    throw createError({ statusCode: 404, statusMessage: 'Reset job not found' })
  }

  return { ok: true, job: formatBatteryJob(resetJob) }
})
