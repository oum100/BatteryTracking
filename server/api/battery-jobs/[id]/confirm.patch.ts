import { prisma } from '../../../utils/prisma'
import {
  allSlotsMeasured,
  batteryJobInclude,
  ensureBatteryJobPhase,
  ensureOptionalText,
  formatBatteryJob,
  getDerivedBatteryJobStatus,
  getNextWorkflowPhase,
  getPhaseCompletionField,
  getPhaseOperatorField,
  isPhaseEditable,
} from '../../../utils/battery-jobs'

interface ConfirmPayload {
  phase?: string
  operatorId?: string | null
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
  const operatorId = ensureOptionalText(body.operatorId)

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

  if (!isPhaseEditable(job, phase)) {
    throw createError({
      statusCode: 409,
      statusMessage: 'This phase is already completed or not available yet for this job',
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
      phase: getNextWorkflowPhase(phase),
      status: getDerivedBatteryJobStatus({
        ...job,
        phase: getNextWorkflowPhase(phase),
        [getPhaseCompletionField(phase)]: new Date(),
      }),
      ...(operatorId ? { [getPhaseOperatorField(phase)]: operatorId } : {}),
      [getPhaseCompletionField(phase)]: new Date(),
    },
    include: batteryJobInclude,
  })

  return {
    ok: true,
    job: formatBatteryJob(updatedJob),
  }
})
