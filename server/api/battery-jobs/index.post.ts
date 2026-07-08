import { prisma } from '../../utils/prisma'
import { createInitialSlots, ensureBatteryJobPhase, ensureOptionalText, ensureRequiredText, formatBatteryJob } from '../../utils/battery-jobs'

interface BatteryJobPayload {
  phase?: string
  palletId?: string
  operatorId?: string | null
  salesOrderId?: string | null
  workStartedAt?: string
  notes?: string | null
}

export default defineEventHandler(async (event) => {
  const body = await readBody<BatteryJobPayload>(event)
  const phase = ensureBatteryJobPhase(body.phase)
  const palletId = ensureRequiredText(body.palletId, 'palletId').toUpperCase()
  const workStartedAt = body.workStartedAt ? new Date(body.workStartedAt) : new Date()
  const operatorId = ensureOptionalText(body.operatorId)
  const salesOrderId = ensureOptionalText(body.salesOrderId)
  const notes = ensureOptionalText(body.notes)

  if (phase === 'BEFORE_CHARGE') {
    const job = await prisma.batteryJob.create({
      data: {
        phase,
        status: 'OPEN',
        palletId,
        workStartedAt,
        operatorId,
        salesOrderId,
        notes,
        slots: {
          create: createInitialSlots(),
        },
      },
      include: {
        operator: true,
        salesOrder: true,
        slots: true,
      },
    })

    return {
      ok: true,
      job: formatBatteryJob(job),
    }
  }

  const existingJob = await prisma.batteryJob.findFirst({
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

  if (!existingJob) {
    throw createError({
      statusCode: 404,
      statusMessage: 'No pallet job found for this pallet ID',
    })
  }

  const job = await prisma.batteryJob.update({
    where: {
      id: existingJob.id,
    },
    data: {
      phase,
      operatorId,
      workStartedAt,
      notes,
      ...(phase === 'AFTER_CHARGE' ? { status: existingJob.status === 'READY_FOR_DELIVERY' ? existingJob.status : 'OPEN' } : {}),
    },
    include: {
      operator: true,
      salesOrder: true,
      slots: true,
    },
  })

  return {
    ok: true,
    job: formatBatteryJob(job),
  }
})
