export const BATTERY_JOB_SLOT_COUNT = 21

export type BatteryJobPhaseValue = 'BEFORE_CHARGE' | 'AFTER_CHARGE' | 'DELIVERY'
export type BatteryJobStatusValue = 'OPEN' | 'BEFORE_CHARGE_COMPLETED' | 'AFTER_CHARGE_COMPLETED' | 'READY_FOR_DELIVERY'
export type VoltageUnit = 'V' | 'MV'
export type BatteryJobScanAction = 'LOAD_EXISTING' | 'OPEN_NEW_BEFORE_CHARGE'

export function ensureBatteryJobPhase(value: unknown): BatteryJobPhaseValue {
  const normalized = String(value ?? '').trim().toUpperCase()

  if (!['BEFORE_CHARGE', 'AFTER_CHARGE', 'DELIVERY'].includes(normalized)) {
    throw createError({
      statusCode: 400,
      statusMessage: 'phase is invalid',
    })
  }

  return normalized as BatteryJobPhaseValue
}

export function ensureRequiredText(value: unknown, field: string) {
  const text = String(value ?? '').trim()

  if (!text) {
    throw createError({
      statusCode: 400,
      statusMessage: `${field} is required`,
    })
  }

  return text
}

export function ensureOptionalText(value: unknown) {
  const text = String(value ?? '').trim()
  return text || null
}

export function ensureSlotNumber(value: unknown) {
  const slotNumber = typeof value === 'number' ? value : Number.parseInt(String(value), 10)

  if (!Number.isInteger(slotNumber) || slotNumber < 1 || slotNumber > BATTERY_JOB_SLOT_COUNT) {
    throw createError({
      statusCode: 400,
      statusMessage: `slotNumber must be between 1 and ${BATTERY_JOB_SLOT_COUNT}`,
    })
  }

  return slotNumber
}

export function ensureVoltageUnit(value: unknown): VoltageUnit {
  const normalized = String(value ?? 'V').trim().toUpperCase()
  return normalized === 'MV' ? 'MV' : 'V'
}

export function normalizeVoltageInput(value: unknown, unit: VoltageUnit) {
  const parsed = typeof value === 'number' ? value : Number.parseFloat(String(value))

  if (Number.isNaN(parsed)) {
    throw createError({
      statusCode: 400,
      statusMessage: 'voltage is invalid',
    })
  }

  const voltage = unit === 'MV' ? parsed / 1000 : parsed
  const roundedVoltage = Number(voltage.toFixed(3))
  const millivolts = Math.round(roundedVoltage * 1000)

  return {
    voltage: roundedVoltage,
    millivolts,
  }
}

export function getPhaseVoltageField(phase: BatteryJobPhaseValue) {
  const map = {
    BEFORE_CHARGE: 'beforeVoltage',
    AFTER_CHARGE: 'afterVoltage',
    DELIVERY: 'deliveryVoltage',
  } as const

  return map[phase]
}

export function getPhaseVoltageMvField(phase: BatteryJobPhaseValue) {
  const map = {
    BEFORE_CHARGE: 'beforeVoltageMv',
    AFTER_CHARGE: 'afterVoltageMv',
    DELIVERY: 'deliveryVoltageMv',
  } as const

  return map[phase]
}

export function getPhaseMeasuredAtField(phase: BatteryJobPhaseValue) {
  const map = {
    BEFORE_CHARGE: 'beforeMeasuredAt',
    AFTER_CHARGE: 'afterMeasuredAt',
    DELIVERY: 'deliveryMeasuredAt',
  } as const

  return map[phase]
}

export function getPhaseCompletionField(phase: BatteryJobPhaseValue) {
  const map = {
    BEFORE_CHARGE: 'beforeChargeCompletedAt',
    AFTER_CHARGE: 'afterChargeCompletedAt',
    DELIVERY: 'deliveryCompletedAt',
  } as const

  return map[phase]
}

export function getConfirmedStatus(phase: BatteryJobPhaseValue): BatteryJobStatusValue {
  const map = {
    BEFORE_CHARGE: 'BEFORE_CHARGE_COMPLETED',
    AFTER_CHARGE: 'AFTER_CHARGE_COMPLETED',
    DELIVERY: 'READY_FOR_DELIVERY',
  } as const

  return map[phase]
}

export function createInitialSlots() {
  return Array.from({ length: BATTERY_JOB_SLOT_COUNT }, (_, index) => ({
    slotNumber: index + 1,
  }))
}

