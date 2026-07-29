import { prisma } from '../../utils/prisma'
import {
  batteryJobInclude,
  createBatteryJobBatchId,
  createInitialSlots,
  createPendingRackId,
  ensureOptionalDate,
  ensureOptionalShipTo,
  ensureOptionalText,
  formatBatteryJob,
} from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

interface AdminCreateBatteryJobPayload {
  rackCount?: number | string | null
  salesOrderId?: string | null
  invoiceId?: string | null
  plannedDeliveryDate?: string | null
  shipTo?: string | null
  notes?: string | null
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const body = await readBody<AdminCreateBatteryJobPayload>(event)
  const requestedRackCount = typeof body.rackCount === 'number'
    ? body.rackCount
    : Number.parseInt(String(body.rackCount ?? ''), 10)
  const rackCount = Number.isInteger(requestedRackCount) && requestedRackCount > 0
    ? Math.min(requestedRackCount, 200)
    : 1
  const salesOrderId = ensureOptionalText(body.salesOrderId)
  const invoiceId = ensureOptionalText(body.invoiceId)
  const plannedDeliveryDate = ensureOptionalDate(body.plannedDeliveryDate)
  const shipTo = ensureOptionalShipTo(body.shipTo)
  const notes = ensureOptionalText(body.notes)
  const openedAt = new Date()
  const batchId = createBatteryJobBatchId()

  const createdJobs = await prisma.$transaction(
    Array.from({ length: rackCount }, (_, index) => prisma.batteryJob.create({
      data: {
        batchId,
        phase: 'BEFORE_CHARGE',
        status: 'NEW_JOB',
        rackId: index === 0 && rackCount === 1 ? createPendingRackId() : createPendingRackId(),
        openedAt,
        salesOrderId,
        invoiceId,
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
})
