import { prisma } from '../../../utils/prisma'
import { ensureOptionalText, ensureSlotNumber, formatBatteryJob } from '../../../utils/battery-jobs'

interface SlotBatteryPayload {
  slotNumber?: number
  batteryId?: string | null
}

export default defineEventHandler(async (event) => {
  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const body = await readBody<SlotBatteryPayload>(event)
  const slotNumber = ensureSlotNumber(body.slotNumber)
  const batteryId = ensureOptionalText(body.batteryId)?.toUpperCase()

  if (!batteryId) {
    throw createError({
      statusCode: 400,
      statusMessage: 'batteryId is required',
    })
  }

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

  if (job.status === 'READY_FOR_DELIVERY') {
    throw createError({
      statusCode: 409,
      statusMessage: 'Delivery job is locked and can no longer be edited',
    })
  }

  const targetSlot = job.slots.find((slot: { slotNumber: number }) => slot.slotNumber === slotNumber)

  if (!targetSlot) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Slot not found',
    })
  }

  const updatedJob = await prisma.batteryJob.update({
    where: { id },
    data: {
      slots: {
        update: {
          where: { id: targetSlot.id },
          data: { batteryId },
        },
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
    job: formatBatteryJob(updatedJob),
  }
})