export function slotHasPhaseMeasurement(slot: Record<string, any>, phase: BatteryJobPhaseValue) {
  return slot[getPhaseVoltageField(phase)] !== null && slot[getPhaseVoltageField(phase)] !== undefined
}

export function findFirstIncompleteSlot(slots: Array<Record<string, any>>, phase: BatteryJobPhaseValue) {
  return slots
    .slice()
    .sort((left, right) => left.slotNumber - right.slotNumber)
    .find(slot => !slotHasPhaseMeasurement(slot, phase))
}

export function allSlotsMeasured(slots: Array<Record<string, any>>, phase: BatteryJobPhaseValue) {
  return slots.length === BATTERY_JOB_SLOT_COUNT && slots.every(slot => slotHasPhaseMeasurement(slot, phase))
}

export function formatBatteryJob(job: Record<string, any>) {
  return {
    id: String(job.id),
    phase: job.phase,
    status: job.status,
    palletId: String(job.palletId),
    workStartedAt: job.workStartedAt,
    operatorId: job.operatorId ?? null,
    operatorName: job.operator?.name ?? null,
    operatorCode: job.operator?.code ?? null,
    salesOrderId: job.salesOrderId ?? null,
    salesOrderNumber: job.salesOrder?.soNumber ?? null,
    salesOrderDescription: job.salesOrder?.description ?? null,
    beforeChargeCompletedAt: job.beforeChargeCompletedAt,
    afterChargeCompletedAt: job.afterChargeCompletedAt,
    deliveryCompletedAt: job.deliveryCompletedAt,
    lockedAt: job.lockedAt,
    notes: job.notes ?? null,
    createdAt: job.createdAt,
    updatedAt: job.updatedAt,
    slots: Array.isArray(job.slots)
      ? [...job.slots]
          .sort((left, right) => left.slotNumber - right.slotNumber)
          .map(slot => ({
            id: String(slot.id),
            slotNumber: Number(slot.slotNumber),
            batteryId: slot.batteryId ?? '',
            beforeVoltage: slot.beforeVoltage,
            beforeVoltageMv: slot.beforeVoltageMv,
            beforeMeasuredAt: slot.beforeMeasuredAt,
            afterVoltage: slot.afterVoltage,
            afterVoltageMv: slot.afterVoltageMv,
            afterMeasuredAt: slot.afterMeasuredAt,
            deliveryVoltage: slot.deliveryVoltage,
            deliveryVoltageMv: slot.deliveryVoltageMv,
            deliveryMeasuredAt: slot.deliveryMeasuredAt,
          }))
      : [],
  }
}

export function getRecommendedPhase(job: Record<string, any>): BatteryJobPhaseValue {
  if (!job.beforeChargeCompletedAt) {
    return 'BEFORE_CHARGE'
  }

  if (!job.afterChargeCompletedAt) {
    return 'AFTER_CHARGE'
  }

  if (!job.deliveryCompletedAt) {
    return 'DELIVERY'
  }

  return 'BEFORE_CHARGE'
}

export function isBatteryJobFullyCompleted(job: Record<string, any>) {
  return Boolean(job.beforeChargeCompletedAt && job.afterChargeCompletedAt && job.deliveryCompletedAt)
}

export function isSameCalendarDay(left: Date, right: Date) {
  return left.getFullYear() === right.getFullYear()
    && left.getMonth() === right.getMonth()
    && left.getDate() === right.getDate()
}

export function getScanDecision(job: Record<string, any> | null, now = new Date()) {
  if (!job) {
    return {
      found: false,
      action: 'OPEN_NEW_BEFORE_CHARGE' as BatteryJobScanAction,
      recommendedPhase: 'BEFORE_CHARGE' as BatteryJobPhaseValue,
      reason: 'NO_JOB_FOUND',
    }
  }

  const recommendedPhase = getRecommendedPhase(job)
  const workStartedAt = job.workStartedAt ? new Date(job.workStartedAt) : null
  const completed = isBatteryJobFullyCompleted(job)
  const expired = workStartedAt ? !isSameCalendarDay(workStartedAt, now) : false

  if (completed || expired) {
    return {
      found: true,
      action: 'OPEN_NEW_BEFORE_CHARGE' as BatteryJobScanAction,
      recommendedPhase: 'BEFORE_CHARGE' as BatteryJobPhaseValue,
      reason: completed ? 'ALL_PHASES_COMPLETED' : 'JOB_EXPIRED',
    }
  }

  return {
    found: true,
    action: 'LOAD_EXISTING' as BatteryJobScanAction,
    recommendedPhase,
    reason: 'CONTINUE_EXISTING',
  }
}
