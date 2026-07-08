import { prisma } from '../../../utils/prisma'
import {
  allSlotsMeasured,
  ensureBatteryJobPhase,
  formatBatteryJob,
  getConfirmedStatus,
  getPhaseCompletionField,
} from '../../../utils/battery-jobs'

interface ConfirmPayload {
  phase?: string
}

export default defineEventHandler(async (event) => {
  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const body = await readBody<ConfirmPayload>(event)
  const phase = ensureBatteryJobPhase(body.phase)

  const job = await prisma.batteryJob.findUnique({
    where: { id },
    include: {
      operator: true,
      salesOrder: true,
      slots: true,
    },
  })

  if (!job) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Job not found',
    })
  }

  if (!allSlotsMeasured(job.slots, phase)) {
    throw createError({
      statusCode: 409,
      statusMessage: 'All 21 slots must be measured before confirmation',
    })
  }

  const updatedJob = await prisma.batteryJob.update({
    where: { id },
    data: {
      phase,
      status: getConfirmedStatus(phase),
      [getPhaseCompletionField(phase)]: new Date(),
      ...(phase === 'DELIVERY' ? { lockedAt: new Date() } : {}),
    },
    include: {
      operator: true,
      salesOrder: true,
      slots: true,
    },
  })

  return {
    ok: true,
    job: formatBatteryJob(updatedJob),
  }
})
