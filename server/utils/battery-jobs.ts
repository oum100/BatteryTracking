export const BATTERY_JOB_SLOT_COUNT = 21
export const PENDING_RACK_PREFIX = 'PENDING-'
export const batteryJobInclude = {
  operator: true,
  beforeChargeOperator: true,
  afterChargeOperator: true,
  beforeDeliveryOperator: true,
  salesOrder: {
    include: {
      invoice: true,
    },
  },
  chargeChannel: true,
  chargeProgram: true,
  slots: true,
} as const

export type BatteryJobPhaseValue = 'BEFORE_CHARGE' | 'AFTER_CHARGE' | 'DELIVERY'
export type BatteryJobStatusValue = 'NEW_JOB' | 'BEFORE_CHARGING' | 'AFTER_CHARGING' | 'QC_FOR_DELIVERY' | 'SHIPPED'
export type BatteryJobWorkflowStageValue = BatteryJobStatusValue
export type VoltageUnit = 'V' | 'MV'
export type BatteryJobScanAction = 'LOAD_EXISTING' | 'OPEN_NEW_BEFORE_CHARGE'
export type ShipToFactoryValue = 'AAT' | 'FTM'

export function createPendingRackId() {
  return `${PENDING_RACK_PREFIX}${Date.now().toString(36).toUpperCase()}`
}

export function createBatteryJobBatchId() {
  return `BATCH-${Date.now().toString(36).toUpperCase()}-${Math.random().toString(36).slice(2, 8).toUpperCase()}`
}

export function isPendingRackId(value: unknown) {
  const rackId = String(value ?? '').trim().toUpperCase()
  return rackId.startsWith(PENDING_RACK_PREFIX)
}

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

export function ensureOptionalDate(value: unknown) {
  if (value === null || value === undefined || String(value).trim() === '') {
    return null
  }

  const date = new Date(String(value))

  if (Number.isNaN(date.getTime())) {
    throw createError({
      statusCode: 400,
      statusMessage: 'date is invalid',
    })
  }

  return date
}

