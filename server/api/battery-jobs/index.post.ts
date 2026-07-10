import { prisma } from '../../utils/prisma'
import { createInitialSlots, ensureBatteryJobPhase, ensureOptionalText, ensureRequiredText, formatBatteryJob } from '../../utils/battery-jobs'

interface BatteryJobPayload {
  phase?: string
  rackId?: string
  palletId?: string
  operatorId?: string | null
  salesOrderId?: string | null
  invoiceId?: string | null
  chargeChannelId?: string | null
  chargeProgramId?: string | null
  openedAt?: string
  workStartedAt?: string
  notes?: string | null
}

export default defineEventHandler(async (event) => {
  const body = await readBody<BatteryJobPayload>(event)
  const phase = ensureBatteryJobPhase(body.phase)
  const rackId = ensureRequiredText(body.rackId ?? body.palletId, 'rackId').toUpperCase()
  const openedAt = body.openedAt ? new Date(body.openedAt) : body.workStartedAt ? new Date(body.workStartedAt) : new Date()
  const operatorId = ensureOptionalText(body.operatorId)
  const salesOrderId = ensureOptionalText(body.salesOrderId)
  const invoiceId = ensureOptionalText(body.invoiceId)
  const chargeChannelId = ensureOptionalText(body.chargeChannelId)
  const chargeProgramId = ensureOptionalText(body.chargeProgramId)
  const notes = ensureOptionalText(body.notes)

  if (phase === 'BEFORE_CHARGE') {
    const job = await prisma.batteryJob.create({
      data: {
        phase,
        status: 'OPEN',
        rackId,
        openedAt,
        operatorId,
        salesOrderId,
        invoiceId,
        chargeChannelId,
        chargeProgramId,
        notes,
        slots: {
          create: createInitialSlots(),
        },
      },
      include: {
        operator: true,
        salesOrder: true,
        invoice: true,
        chargeChannel: true,
        chargeProgram: true,
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

  if (!existingJob) {
    throw createError({
      statusCode: 404,
      statusMessage: 'No rack job found for this rack ID',
    })
  }

  const job = await prisma.batteryJob.update({
    where: {
      id: existingJob.id,
    },
    data: {
      phase,
      operatorId,
      openedAt,
      salesOrderId,
      invoiceId,
      chargeChannelId,
      chargeProgramId,
      notes,
      ...(phase === 'AFTER_CHARGE' ? { status: existingJob.status === 'READY_FOR_DELIVERY' ? existingJob.status : 'OPEN' } : {}),
    },
    include: {
      operator: true,
      salesOrder: true,
      invoice: true,
      chargeChannel: true,
      chargeProgram: true,
      slots: true,
    },
  })

  return {
    ok: true,
    job: formatBatteryJob(job),
  }
})
