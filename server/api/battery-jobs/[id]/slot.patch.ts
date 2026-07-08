import { prisma } from '../../../utils/prisma'
import {
  ensureBatteryJobPhase,
  ensureOptionalText,
  ensureSlotNumber,
  ensureVoltageUnit,
  findFirstIncompleteSlot,
  formatBatteryJob,
  getPhaseMeasuredAtField,
  getPhaseVoltageField,
  getPhaseVoltageMvField,
  normalizeVoltageInput,
  slotHasPhaseMeasurement,
} from '../../../utils/battery-jobs'

interface SlotPayload {
  phase?: string
  slotNumber?: number
  batteryId?: string | null
  voltage?: number | string
  voltageUnit?: string
  measuredAt?: string
}

export default defineEventHandler(async (event) => {
  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const body = await readBody<SlotPayload>(event)
  const phase = ensureBatteryJobPhase(body.phase)
  const slotNumber = ensureSlotNumber(body.slotNumber)
  const batteryId = ensureOptionalText(body.batteryId)?.toUpperCase() ?? null
  const voltageUnit = ensureVoltageUnit(body.voltageUnit)
  const { voltage, millivolts } = normalizeVoltageInput(body.voltage, voltageUnit)
  const measuredAt = body.measuredAt ? new Date(body.measuredAt) : new Date()

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

  const firstIncompleteSlot = findFirstIncompleteSlot(job.slots, phase)
  const isFirstPassMeasurement = !slotHasPhaseMeasurement(targetSlot, phase)

  if (isFirstPassMeasurement && firstIncompleteSlot && firstIncompleteSlot.slotNumber !== slotNumber) {
    throw createError({
      statusCode: 409,
      statusMessage: `Please measure slot ${firstIncompleteSlot.slotNumber} before skipping ahead`,
    })
  }

  if (phase === 'BEFORE_CHARGE' && !batteryId && !targetSlot.batteryId) {
    throw createError({
      statusCode: 400,
      statusMessage: 'batteryId is required for before-charge measurement',
    })
  }

  const updatedJob = await prisma.batteryJob.update({
    where: { id },
    data: {
      phase,
      slots: {
        update: {
          where: {
            id: targetSlot.id,
          },
          data: {
            ...(batteryId ? { batteryId } : {}),
            [getPhaseVoltageField(phase)]: voltage,
            [getPhaseVoltageMvField(phase)]: millivolts,
            [getPhaseMeasuredAtField(phase)]: measuredAt,
          },
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
