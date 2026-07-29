import { prisma } from '../../utils/prisma'
import { allSlotsMeasured, batteryJobInclude, ensureOptionalDate, ensureOptionalShipTo, ensureOptionalText, isBatteryJobLocked, jobHasRecordedSlotData } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

interface BulkPayload {
  action?: 'UPDATE' | 'DELETE' | 'SET_SHIPPED' | string
  ids?: string[]
  applyToGroups?: boolean
  salesOrderId?: string | null
  invoiceId?: string | null
  plannedDeliveryDate?: string | null
  shipTo?: string | null
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const body = await readBody<BulkPayload>(event)
  const action = String(body.action ?? '').trim().toUpperCase()
  const ids = Array.isArray(body.ids) ? body.ids.map(id => String(id).trim()).filter(Boolean) : []
  const applyToGroups = Boolean(body.applyToGroups)

  if (!ids.length) {
    throw createError({
      statusCode: 400,
      statusMessage: 'ids are required',
    })
  }

  if (!['UPDATE', 'DELETE', 'SET_SHIPPED'].includes(action)) {
    throw createError({
      statusCode: 400,
      statusMessage: 'action is invalid',
    })
  }

  const sourceJobs = await prisma.batteryJob.findMany({
    where: {
      id: {
        in: ids,
      },
    },
    include: batteryJobInclude,
  })

  if (!sourceJobs.length) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Jobs not found',
    })
  }

  const batchIds = applyToGroups
    ? sourceJobs.map(job => job.batchId).filter((value): value is string => Boolean(value))
    : []

  const targetJobs = await prisma.batteryJob.findMany({
    where: applyToGroups && batchIds.length
      ? {
          OR: [
            { id: { in: ids } },
            { batchId: { in: batchIds } },
          ],
        }
      : {
          id: { in: ids },
        },
    include: batteryJobInclude,
  })

  if (action === 'UPDATE') {
    const hasStartedJob = targetJobs.some(job => isBatteryJobLocked(job) || jobHasRecordedSlotData(job))
    if (hasStartedJob) {
      throw createError({
        statusCode: 409,
        statusMessage: 'One or more selected jobs already have QC progress and cannot be updated',
      })
    }

    const salesOrderId = ensureOptionalText(body.salesOrderId)
    const invoiceId = ensureOptionalText(body.invoiceId)
    const plannedDeliveryDate = ensureOptionalDate(body.plannedDeliveryDate)
    const shipTo = ensureOptionalShipTo(body.shipTo)

    const data = {
      ...(salesOrderId !== null ? { salesOrderId } : {}),
      ...(invoiceId !== null ? { invoiceId } : {}),
      ...(plannedDeliveryDate !== null ? { plannedDeliveryDate } : {}),
      ...(shipTo !== null ? { shipTo } : {}),
    }

    await prisma.batteryJob.updateMany({
      where: {
        id: {
          in: targetJobs.map(job => job.id),
        },
      },
      data,
    })

    return {
      ok: true,
      action: 'UPDATE',
      affectedCount: targetJobs.length,
    }
  }

  if (action === 'DELETE') {
    const hasLockedOrStarted = targetJobs.some(job => isBatteryJobLocked(job) || jobHasRecordedSlotData(job))
    if (hasLockedOrStarted) {
      throw createError({
        statusCode: 409,
        statusMessage: 'One or more selected jobs already have QC progress and cannot be deleted',
      })
    }

    const deleted = await prisma.batteryJob.deleteMany({
      where: {
        id: {
          in: targetJobs.map(job => job.id),
        },
      },
    })

    return {
      ok: true,
      action: 'DELETE',
      affectedCount: deleted.count,
    }
  }

  const notReadyJob = targetJobs.find(job =>
    !allSlotsMeasured(job.slots, 'BEFORE_CHARGE')
    || !allSlotsMeasured(job.slots, 'AFTER_CHARGE')
    || !allSlotsMeasured(job.slots, 'DELIVERY'),
  )

  if (notReadyJob) {
    throw createError({
      statusCode: 409,
      statusMessage: `Job ${String(notReadyJob.id).slice(-8).toUpperCase()} is not completed for all 3 phases`,
    })
  }

  await prisma.batteryJob.updateMany({
    where: {
      id: {
        in: targetJobs.map(job => job.id),
      },
    },
    data: {
      status: 'SHIPPED',
      lockedAt: new Date(),
    },
  })

  return {
    ok: true,
    action: 'SET_SHIPPED',
    affectedCount: targetJobs.length,
  }
})