export function ensureOptionalShipTo(value: unknown): ShipToFactoryValue | null {
  const normalized = String(value ?? '').trim().toUpperCase()

  if (!normalized) {
    return null
  }

  if (normalized !== 'AAT' && normalized !== 'FTM') {
    throw createError({
      statusCode: 400,
      statusMessage: 'shipTo is invalid',
    })
  }

  return normalized as ShipToFactoryValue
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

export function getNextWorkflowPhase(phase: BatteryJobPhaseValue): BatteryJobPhaseValue {
  if (phase === 'BEFORE_CHARGE') {
    return 'AFTER_CHARGE'
  }

  if (phase === 'AFTER_CHARGE') {
    return 'DELIVERY'
  }

  return 'DELIVERY'
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

export function slotHasRecordedData(slot: Record<string, any>) {
  return Boolean(
    String(slot.batteryId ?? '').trim()
    || slot.beforeVoltage !== null && slot.beforeVoltage !== undefined
    || slot.afterVoltage !== null && slot.afterVoltage !== undefined
    || slot.deliveryVoltage !== null && slot.deliveryVoltage !== undefined,
  )
}

export function jobHasRecordedSlotData(job: Record<string, any>) {
  return Array.isArray(job.slots) && job.slots.some(slotHasRecordedData)
}

export function isBatteryJobLocked(job: Record<string, any>) {
  return Boolean(job.lockedAt || job.status === 'SHIPPED')
}

export function hasPhaseActivity(job: Record<string, any>, phase: BatteryJobPhaseValue) {
  if (!Array.isArray(job.slots)) {
    return false
  }

  const voltageField = getPhaseVoltageField(phase)
  const measuredAtField = getPhaseMeasuredAtField(phase)

  return job.slots.some((slot: Record<string, any>) => {
    if (phase === 'BEFORE_CHARGE' && String(slot.batteryId ?? '').trim()) {
      return true
    }

    return slot[voltageField] !== null && slot[voltageField] !== undefined
      || slot[measuredAtField] !== null && slot[measuredAtField] !== undefined
  })
}

export function getBatteryJobWorkflowStage(job: Record<string, any>): BatteryJobWorkflowStageValue {
  if (isBatteryJobLocked(job)) {
    return 'SHIPPED'
  }

  if (!job.beforeChargeCompletedAt && hasPhaseActivity(job, 'BEFORE_CHARGE')) {
    return 'BEFORE_CHARGING'
  }

  if (!job.beforeChargeCompletedAt) {
    return 'NEW_JOB'
  }

  if (!job.afterChargeCompletedAt) {
    return 'AFTER_CHARGING'
  }

  return 'QC_FOR_DELIVERY'
}

export function getBatteryJobWorkflowLabel(job: Record<string, any>) {
  const labels: Record<BatteryJobWorkflowStageValue, string> = {
    NEW_JOB: 'New Job',
    BEFORE_CHARGING: 'Before Charge',
    AFTER_CHARGING: 'After Charge',
    QC_FOR_DELIVERY: 'QC for Delivery',
    SHIPPED: 'Shipped',
  }

  return labels[getBatteryJobWorkflowStage(job)]
}

export function formatBatteryJob(job: Record<string, any>) {
  const rawRackId = String(job.rackId ?? job.palletId ?? '')
  const hasAssignedRack = Boolean(rawRackId) && !isPendingRackId(rawRackId)
  const rackId = hasAssignedRack ? rawRackId : ''
  const openedAt = job.openedAt ?? job.workStartedAt ?? null
  const workflowStage = getBatteryJobWorkflowStage(job)

  return {
    id: String(job.id),
    batchId: String(job.batchId ?? job.id),
    batchRef: String(job.batchId ?? job.id).replace(/^BATCH-/, '').slice(-10).toUpperCase(),
    jobRef: String(job.id).slice(-8).toUpperCase(),
    phase: job.phase,
    status: getBatteryJobWorkflowStage(job),
    workflowStage,
    workflowLabel: getBatteryJobWorkflowLabel(job),
    recommendedPhase: getRecommendedPhase(job),
    isLocked: isBatteryJobLocked(job),
    canDelete: !isBatteryJobLocked(job) && !jobHasRecordedSlotData(job),
    internalRackId: rawRackId,
    hasAssignedRack,
    rackLabel: hasAssignedRack ? rackId : 'รอเจ้าหน้าที่กำหนด Rack',
    rackId,
    palletId: rackId,
    openedAt,
    workStartedAt: openedAt,
    operatorId: job.operatorId ?? null,
    operatorName: job.operator?.name ?? null,
    operatorCode: job.operator?.code ?? null,
    beforeChargeOperatorId: job.beforeChargeOperatorId ?? null,
    beforeChargeOperatorName: job.beforeChargeOperator?.name ?? null,
    beforeChargeOperatorCode: job.beforeChargeOperator?.code ?? null,
    afterChargeOperatorId: job.afterChargeOperatorId ?? null,
    afterChargeOperatorName: job.afterChargeOperator?.name ?? null,
    afterChargeOperatorCode: job.afterChargeOperator?.code ?? null,
    beforeDeliveryOperatorId: job.beforeDeliveryOperatorId ?? null,
    beforeDeliveryOperatorName: job.beforeDeliveryOperator?.name ?? null,
    beforeDeliveryOperatorCode: job.beforeDeliveryOperator?.code ?? null,
    salesOrderId: job.salesOrderId ?? null,
    salesOrderNumber: job.salesOrder?.soNumber ?? null,
    salesOrderDescription: job.salesOrder?.description ?? null,
    invoiceId: job.salesOrder?.invoice?.id ?? null,
    invoiceNumber: job.salesOrder?.invoice?.invoiceNo ?? null,
    chargeChannelId: job.chargeChannelId ?? null,
    chargeChannelCode: job.chargeChannel?.code ?? null,
    chargeChannelName: job.chargeChannel?.name ?? null,
    chargeProgramId: job.chargeProgramId ?? null,
    chargeProgramCode: job.chargeProgram?.code ?? null,
    chargeProgramName: job.chargeProgram?.name ?? null,
    plannedDeliveryDate: job.plannedDeliveryDate,
    shipTo: job.shipTo ?? null,
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

export function getDerivedBatteryJobStatus(job: Record<string, any>): BatteryJobStatusValue {
  return getBatteryJobWorkflowStage(job)
}

export function isPhaseEditable(job: Record<string, any>, phase: BatteryJobPhaseValue) {
  if (isBatteryJobLocked(job) || isBatteryJobFullyCompleted(job)) {
    return false
  }

  if (phase === 'BEFORE_CHARGE') {
    return !job.beforeChargeCompletedAt
  }

  if (phase === 'AFTER_CHARGE') {
    return Boolean(job.beforeChargeCompletedAt) && !job.afterChargeCompletedAt
  }

  return Boolean(job.afterChargeCompletedAt) && !job.deliveryCompletedAt
}

export function getPhaseOperatorField(phase: BatteryJobPhaseValue) {
  const map = {
    BEFORE_CHARGE: 'beforeChargeOperatorId',
    AFTER_CHARGE: 'afterChargeOperatorId',
    DELIVERY: 'beforeDeliveryOperatorId',
  } as const

  return map[phase]
}

export function isSameCalendarDay(left: Date, right: Date) {
  return left.getFullYear() === right.getFullYear()
    && left.getMonth() === right.getMonth()
    && left.getDate() === right.getDate()
}

export function getScanDecision(job: Record<string, any> | null) {
  if (!job) {
    return {
      found: false,
      action: 'OPEN_NEW_BEFORE_CHARGE' as BatteryJobScanAction,
      recommendedPhase: 'BEFORE_CHARGE' as BatteryJobPhaseValue,
      reason: 'NO_JOB_FOUND',
    }
  }

  const recommendedPhase = getRecommendedPhase(job)
  const completed = isBatteryJobFullyCompleted(job)
  const locked = isBatteryJobLocked(job)

  if (completed || locked) {
    return {
      found: true,
      action: 'OPEN_NEW_BEFORE_CHARGE' as BatteryJobScanAction,
      recommendedPhase: 'BEFORE_CHARGE' as BatteryJobPhaseValue,
      reason: 'ALL_PHASES_COMPLETED',
    }
  }

  return {
    found: true,
    action: 'LOAD_EXISTING' as BatteryJobScanAction,
    recommendedPhase,
    reason: 'CONTINUE_EXISTING',
  }
}
