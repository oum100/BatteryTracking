import { prisma } from '../../utils/prisma'
import {
  batteryJobInclude,
  createBatteryJobBatchId,
  createPendingRackId,
  createInitialSlots,
  getDerivedBatteryJobStatus,
  ensureBatteryJobPhase,
  ensureOptionalDate,
  ensureOptionalShipTo,
  ensureOptionalText,
  formatBatteryJob,
  getPhaseOperatorField,
  isPendingRackId,
  isBatteryJobLocked,
  isPhaseEditable,
  jobHasRecordedSlotData,
} from '../../utils/battery-jobs'

interface BatteryJobPayload {
  jobId?: string | null
  phase?: string
  rackCount?: number | string | null
  rackId?: string
  palletId?: string
  operatorId?: string | null
  salesOrderId?: string | null
  invoiceId?: string | null
  chargeChannelId?: string | null
  chargeProgramId?: string | null
  openedAt?: string
  workStartedAt?: string
  plannedDeliveryDate?: string | null
  shipTo?: string | null
  notes?: string | null
}

export default defineEventHandler(async (event) => {
  const body = await readBody<BatteryJobPayload>(event)
  const phase = ensureBatteryJobPhase(body.phase)
  const jobId = ensureOptionalText(body.jobId)
  const requestedRackCount = typeof body.rackCount === 'number'
    ? body.rackCount
    : Number.parseInt(String(body.rackCount ?? ''), 10)
  const rackCount = Number.isInteger(requestedRackCount) && requestedRackCount > 0
    ? Math.min(requestedRackCount, 200)
    : 1
  const requestedRackId = ensureOptionalText(body.rackId ?? body.palletId)?.toUpperCase() ?? null
  const rackId = requestedRackId ?? createPendingRackId()
  const openedAt = body.openedAt ? new Date(body.openedAt) : body.workStartedAt ? new Date(body.workStartedAt) : new Date()
  const operatorId = ensureOptionalText(body.operatorId)
  const salesOrderId = ensureOptionalText(body.salesOrderId)
  const invoiceId = ensureOptionalText(body.invoiceId)
  const chargeChannelId = ensureOptionalText(body.chargeChannelId)
  const chargeProgramId = ensureOptionalText(body.chargeProgramId)
  const plannedDeliveryDate = ensureOptionalDate(body.plannedDeliveryDate)
  const shipTo = ensureOptionalShipTo(body.shipTo)
  const notes = ensureOptionalText(body.notes)

  const targetJob = jobId
    ? await prisma.batteryJob.findUnique({
        where: { id: jobId },
        include: batteryJobInclude,
      })
    : null

  if (targetJob && isPendingRackId(targetJob.rackId)) {
    if (isBatteryJobLocked(targetJob) || jobHasRecordedSlotData(targetJob)) {
      throw createError({
        statusCode: 409,
        statusMessage: 'This job already has QC progress and job details can no longer be changed',
      })
    }

    if (!requestedRackId) {
      throw createError({
        statusCode: 400,
        statusMessage: 'rackId is required when assigning a pending job',
      })
    }

    const rackConflict = await prisma.batteryJob.findFirst({
      where: {
        rackId: requestedRackId,
        id: {
          not: targetJob.id,
        },
      },
      include: batteryJobInclude,
      orderBy: {
        createdAt: 'desc',
      },
    })

    if (rackConflict) {
      throw createError({
        statusCode: 409,
        statusMessage: `rack ${requestedRackId} is already assigned to another job`,
      })
    }

    const assignedJob = await prisma.batteryJob.update({
      where: {
        id: targetJob.id,
      },
      data: {
        rackId: requestedRackId,
        phase,
        openedAt,
        ...(operatorId ? { [getPhaseOperatorField(phase)]: operatorId } : {}),
        ...(salesOrderId !== null ? { salesOrderId } : {}),
        ...(invoiceId !== null ? { invoiceId } : {}),
        ...(phase === 'BEFORE_CHARGE' && chargeChannelId !== null ? { chargeChannelId } : {}),
        ...(phase === 'BEFORE_CHARGE' && chargeProgramId !== null ? { chargeProgramId } : {}),
        ...(plannedDeliveryDate !== null ? { plannedDeliveryDate } : {}),
        ...(shipTo !== null ? { shipTo } : {}),
        notes,
        status: getDerivedBatteryJobStatus({
          ...targetJob,
          phase,
          rackId: requestedRackId,
        }),
      },
      include: batteryJobInclude,
    })

    return {
      ok: true,
      job: formatBatteryJob(assignedJob),
    }
  }

  const existingJob = targetJob ?? (requestedRackId
    ? await prisma.batteryJob.findFirst({
        where: {
          rackId: requestedRackId,
        },
        include: batteryJobInclude,
        orderBy: {
          createdAt: 'desc',
        },
      })
    : null)

  if (!existingJob || (phase === 'BEFORE_CHARGE' && existingJob.lockedAt)) {
    const batchId = createBatteryJobBatchId()
    const createdJobs = await prisma.$transaction(
      Array.from({ length: rackCount }, (_, index) => prisma.batteryJob.create({
        data: {
          batchId,
          phase: 'BEFORE_CHARGE',
          status: 'NEW_JOB',
          rackId: rackCount === 1 && index === 0 ? rackId : createPendingRackId(),
          openedAt,
          salesOrderId,
          invoiceId,
          chargeChannelId,
          chargeProgramId,
          plannedDeliveryDate,
          shipTo,
          notes,
          slots: {
            create: createInitialSlots(),
          },
        },
        include: batteryJobInclude,
      })),
    )

    const jobs = createdJobs.map(formatBatteryJob)

    return {
      ok: true,
      job: jobs[0],
      jobs,
    }
  }

  if (jobHasRecordedSlotData(existingJob)) {
    if (!isPhaseEditable(existingJob, phase)) {
      throw createError({
        statusCode: 409,
        statusMessage: 'This phase is already completed or not available yet for this job',
      })
    }

    if (requestedRackId !== existingJob.rackId) {
      throw createError({
        statusCode: 409,
        statusMessage: 'rackId cannot be changed after slot data has been recorded',
      })
    }

    const job = await prisma.batteryJob.update({
      where: {
        id: existingJob.id,
      },
      data: {
        phase,
        status: getDerivedBatteryJobStatus({
          ...existingJob,
          phase,
        }),
        ...(operatorId ? { [getPhaseOperatorField(phase)]: operatorId } : {}),
        ...(chargeChannelId !== null ? { chargeChannelId } : {}),
        ...(chargeProgramId !== null ? { chargeProgramId } : {}),
      },
      include: batteryJobInclude,
    })

    return {
      ok: true,
      job: formatBatteryJob(job),
    }
  }

  if (isBatteryJobLocked(existingJob)) {
    throw createError({
      statusCode: 409,
      statusMessage: 'This job is locked and job details can no longer be changed',
    })
  }

  const job = await prisma.batteryJob.update({
    where: {
      id: existingJob.id,
    },
    data: {
      phase,
      openedAt,
      ...(operatorId ? { [getPhaseOperatorField(phase)]: operatorId } : {}),
      ...(salesOrderId !== null ? { salesOrderId } : {}),
      ...(invoiceId !== null ? { invoiceId } : {}),
      ...(phase === 'BEFORE_CHARGE' && chargeChannelId !== null ? { chargeChannelId } : {}),
      ...(phase === 'BEFORE_CHARGE' && chargeProgramId !== null ? { chargeProgramId } : {}),
      ...(plannedDeliveryDate !== null ? { plannedDeliveryDate } : {}),
      ...(shipTo !== null ? { shipTo } : {}),
      notes,
      status: getDerivedBatteryJobStatus({
        ...existingJob,
        phase,
      }),
    },
    include: batteryJobInclude,
  })

  return {
    ok: true,
    job: formatBatteryJob(job),
  }
})
