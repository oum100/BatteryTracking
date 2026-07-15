import { prisma } from '../../../utils/prisma'
import { batteryJobInclude, ensureOptionalText, ensureSlotNumber, formatBatteryJob, getDerivedBatteryJobStatus, isBatteryJobLocked, isPhaseEditable } from '../../../utils/battery-jobs'

interface SlotBatteryPayload {
  slotNumber?: number
  batteryId?: string | null
  checkDuplicateId?: boolean
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
    include: batteryJobInclude,
  })

  if (!job) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Job not found',
    })
  }

  if (isBatteryJobLocked(job)) {
    throw createError({
      statusCode: 409,
      statusMessage: 'Delivery job is locked and can no longer be edited',
    })
  }

  if (!isPhaseEditable(job, 'BEFORE_CHARGE')) {
    throw createError({
      statusCode: 409,
      statusMessage: 'Before-charge phase is already completed or not available for this job',
    })
  }

  const targetSlot = job.slots.find((slot: { slotNumber: number }) => slot.slotNumber === slotNumber)

  if (!targetSlot) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Slot not found',
    })
  }

  if (body.checkDuplicateId) {
    const duplicateSlot = job.slots.find(slot => (
      slot.slotNumber !== slotNumber
      && String(slot.batteryId ?? '').trim().toUpperCase() === batteryId
    ))

    if (duplicateSlot) {
      throw createError({
        statusCode: 409,
        statusMessage: `Battery ID already exists in slot ${duplicateSlot.slotNumber}`,
      })
    }
  }

  const updatedJob = await prisma.batteryJob.update({
    where: { id },
    data: {
      status: getDerivedBatteryJobStatus({
        ...job,
        slots: job.slots.map(slot => slot.id === targetSlot.id ? { ...slot, batteryId } : slot),
      }),
      slots: {
        update: {
          where: { id: targetSlot.id },
          data: { batteryId },
        },
      },
    },
    include: batteryJobInclude,
  })

  return {
    ok: true,
    job: formatBatteryJob(updatedJob),
  }
})
