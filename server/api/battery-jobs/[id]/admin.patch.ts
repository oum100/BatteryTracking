import { prisma } from '../../../utils/prisma'
import {
  batteryJobInclude,
  ensureOptionalDate,
  ensureOptionalShipTo,
  ensureOptionalText,
  formatBatteryJob,
  isBatteryJobLocked,
  jobHasRecordedSlotData,
} from '../../../utils/battery-jobs'
import { requireAdminSession } from '../../../utils/admin-auth'

interface AdminBatteryJobPayload {
  scope?: 'JOB' | 'GROUP' | string | null
  salesOrderId?: string | null
  invoiceId?: string | null
  plannedDeliveryDate?: string | null
  shipTo?: string | null
  chargeChannelId?: string | null
  chargeProgramId?: string | null
  notes?: string | null
}

export default defineEventHandler(async (event) => {
  requireAdminSession(event)

  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const body = await readBody<AdminBatteryJobPayload>(event)
  const scope = String(body.scope ?? 'GROUP').trim().toUpperCase()
  const salesOrderId = ensureOptionalText(body.salesOrderId)
  const invoiceId = ensureOptionalText(body.invoiceId)
  const plannedDeliveryDate = ensureOptionalDate(body.plannedDeliveryDate)
  const shipTo = ensureOptionalShipTo(body.shipTo)
  const chargeChannelId = ensureOptionalText(body.chargeChannelId)
  const chargeProgramId = ensureOptionalText(body.chargeProgramId)
  const notes = ensureOptionalText(body.notes)

  const targetJob = await prisma.batteryJob.findUnique({
    where: { id },
    include: batteryJobInclude,
  })

  if (!targetJob) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Job not found',
    })
  }

  if (isBatteryJobLocked(targetJob) || jobHasRecordedSlotData(targetJob)) {
    throw createError({
      statusCode: 409,
      statusMessage: 'This job already has QC progress and cannot be edited from admin',
    })
  }

  const data = {
    ...(salesOrderId !== null ? { salesOrderId } : {}),
    ...(invoiceId !== null ? { invoiceId } : {}),
    ...(plannedDeliveryDate !== null ? { plannedDeliveryDate } : {}),
    ...(shipTo !== null ? { shipTo } : {}),
    ...(chargeChannelId !== null ? { chargeChannelId } : {}),
    ...(chargeProgramId !== null ? { chargeProgramId } : {}),
    ...(notes !== null ? { notes } : {}),
  }

  const groupWhere = targetJob.batchId
    ? { batchId: targetJob.batchId }
    : { id: targetJob.id }

  if (scope === 'GROUP') {
    await prisma.batteryJob.updateMany({
      where: groupWhere,
      data,
    })
  }
  else {
    await prisma.batteryJob.update({
      where: { id },
      data,
    })
  }

  const updatedJob = await prisma.batteryJob.findUnique({
    where: { id: targetJob.id },
    include: batteryJobInclude,
  })

  if (!updatedJob) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Updated job not found',
    })
  }

  return {
    ok: true,
    scope: scope === 'GROUP' ? 'GROUP' : 'JOB',
    job: formatBatteryJob(updatedJob),
  }
})
