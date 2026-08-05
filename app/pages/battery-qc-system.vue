<script setup lang="ts">
type JobPhase = "BEFORE_CHARGE" | "AFTER_CHARGE" | "DELIVERY";
type ScanTarget = "rack" | "battery";

interface EmployeeItem {
  id: string;
  code: string;
  name: string;
}

interface SalesOrderItem {
  id: string;
  soNumber: string;
  description: string | null;
}

interface InvoiceItem {
  id: string;
  invoiceNo: string;
  description: string | null;
}

interface ChargeChannelItem {
  id: string;
  code: string;
  name: string;
}

interface ChargeProgramItem {
  id: string;
  code: string;
  name: string;
}

interface BatteryJobSlot {
  id: string;
  slotNumber: number;
  batteryId: string;
  beforeVoltage: number | null;
  beforeVoltageMv: number | null;
  beforeMeasuredAt: string | null;
  afterVoltage: number | null;
  afterVoltageMv: number | null;
  afterMeasuredAt: string | null;
  deliveryVoltage: number | null;
  deliveryVoltageMv: number | null;
  deliveryMeasuredAt: string | null;
}

interface BatteryJobRecord {
  id: string;
  jobRef?: string;
  internalRackId?: string;
  hasAssignedRack?: boolean;
  rackLabel?: string;
  phase: JobPhase;
  status: string;
  workflowStage?:
    | "NEW_JOB"
    | "BEFORE_CHARGING"
    | "AFTER_CHARGING"
    | "QC_FOR_DELIVERY"
    | "SHIPPED";
  workflowLabel?: string;
  recommendedPhase?: JobPhase;
  isLocked?: boolean;
  rackId: string;
  palletId: string;
  openedAt: string;
  workStartedAt: string;
  operatorId: string | null;
  operatorName: string | null;
  beforeChargeOperatorId: string | null;
  beforeChargeOperatorName: string | null;
  afterChargeOperatorId: string | null;
  afterChargeOperatorName: string | null;
  beforeDeliveryOperatorId: string | null;
  beforeDeliveryOperatorName: string | null;
  salesOrderId: string | null;
  salesOrderNumber: string | null;
  invoiceId: string | null;
  invoiceNumber: string | null;
  chargeChannelId: string | null;
  chargeChannelCode: string | null;
  chargeChannelName: string | null;
  chargeProgramId: string | null;
  chargeProgramCode: string | null;
  chargeProgramName: string | null;
  plannedDeliveryDate: string | null;
  shipTo: "AAT" | "FTM" | null;
  beforeChargeCompletedAt: string | null;
  afterChargeCompletedAt: string | null;
  deliveryCompletedAt: string | null;
  lockedAt: string | null;
  slots: BatteryJobSlot[];
}

interface ScanDecisionResponse {
  ok: boolean;
  rackId: string;
  palletId: string;
  found: boolean;
  action: "LOAD_EXISTING" | "OPEN_NEW_BEFORE_CHARGE";
  recommendedPhase: JobPhase;
  reason:
    | "NO_JOB_FOUND"
    | "ALL_PHASES_COMPLETED"
    | "JOB_EXPIRED"
    | "CONTINUE_EXISTING";
  job: BatteryJobRecord | null;
}

interface VoltMeterCalibrationRecord {
  id: string;
  deviceId: string;
  deviceName: string | null;
  inaGain: number;
  inaOffset: number;
  pointOneMeter: number;
  pointOneEsp: number;
  pointTwoMeter: number;
  pointTwoEsp: number;
  syncedAt: string | null;
  createdAt: string;
  updatedAt: string;
}

const phase = ref<JobPhase | null>(null);
const currentJob = ref<BatteryJobRecord | null>(null);
const jobOptions = ref<BatteryJobRecord[]>([]);
const adminJobId = ref("");
const employees = ref<EmployeeItem[]>([]);
const salesOrders = ref<SalesOrderItem[]>([]);
const chargeChannels = ref<ChargeChannelItem[]>([]);
const chargePrograms = ref<ChargeProgramItem[]>([]);
const selectedSlotNumber = ref(1);
const editingSlotNumber = ref<number | null>(null);
const openedAt = ref(toDateTimeLocalValue(new Date()));
const operatorId = ref("");
const employeeScanInput = ref("");
const salesOrderId = ref("");
const rackId = ref("");
const chargeChannelId = ref("");
const chargeProgramId = ref("");
const batteryScanInput = ref("");
const voltageScanInput = ref("");
const voltageInput = ref("");
const checkDuplicateBatteryId = ref(false);
const newEmployeeCode = ref("");
const newEmployeeName = ref("");
const newSoNumber = ref("");
const newSoDescription = ref("");
const detailModalOpen = ref(false);
const scanTarget = ref<ScanTarget>("rack");
const rackPickerOpen = ref(false);
const bleConnected = ref(false);
const bleDeviceName = ref("BLE Volt Meter");
const isBusy = ref(false);
const isSavingBattery = ref(false);
const isMeasuring = ref(false);
const isConfirming = ref(false);
const jobDetailsSaved = ref(false);
const loadError = ref("");
const actionMessage = ref(
  "เปิดใบงานหรือเปิด rack เพื่อเริ่มงานวัดแรงดัน battery",
);
const measurementPopupOpen = ref(false);
const measurementPopupValue = ref("");
const measurementPopupUnit = ref("");
const measurementPopupLabel = ref("Voltage Read");
const measurementPopupContext = ref("");
const measurementPopupKey = ref(0);
const workflowActionMode = ref<"battery" | "voltage" | null>(null);
const calibrateModalOpen = ref(false);
const calibrateSubmitting = ref(false);
const calibrateSyncing = ref(false);
const calibrateResyncing = ref(false);
const calibratePointOneMeter = ref("");
const calibratePointOneEsp = ref("");
const calibratePointTwoMeter = ref("");
const calibratePointTwoEsp = ref("");
const calibrateResultGain = ref<number | null>(null);
const calibrateResultOffset = ref<number | null>(null);
const calibratePreviewMessage = ref("");
const currentEspInaGain = ref<number | null>(null);
const currentEspInaOffset = ref<number | null>(null);
const lastEspVoltReading = ref<number | null>(null);
const bleMeterDeviceId = ref("");
const bleMeterFirmwareVersion = ref("");
const savedDbCalibration = ref<VoltMeterCalibrationRecord | null>(null);
const jobDetailsCard = ref<HTMLElement | null>(null);

const BLE_SERVICE_UUID = "7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001";
const BLE_STATUS_CHARACTERISTIC_UUID = "7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001";
const BLE_CALIBRATION_CHARACTERISTIC_UUID =
  "7f9e0003-6a9d-4f7e-8d4d-32e7be6f1001";
const BLE_DEVICE_NAME_PREFIX = "PUMA-Voltmeter-";

const keyboardBatteryScanRef = useTemplateRef<HTMLInputElement>(
  "keyboardBatteryScan",
);
const keyboardVoltageScanRef = useTemplateRef<HTMLInputElement>(
  "keyboardVoltageScan",
);

let bleDevice: any = null;
let bleStatusCharacteristic: BluetoothRemoteGATTCharacteristic | null = null;
let bleCalibrationCharacteristic: BluetoothRemoteGATTCharacteristic | null =
  null;
let measurementPopupTimer: ReturnType<typeof setTimeout> | null = null;
const FEEDBACK_POPUP_DURATION = 2000;
const WORKFLOW_COMPLETE_POPUP_DURATION = 3500;

const phaseOptions = [
  {
    value: "BEFORE_CHARGE" as const,
    label: "QC Before Charge",
    title: "QC Before Charge",
    detail: "บันทึก Battery ID และค่าแรงดันก่อนชาร์จ",
    icon: "i-lucide-battery-low",
    theme: "before" as const,
    tone: "bg-lime-700 text-white hover:bg-lime-800 active:bg-lime-900",
    softTone: "border border-lime-300 bg-lime-100 text-lime-950",
  },
  {
    value: "AFTER_CHARGE" as const,
    label: "QC After Charge",
    title: "QC After Charge",
    detail: "อ่านค่าแรงดันหลังชาร์จตามตำแหน่งเดิม",
    icon: "i-lucide-battery-full",
    theme: "after" as const,
    tone: "bg-sky-700 text-white hover:bg-sky-800 active:bg-sky-900",
    softTone: "border border-sky-300 bg-sky-100 text-sky-950",
  },
  {
    value: "DELIVERY" as const,
    label: "QC Before Delivery",
    title: "QC Before Delivery",
    detail: "ตรวจวัดก่อนส่งมอบและยืนยันค่าก่อนจัดส่ง",
    icon: "i-lucide-truck",
    theme: "delivery" as const,
    tone: "bg-amber-700 text-white hover:bg-amber-800 active:bg-amber-900",
    softTone: "border border-amber-300 bg-amber-100 text-amber-950",
  },
] as const;

type PhaseQueueCounts = {
  beforeCharge: number | null;
  afterCharge: number | null;
  delivery: number | null;
};

const phaseQueueCounts = ref<PhaseQueueCounts>({
  beforeCharge: null,
  afterCharge: null,
  delivery: null,
});
let phaseQueueRefreshTimer: ReturnType<typeof setInterval> | null = null;

function getPhaseQueueCount(targetPhase: JobPhase) {
  if (targetPhase === "BEFORE_CHARGE")
    return phaseQueueCounts.value.beforeCharge;
  if (targetPhase === "AFTER_CHARGE") return phaseQueueCounts.value.afterCharge;
  return phaseQueueCounts.value.delivery;
}

async function loadPhaseQueueCounts() {
  const response = await $fetch<{ queues: Required<PhaseQueueCounts> }>(
    "/api/battery-jobs/queue",
  );
  phaseQueueCounts.value = response.queues;
}

const currentPhaseMeta = computed(
  () => phaseOptions.find((option) => option.value === phase.value) ?? null,
);
const currentPhaseJobDetailsTitle = computed(() => {
  if (phase.value === "BEFORE_CHARGE") {
    return "QC Before Charge Job Details";
  }

  if (phase.value === "AFTER_CHARGE") {
    return "QC After Charge Job Details";
  }

  if (phase.value === "DELIVERY") {
    return "QC Before Delivery Job Details";
  }

  return "QC Job Details";
});
const hasPhaseSelected = computed(() => phase.value !== null);
const selectedEmployee = computed(
  () =>
    employees.value.find((employee) => employee.id === operatorId.value) ??
    null,
);
const employeeOptions = computed(() =>
  employees.value.map((employee) => ({
    label: `${employee.code} - ${employee.name}`,
    value: employee.code,
  })),
);
const employeeSelectOptions = computed(() =>
  employees.value.map((employee) => ({
    label: `${employee.code} - ${employee.name}`,
    value: employee.id,
    code: employee.code,
    name: employee.name,
  })),
);
const selectedEmployeeOption = computed(
  () =>
    employeeSelectOptions.value.find(
      (employee) => employee.value === operatorId.value,
    ) ?? null,
);
const rackOptions = computed(() =>
  jobOptions.value
    .filter((job) => {
      if (job.hasAssignedRack === false || !job.rackId || job.isLocked) {
        return false;
      }

      if (phase.value === "BEFORE_CHARGE") {
        return !job.beforeChargeCompletedAt;
      }

      if (phase.value === "AFTER_CHARGE") {
        return (
          Boolean(job.beforeChargeCompletedAt) && !job.afterChargeCompletedAt
        );
      }

      return Boolean(job.afterChargeCompletedAt) && !job.deliveryCompletedAt;
    })
    .map((job) => ({
      label: `${job.rackId}${job.salesOrderNumber ? ` · ${job.salesOrderNumber}` : ""}`,
      value: job.rackId,
    })),
);
const chargeChannelOptions = computed(() =>
  chargeChannels.value.map((channel) => ({
    label: channel.name,
    value: channel.id,
  })),
);
const chargeProgramOptions = computed(() =>
  chargePrograms.value.map((program) => ({
    label: program.name,
    value: program.id,
  })),
);
const pendingAdminJobOptions = computed(() =>
  jobOptions.value
    .filter((job) => {
      if (job.isLocked) {
        return false;
      }

      if (phase.value === "BEFORE_CHARGE") {
        return !job.beforeChargeCompletedAt;
      }

      if (phase.value === "AFTER_CHARGE") {
        return (
          Boolean(job.beforeChargeCompletedAt) && !job.afterChargeCompletedAt
        );
      }

      return Boolean(job.afterChargeCompletedAt) && !job.deliveryCompletedAt;
    })
    .map((job) => ({
      label: `${job.jobRef ?? job.id.slice(-8).toUpperCase()} · ${job.salesOrderNumber || "-"} · ${job.invoiceNumber || "-"}`,
      value: job.id,
      jobRef: job.jobRef ?? job.id.slice(-8).toUpperCase(),
      salesOrderNumber: job.salesOrderNumber || "-",
      invoiceNumber: job.invoiceNumber || "-",
    })),
);
const selectedPendingAdminJobOption = computed(
  () =>
    pendingAdminJobOptions.value.find(
      (job) => job.value === adminJobId.value,
    ) ?? null,
);
function getDefaultNightChargeProgramId() {
  const nightProgram = chargePrograms.value.find(
    (program) =>
      program.name.trim().toLowerCase() === "night" ||
      program.code.trim().toUpperCase() === "PRG-NIGHT",
  );

  return nightProgram?.id ?? "";
}

function getPhaseOperatorId(
  job: BatteryJobRecord | null,
  targetPhase: JobPhase | null,
) {
  if (!job) {
    return "";
  }

  if (targetPhase === "BEFORE_CHARGE") {
    return job.beforeChargeOperatorId ?? "";
  }

  if (targetPhase === "AFTER_CHARGE") {
    return job.afterChargeOperatorId ?? "";
  }

  if (targetPhase === "DELIVERY") {
    return job.beforeDeliveryOperatorId ?? "";
  }

  return job.operatorId ?? "";
}

function slotHasRecordedData(slot: BatteryJobSlot) {
  return Boolean(
    slot.batteryId.trim() ||
    slot.beforeVoltage !== null ||
    slot.afterVoltage !== null ||
    slot.deliveryVoltage !== null,
  );
}

const qcFieldUi = {
  root: "w-full",
  label: "mb-1 text-sm font-bold text-slate-700",
} as const;
const qcInputUi = {
  base: "rounded-[8px] border border-slate-300 bg-white px-3 text-sm font-semibold text-slate-950 ring-1 ring-inset ring-slate-300 transition hover:border-slate-400 hover:ring-slate-400 focus:border-slate-500 focus:ring-2 focus:ring-inset focus:ring-slate-400",
} as const;
const qcInputUpperUi = {
  base: "rounded-[8px] border border-slate-300 bg-white px-3 text-sm font-semibold uppercase text-slate-950 ring-1 ring-inset ring-slate-300 transition hover:border-slate-400 hover:ring-slate-400 focus:border-slate-500 focus:ring-2 focus:ring-inset focus:ring-slate-400",
} as const;
const qcSelectUi = {
  base: "rounded-[8px] border border-slate-300 bg-white px-3 text-sm font-semibold text-slate-950 ring-1 ring-inset ring-slate-300 transition hover:border-slate-400 hover:ring-slate-400 focus:border-slate-500 focus:ring-2 focus:ring-inset focus:ring-slate-400",
  content: "rounded-[8px]",
  viewport: "max-h-[240px]",
  item: "text-sm font-medium text-slate-900",
} as const;
const qcReadonlyUi = {
  base: "rounded-[8px] border border-slate-200 bg-slate-50 px-3 text-sm font-semibold text-slate-950",
} as const;

const THAI_DIGIT_MAP: Record<string, string> = {
  "๐": "0",
  "๑": "1",
  "๒": "2",
  "๓": "3",
  "๔": "4",
  "๕": "5",
  "๖": "6",
  "๗": "7",
  "๘": "8",
  "๙": "9",
};

const THAI_TO_ASCII_KEY_MAP: Record<string, string> = {
  ๅ: "1",
  "/": "2",
  "-": "3",
  ภ: "4",
  ถ: "5",
  "ุ": "6",
  "ึ": "7",
  ค: "8",
  ต: "9",
  จ: "0",
  ข: "-",
  ช: "=",
  ๆ: "q",
  ไ: "w",
  ำ: "e",
  พ: "r",
  ะ: "t",
  "ั": "y",
  "ี": "u",
  ร: "i",
  น: "o",
  ย: "p",
  บ: "[",
  ล: "]",
  ฃ: "\\",
  ฟ: "a",
  ห: "s",
  ก: "d",
  ด: "f",
  เ: "g",
  "้": "h",
  "่": "j",
  า: "k",
  ส: "l",
  ว: ";",
  ง: "'",
  ผ: "z",
  ป: "x",
  แ: "c",
  อ: "v",
  "ิ": "b",
  "ื": "n",
  ท: "m",
  ม: ",",
  ใ: ".",
  ฝ: "/",
};

function mapThaiKeyboardToAscii(value: string) {
  return Array.from(value)
    .map((char) => {
      if (THAI_DIGIT_MAP[char]) {
        return THAI_DIGIT_MAP[char];
      }

      return THAI_TO_ASCII_KEY_MAP[char] ?? char;
    })
    .join("");
}

function sanitizeBatteryIdInput(value: string) {
  const mapped = mapThaiKeyboardToAscii(
    String(value ?? "")
      .normalize("NFKC")
      .trim(),
  );
  return mapped
    .toUpperCase()
    .replace(/\s+/g, "")
    .replace(/[^A-Z0-9\-_/.:]/g, "");
}

function sanitizeVoltageReaderInput(value: string) {
  const mapped = mapThaiKeyboardToAscii(
    String(value ?? "")
      .normalize("NFKC")
      .trim(),
  ).replace(/,/g, ".");
  const compact = mapped.replace(/\s+/g, "");
  const match = compact.match(/[-+]?\d+(?:\.\d+)?/);
  return match?.[0] ?? "";
}

function isLikelyVoltageToken(value: string) {
  const normalized = sanitizeVoltageReaderInput(value);
  if (!normalized || !/^[-+]?\d+(?:\.\d{1,3})?$/.test(normalized)) {
    return false;
  }

  const parsed = Number.parseFloat(normalized);
  return Number.isFinite(parsed) && parsed >= 0 && parsed <= 30;
}

function isLikelyBatteryIdToken(value: string) {
  const normalized = sanitizeBatteryIdInput(value);
  if (!normalized) {
    return false;
  }

  if (isLikelyVoltageToken(normalized)) {
    return false;
  }

  return normalized.length >= 6;
}

function sanitizeRackInput(value: string) {
  const rawValue = String(value ?? "")
    .normalize("NFKC")
    .trim();
  const mapped = Array.from(rawValue)
    .map((char) => {
      // Rack labels commonly contain hyphens. Keep them intact for list selections.
      if (char === "-") {
        return char;
      }

      if (THAI_DIGIT_MAP[char]) {
        return THAI_DIGIT_MAP[char];
      }

      return THAI_TO_ASCII_KEY_MAP[char] ?? char;
    })
    .join("");

  return mapped
    .toUpperCase()
    .replace(/\s+/g, "")
    .replace(/[^A-Z0-9\-_/.:]/g, "");
}

function sanitizeEmployeeIdInput(value: string) {
  return sanitizeBatteryIdInput(value);
}

function clearWorkflowCaptureInputs() {
  batteryScanInput.value = "";
  voltageScanInput.value = "";

  if (keyboardBatteryScanRef.value) {
    keyboardBatteryScanRef.value.value = "";
  }

  if (keyboardVoltageScanRef.value) {
    keyboardVoltageScanRef.value.value = "";
  }
}

function getFocusableInput(id: string) {
  if (typeof document === "undefined") {
    return null;
  }

  const target = document.getElementById(id);
  if (!target) {
    return null;
  }

  if (
    target instanceof HTMLInputElement ||
    target instanceof HTMLTextAreaElement
  ) {
    return target;
  }

  return target.querySelector('input, textarea, button, [role="combobox"]') as
    HTMLInputElement | HTMLTextAreaElement | HTMLButtonElement | null;
}

function focusSelectableElement(
  element: HTMLInputElement | HTMLTextAreaElement | HTMLButtonElement | null,
) {
  if (!element) {
    return;
  }

  try {
    element.focus({ preventScroll: true });
  } catch {
    element.focus();
  }

  if ("select" in element && typeof element.select === "function") {
    element.select();
  }
}

function blurActiveElement() {
  if (typeof document === "undefined") {
    return;
  }

  const activeElement = document.activeElement;
  if (activeElement instanceof HTMLElement) {
    activeElement.blur();
  }
}

function isElementFocused(id: string) {
  if (typeof document === "undefined") {
    return false;
  }

  const activeElement = document.activeElement;
  const target = document.getElementById(id);
  if (!activeElement || !target) {
    return false;
  }

  return activeElement === target || target.contains(activeElement);
}
const slotRows = computed(() => {
  const slots =
    currentJob.value?.slots ??
    Array.from({ length: 21 }, (_, index) => ({
      id: `preview-${index + 1}`,
      slotNumber: index + 1,
      batteryId: "",
      beforeVoltage: null,
      beforeVoltageMv: null,
      beforeMeasuredAt: null,
      afterVoltage: null,
      afterVoltageMv: null,
      afterMeasuredAt: null,
      deliveryVoltage: null,
      deliveryVoltageMv: null,
      deliveryMeasuredAt: null,
    }));

  return [slots.slice(0, 7), slots.slice(7, 14), slots.slice(14, 21)];
});

const selectedSlot = computed(
  () =>
    currentJob.value?.slots.find(
      (slot) => slot.slotNumber === selectedSlotNumber.value,
    ) ?? null,
);
const modalSelectedSlot = computed(() => {
  if (!editingSlotNumber.value) {
    return null;
  }

  return (
    currentJob.value?.slots.find(
      (slot) => slot.slotNumber === editingSlotNumber.value,
    ) ?? null
  );
});
const hasJob = computed(() => currentJob.value !== null);
const jobHasRecordedSlotData = computed(() =>
  Boolean(currentJob.value?.slots.some(slotHasRecordedData)),
);
const currentPhaseConfirmed = computed(() => {
  if (!currentJob.value || !phase.value) {
    return false;
  }

  if (phase.value === "BEFORE_CHARGE") {
    return Boolean(currentJob.value.beforeChargeCompletedAt);
  }

  if (phase.value === "AFTER_CHARGE") {
    return Boolean(currentJob.value.afterChargeCompletedAt);
  }

  return Boolean(currentJob.value.deliveryCompletedAt);
});
const jobDetailsLocked = computed(
  () =>
    hasJob.value &&
    (Boolean(currentJob.value?.isLocked) || currentPhaseConfirmed.value),
);
const canEditRackDuringBeforeCharge = computed(
  () =>
    currentJob.value?.phase === "BEFORE_CHARGE" &&
    phase.value === "BEFORE_CHARGE" &&
    !jobDetailsLocked.value,
);
const rackDetailsLocked = computed(
  () =>
    jobDetailsLocked.value ||
    (jobHasRecordedSlotData.value && !canEditRackDuringBeforeCharge.value),
);
const jobDetailsMatchCurrentJob = computed(() => {
  if (!currentJob.value) {
    return false;
  }

  if (rackId.value.trim() !== currentJob.value.rackId.trim()) {
    return false;
  }

  if (
    (operatorId.value || "") !==
    getPhaseOperatorId(currentJob.value, phase.value)
  ) {
    return false;
  }

  if (phase.value === "BEFORE_CHARGE") {
    return (
      adminJobId.value === currentJob.value.id &&
      (chargeChannelId.value || "") ===
        (currentJob.value.chargeChannelId || "") &&
      (chargeProgramId.value || "") === (currentJob.value.chargeProgramId || "")
    );
  }

  return true;
});
const hasPersistedRequiredJobDetails = computed(() =>
  Boolean(
    currentJob.value &&
    currentJob.value.hasAssignedRack !== false &&
    currentJob.value.rackId.trim() &&
    getPhaseOperatorId(currentJob.value, phase.value) &&
    (phase.value !== "BEFORE_CHARGE" ||
      (currentJob.value.chargeChannelId && currentJob.value.chargeProgramId)),
  ),
);
const showRackLayoutCard = computed(
  () =>
    hasJob.value &&
    (jobHasRecordedSlotData.value ||
      hasPersistedRequiredJobDetails.value ||
      (jobDetailsSaved.value && jobDetailsMatchCurrentJob.value)),
);
const firstIncompleteSlotNumber = computed(() => {
  if (!currentJob.value) {
    return 1;
  }

  const slots = currentJob.value?.slots ?? [];
  const firstIncomplete = slots.find(
    (slot) => !getPhaseVoltage(slot, phase.value),
  );
  return firstIncomplete?.slotNumber ?? 1;
});
const progressCount = computed(() => {
  const slots = currentJob.value?.slots ?? [];
  return slots.filter((slot) => getPhaseVoltage(slot, phase.value) !== null)
    .length;
});
const canConfirmPhase = computed(
  () => currentJob.value !== null && progressCount.value === 21,
);
const jobStatusLabel = computed(() => {
  if (!currentJob.value) {
    return "No job opened";
  }

  return currentJob.value.workflowLabel ?? "Open";
});
const openButtonLabel = computed(() => "Rack View");
const activeSlotCardClass = computed(() => {
  if (phase.value === "BEFORE_CHARGE") {
    return "border-lime-400 bg-lime-50 text-lime-950";
  }

  if (phase.value === "AFTER_CHARGE") {
    return "border-sky-400 bg-sky-50 text-sky-950";
  }

  return "border-amber-400 bg-amber-50 text-amber-950";
});
const workflowActionActiveClass = computed(() => {
  if (phase.value === "BEFORE_CHARGE") {
    return "bg-emerald-700 text-white hover:bg-emerald-800 active:bg-emerald-950";
  }

  if (phase.value === "AFTER_CHARGE") {
    return "bg-sky-700 text-white hover:bg-sky-800 active:bg-sky-950";
  }

  return "bg-amber-600 text-white hover:bg-amber-700 active:bg-amber-900";
});

const workflowActionIdleClass = computed(() => {
  if (phase.value === "BEFORE_CHARGE") {
    return "border border-emerald-400 bg-emerald-50 text-emerald-950 hover:bg-emerald-200 active:bg-emerald-300";
  }

  if (phase.value === "AFTER_CHARGE") {
    return "border border-sky-400 bg-sky-50 text-sky-950 hover:bg-sky-200 active:bg-sky-300";
  }

  return "border border-amber-400 bg-amber-50 text-amber-950 hover:bg-amber-200 active:bg-amber-300";
});
const isBatteryIdWorkflowEnabled = computed(
  () => phase.value === "BEFORE_CHARGE",
);
const hasRackInputReady = computed(() => Boolean(rackId.value.trim()));
const hasEditableJobDetailsChanges = computed(() => {
  if (!currentJob.value || !phase.value) {
    return false;
  }

  const operatorChanged =
    (operatorId.value || "") !==
    getPhaseOperatorId(currentJob.value, phase.value);
  const rackChanged = rackId.value.trim() !== currentJob.value.rackId.trim();

  if (phase.value === "BEFORE_CHARGE") {
    return (
      operatorChanged ||
      (!rackDetailsLocked.value && rackChanged) ||
      (chargeChannelId.value || "") !==
        (currentJob.value.chargeChannelId || "") ||
      (chargeProgramId.value || "") !== (currentJob.value.chargeProgramId || "")
    );
  }

  return operatorChanged || (!rackDetailsLocked.value && rackChanged);
});
const canSaveJobDetails = computed(() => {
  if (!phase.value || !currentJob.value || jobDetailsLocked.value) {
    return false;
  }

  if (!rackId.value.trim() || !operatorId.value) {
    return false;
  }

  if (phase.value === "BEFORE_CHARGE") {
    return Boolean(
      adminJobId.value &&
      chargeChannelId.value &&
      chargeProgramId.value &&
      hasEditableJobDetailsChanges.value,
    );
  }

  return hasEditableJobDetailsChanges.value;
});
const canStartWorkflow = computed(() =>
  Boolean(currentJob.value && hasRackAssignedForWorkflow.value),
);
const canStartBatteryWorkflow = computed(
  () => canStartWorkflow.value && isBatteryIdWorkflowEnabled.value,
);
const canStartVoltageWorkflow = computed(() => canStartWorkflow.value);
const phasePrimaryButtonClass = computed(() => {
  if (phase.value === "BEFORE_CHARGE") {
    return "bg-lime-50 text-lime-950 border border-lime-400 hover:bg-lime-200 active:bg-lime-300";
  }

  if (phase.value === "AFTER_CHARGE") {
    return "bg-sky-50 text-sky-950 border border-sky-400 hover:bg-sky-200 active:bg-sky-300";
  }

  return "bg-amber-50 text-amber-950 border border-amber-400 hover:bg-amber-200 active:bg-amber-300";
});

const phaseSoftButtonClass = computed(() => {
  if (phase.value === "BEFORE_CHARGE") {
    return "border border-lime-400 bg-lime-50 text-lime-950 hover:bg-lime-200 active:bg-lime-300";
  }

  if (phase.value === "AFTER_CHARGE") {
    return "border border-sky-400 bg-sky-50 text-sky-950 hover:bg-sky-200 active:bg-sky-300";
  }

  return "border border-amber-400 bg-amber-50 text-amber-950 hover:bg-amber-200 active:bg-amber-300";
});

const calibrateResultLabel = computed(() => {
  if (
    calibrateResultGain.value === null ||
    calibrateResultOffset.value === null
  ) {
    return "กรอกค่าเทียบ 2 จุด เพื่อคำนวณ gain และ offset";
  }

  const gain = calibrateResultGain.value.toFixed(5);
  const offset =
    calibrateResultOffset.value >= 0
      ? `+${calibrateResultOffset.value.toFixed(5)}`
      : calibrateResultOffset.value.toFixed(5);

  return `Vcal = (Vesp x ${gain}) ${offset.startsWith("-") ? "-" : "+"} ${Math.abs(calibrateResultOffset.value).toFixed(5)}`;
});

const currentEspCalibrationLabel = computed(() => {
  if (currentEspInaGain.value === null || currentEspInaOffset.value === null) {
    return "ยังไม่ได้อ่านค่าปัจจุบันจาก ESP";
  }

  return `ESP current: gain ${currentEspInaGain.value.toFixed(5)} • offset ${currentEspInaOffset.value >= 0 ? "+" : ""}${currentEspInaOffset.value.toFixed(5)}`;
});

const savedDbCalibrationLabel = computed(() => {
  if (!savedDbCalibration.value) {
    return "ยังไม่มี calibration ล่าสุดในฐานข้อมูลสำหรับเครื่องนี้";
  }

  return `DB latest: gain ${savedDbCalibration.value.inaGain.toFixed(5)} • offset ${savedDbCalibration.value.inaOffset >= 0 ? "+" : ""}${savedDbCalibration.value.inaOffset.toFixed(5)}`;
});

const calibrationBleStatusLabel = computed(() => {
  if (bleConnected.value && bleStatusCharacteristic) {
    return bleDeviceName.value
      ? `Connected: ${bleDeviceName.value}`
      : "Connected to BLE Volt Meter";
  }

  return "ยังไม่ได้เชื่อมต่อ BLE สำหรับ calibration";
});

const calibrationSaveButtonLabel = computed(() =>
  bleConnected.value && bleCalibrationCharacteristic
    ? "Save To DB + Sync BLE"
    : "Save To DB",
);

const canResyncCalibrationToBle = computed(() =>
  Boolean(
    bleConnected.value &&
    bleCalibrationCharacteristic &&
    savedDbCalibration.value,
  ),
);
const hasRackAssignedForWorkflow = computed(() =>
  Boolean(
    (hasPersistedRequiredJobDetails.value ||
      (jobDetailsSaved.value && jobDetailsMatchCurrentJob.value)) &&
    currentJob.value &&
    currentJob.value.hasAssignedRack !== false &&
    currentJob.value.rackId.trim() &&
    rackId.value.trim(),
  ),
);
const jobDetailsCollapsed = computed(() => false);

function toDateTimeLocalValue(date: Date) {
  const year = date.getFullYear();
  const month = `${date.getMonth() + 1}`.padStart(2, "0");
  const day = `${date.getDate()}`.padStart(2, "0");
  const hours = `${date.getHours()}`.padStart(2, "0");
  const minutes = `${date.getMinutes()}`.padStart(2, "0");
  return `${year}-${month}-${day}T${hours}:${minutes}`;
}

function getDefaultWorkflowActionMode(targetPhase: JobPhase) {
  return targetPhase === "BEFORE_CHARGE" ? "battery" : "voltage";
}

function getResumeWorkflowActionMode(targetPhase: JobPhase) {
  if (targetPhase !== "BEFORE_CHARGE" || !currentJob.value) {
    return "voltage";
  }

  // Before charge collects every Battery ID first, then continues with voltage.
  return currentJob.value.slots.some((slot) => !slot.batteryId.trim())
    ? "battery"
    : "voltage";
}

async function armDefaultWorkflow(autoFocus = true) {
  if (!currentJob.value || !phase.value) {
    workflowActionMode.value = null;
    return;
  }

  workflowActionMode.value = getResumeWorkflowActionMode(phase.value);
  activateNextModeSlot();

  if (autoFocus) {
    await focusWorkflowInput();
  }
}

function resetCurrentJobState() {
  currentJob.value = null;
  jobDetailsSaved.value = false;
  selectedSlotNumber.value = 1;
  editingSlotNumber.value = null;
  batteryScanInput.value = "";
  voltageInput.value = "";
  workflowActionMode.value = null;
  detailModalOpen.value = false;
}

function resetPhaseContext() {
  resetCurrentJobState();
  adminJobId.value = "";
  rackId.value = "";
  operatorId.value = "";
  employeeScanInput.value = "";
  salesOrderId.value = "";
  chargeChannelId.value = "";
  chargeProgramId.value = getDefaultNightChargeProgramId();
  openedAt.value = toDateTimeLocalValue(new Date());
}

function syncWorkflowSlotInputs() {
  batteryScanInput.value = selectedSlot.value?.batteryId ?? "";
}

function syncModalSlotInputs() {
  batteryScanInput.value = modalSelectedSlot.value?.batteryId ?? "";
  const phaseVoltage =
    modalSelectedSlot.value && phase.value
      ? getPhaseVoltage(modalSelectedSlot.value, phase.value)
      : null;
  voltageInput.value = phaseVoltage === null ? "" : phaseVoltage.toFixed(3);
}

function clearMeasurementPopupTimer() {
  if (measurementPopupTimer) {
    clearTimeout(measurementPopupTimer);
    measurementPopupTimer = null;
  }
}

function openMeasurementPopup(duration = FEEDBACK_POPUP_DURATION) {
  const scrollLeft = typeof window === "undefined" ? 0 : window.scrollX;
  const scrollTop = typeof window === "undefined" ? 0 : window.scrollY;
  measurementPopupKey.value += 1;
  measurementPopupOpen.value = false;

  const reopen = () => {
    measurementPopupOpen.value = true;
    // Scanner focus and the feedback overlay must never pull the operator away from the active Rack row.
    if (typeof window !== "undefined") {
      window.requestAnimationFrame(() =>
        window.scrollTo({
          left: scrollLeft,
          top: scrollTop,
          behavior: "instant",
        }),
      );
    }
    measurementPopupTimer = setTimeout(() => {
      measurementPopupOpen.value = false;
    }, duration);
  };

  if (
    typeof window !== "undefined" &&
    typeof window.requestAnimationFrame === "function"
  ) {
    window.requestAnimationFrame(reopen);
    return;
  }

  setTimeout(reopen, 0);
}

async function revealRackLayout() {
  await nextTick();

  // This is the only intentional page movement: reveal a newly opened rack once.
  // Scanner reads keep the current viewport and never call this helper.
  jobDetailsCard.value?.scrollIntoView({ behavior: "smooth", block: "start" });
}

function showMeasurementPopup(voltage: number, slotNumber?: number | null) {
  clearMeasurementPopupTimer();
  measurementPopupLabel.value = "Voltage";
  measurementPopupValue.value = voltage.toFixed(3);
  measurementPopupUnit.value = "V";
  measurementPopupContext.value = slotNumber ? `Slot ${slotNumber}` : "";
  openMeasurementPopup();
}

function showBatteryIdPopup(batteryId: string, slotNumber?: number | null) {
  clearMeasurementPopupTimer();
  measurementPopupLabel.value = "Battery ID";
  measurementPopupValue.value = batteryId;
  measurementPopupUnit.value = "";
  measurementPopupContext.value = slotNumber ? `Slot ${slotNumber}` : "";
  openMeasurementPopup();
}

function showBatteryIdCompletePopup() {
  clearMeasurementPopupTimer();
  measurementPopupLabel.value = "Battery ID Complete";
  measurementPopupValue.value = "พร้อมวัด Voltage";
  measurementPopupUnit.value = "";
  measurementPopupContext.value = "บันทึก Battery ID ครบ 21 slots แล้ว";
  openMeasurementPopup(WORKFLOW_COMPLETE_POPUP_DURATION);
}

function showDuplicateBatteryIdPopup(batteryId: string, slotNumber: number) {
  clearMeasurementPopupTimer();
  measurementPopupLabel.value = "Duplicate Battery ID";
  measurementPopupValue.value = batteryId;
  measurementPopupUnit.value = "";
  measurementPopupContext.value = `พบ ID นี้แล้วใน Slot ${slotNumber}`;
  openMeasurementPopup(WORKFLOW_COMPLETE_POPUP_DURATION);
}

function wait(ms: number) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function deriveDeviceIdFromBleName(name: string) {
  const trimmed = name.trim();
  if (!trimmed) {
    return "";
  }

  if (trimmed.startsWith(BLE_DEVICE_NAME_PREFIX)) {
    return `PUMA-VoltMeter-${trimmed.slice(BLE_DEVICE_NAME_PREFIX.length)}`;
  }

  return "";
}

function parseCalibrationNumber(value: string) {
  const parsed = Number.parseFloat(value.trim());
  return Number.isFinite(parsed) ? parsed : null;
}

function updateCalibrationPreview() {
  const meterOne = parseCalibrationNumber(calibratePointOneMeter.value);
  const espOne = parseCalibrationNumber(calibratePointOneEsp.value);
  const meterTwo = parseCalibrationNumber(calibratePointTwoMeter.value);
  const espTwo = parseCalibrationNumber(calibratePointTwoEsp.value);

  calibratePreviewMessage.value = "";
  calibrateResultGain.value = null;
  calibrateResultOffset.value = null;

  if ([meterOne, espOne, meterTwo, espTwo].some((value) => value === null)) {
    return;
  }

  if (espOne === espTwo) {
    calibratePreviewMessage.value =
      "ค่า ESP Volt Meter ทั้งสองจุดต้องไม่เท่ากัน";
    return;
  }

  const gain = (meterTwo! - meterOne!) / (espTwo! - espOne!);
  const offset = meterOne! - espOne! * gain;

  if (!Number.isFinite(gain) || !Number.isFinite(offset)) {
    calibratePreviewMessage.value = "ไม่สามารถคำนวณ gain / offset ได้";
    return;
  }

  calibrateResultGain.value = gain;
  calibrateResultOffset.value = offset;
  calibratePreviewMessage.value = `คำนวณแล้ว gain ${gain.toFixed(5)} และ offset ${offset >= 0 ? "+" : ""}${offset.toFixed(5)}`;
}

function fillCalibrationEspInput(point: 1 | 2) {
  const fallbackVoltage =
    lastEspVoltReading.value ??
    selectedSlot.value?.beforeVoltage ??
    selectedSlot.value?.afterVoltage ??
    selectedSlot.value?.deliveryVoltage ??
    null;
  if (fallbackVoltage === null) {
    actionMessage.value = "ยังไม่มีค่า ESP Volt Meter ล่าสุดให้ดึงมาใช้";
    return;
  }

  if (point === 1) {
    calibratePointOneEsp.value = fallbackVoltage.toFixed(3);
  } else {
    calibratePointTwoEsp.value = fallbackVoltage.toFixed(3);
  }

  updateCalibrationPreview();
}

function openCalibrationModal() {
  calibrateModalOpen.value = true;
  void syncCalibrationFromBle();
  void loadCalibrationFromDb();
}

async function readBleStatusPayload() {
  if (!bleStatusCharacteristic) {
    return null;
  }

  const value = await bleStatusCharacteristic.readValue();
  const text = textDecoder.decode(value);
  return JSON.parse(text) as Record<string, any>;
}

async function syncCalibrationFromBle() {
  if (!bleConnected.value || !bleStatusCharacteristic) {
    actionMessage.value =
      "เชื่อมต่อ BLE Volt Meter ก่อนอ่าน calibration จากอุปกรณ์";
    return;
  }

  calibrateSyncing.value = true;

  try {
    const payload = await readBleStatusPayload();
    const deviceId = String(
      payload?.device_id ?? payload?.deviceId ?? "",
    ).trim();
    const firmwareVersion = String(
      payload?.fw ?? payload?.firmwareVersion ?? "",
    ).trim();
    const inaGain =
      typeof payload?.ina_gain === "number"
        ? payload.ina_gain
        : Number.parseFloat(String(payload?.ina_gain ?? ""));
    const inaOffset =
      typeof payload?.ina_offset === "number"
        ? payload.ina_offset
        : Number.parseFloat(String(payload?.ina_offset ?? ""));
    const voltage =
      typeof payload?.voltage === "number"
        ? payload.voltage
        : Number.parseFloat(String(payload?.voltage ?? ""));

    bleMeterDeviceId.value =
      deviceId || deriveDeviceIdFromBleName(bleDeviceName.value);
    bleMeterFirmwareVersion.value = firmwareVersion;
    currentEspInaGain.value = Number.isFinite(inaGain) ? inaGain : null;
    currentEspInaOffset.value = Number.isFinite(inaOffset) ? inaOffset : null;
    lastEspVoltReading.value = Number.isFinite(voltage)
      ? voltage
      : lastEspVoltReading.value;
    actionMessage.value = bleMeterDeviceId.value
      ? `อ่าน calibration จาก ${bleMeterDeviceId.value} แล้ว`
      : "อ่าน calibration จาก BLE Volt Meter แล้ว";
  } catch (error) {
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "อ่านค่าคาลิเบรตจาก BLE ไม่สำเร็จ";
  } finally {
    calibrateSyncing.value = false;
  }
}

async function loadCalibrationFromDb() {
  const deviceId =
    bleMeterDeviceId.value || deriveDeviceIdFromBleName(bleDeviceName.value);
  if (!deviceId) {
    savedDbCalibration.value = null;
    return;
  }

  try {
    const response = await $fetch<{
      ok: boolean;
      calibration: VoltMeterCalibrationRecord | null;
    }>("/api/voltmeter-calibrations/latest", {
      query: { deviceId },
    });
    savedDbCalibration.value = response.calibration;
  } catch {
    savedDbCalibration.value = null;
  }
}

function applySavedCalibrationToForm() {
  if (!savedDbCalibration.value) {
    actionMessage.value = "ยังไม่มี calibration ใน DB ให้โหลด";
    return;
  }

  calibratePointOneMeter.value =
    savedDbCalibration.value.pointOneMeter.toFixed(3);
  calibratePointOneEsp.value = savedDbCalibration.value.pointOneEsp.toFixed(3);
  calibratePointTwoMeter.value =
    savedDbCalibration.value.pointTwoMeter.toFixed(3);
  calibratePointTwoEsp.value = savedDbCalibration.value.pointTwoEsp.toFixed(3);
  calibrateResultGain.value = savedDbCalibration.value.inaGain;
  calibrateResultOffset.value = savedDbCalibration.value.inaOffset;
  calibratePreviewMessage.value = `โหลด calibration ล่าสุดจาก DB แล้ว (gain ${savedDbCalibration.value.inaGain.toFixed(5)}, offset ${savedDbCalibration.value.inaOffset >= 0 ? "+" : ""}${savedDbCalibration.value.inaOffset.toFixed(5)})`;
}

async function syncSavedCalibrationToBle() {
  if (!savedDbCalibration.value) {
    actionMessage.value = "ยังไม่มี calibration ใน DB สำหรับ sync";
    return;
  }

  if (!bleConnected.value || !bleCalibrationCharacteristic) {
    actionMessage.value =
      "เชื่อมต่อ BLE Volt Meter ก่อน แล้วค่อย sync calibration";
    return;
  }

  calibrateResyncing.value = true;

  try {
    const syncPayload = JSON.stringify({
      cmd: "set_calibration",
      ina_gain: savedDbCalibration.value.inaGain,
      ina_offset: savedDbCalibration.value.inaOffset,
    });

    await bleCalibrationCharacteristic.writeValue(
      new TextEncoder().encode(syncPayload),
    );

    const syncedAt = new Date().toISOString();
    const response = await $fetch<{
      ok: boolean;
      calibration: VoltMeterCalibrationRecord;
    }>("/api/voltmeter-calibrations", {
      method: "POST",
      body: {
        deviceId: savedDbCalibration.value.deviceId,
        deviceName: savedDbCalibration.value.deviceName,
        inaGain: savedDbCalibration.value.inaGain,
        inaOffset: savedDbCalibration.value.inaOffset,
        pointOneMeter: savedDbCalibration.value.pointOneMeter,
        pointOneEsp: savedDbCalibration.value.pointOneEsp,
        pointTwoMeter: savedDbCalibration.value.pointTwoMeter,
        pointTwoEsp: savedDbCalibration.value.pointTwoEsp,
        syncedAt,
      },
    });

    savedDbCalibration.value = response.calibration;
    await syncCalibrationFromBle();
    actionMessage.value = `sync calibration ล่าสุดจาก DB ไปที่ ${savedDbCalibration.value.deviceId} แล้ว`;
  } catch (error) {
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "sync calibration จาก DB ไป BLE ไม่สำเร็จ";
  } finally {
    calibrateResyncing.value = false;
  }
}

async function submitCalibrationToDbAndBle() {
  updateCalibrationPreview();

  if (
    calibrateResultGain.value === null ||
    calibrateResultOffset.value === null
  ) {
    actionMessage.value =
      calibratePreviewMessage.value || "คำนวณ gain / offset ยังไม่สำเร็จ";
    return;
  }

  const deviceId =
    bleMeterDeviceId.value || deriveDeviceIdFromBleName(bleDeviceName.value);
  if (!deviceId) {
    actionMessage.value =
      "เชื่อมต่อ BLE Volt Meter ก่อน เพื่อระบุ device ID สำหรับบันทึก calibration";
    return;
  }

  calibrateSubmitting.value = true;

  try {
    const payload = {
      deviceId,
      deviceName: bleDeviceName.value || null,
      inaGain: Number(calibrateResultGain.value.toFixed(5)),
      inaOffset: Number(calibrateResultOffset.value.toFixed(5)),
      pointOneMeter: Number(
        parseCalibrationNumber(calibratePointOneMeter.value)?.toFixed(3),
      ),
      pointOneEsp: Number(
        parseCalibrationNumber(calibratePointOneEsp.value)?.toFixed(3),
      ),
      pointTwoMeter: Number(
        parseCalibrationNumber(calibratePointTwoMeter.value)?.toFixed(3),
      ),
      pointTwoEsp: Number(
        parseCalibrationNumber(calibratePointTwoEsp.value)?.toFixed(3),
      ),
      syncedAt: null as string | null,
    };

    const savedResponse = await $fetch<{
      ok: boolean;
      calibration: VoltMeterCalibrationRecord;
    }>("/api/voltmeter-calibrations", {
      method: "POST",
      body: payload,
    });

    savedDbCalibration.value = savedResponse.calibration;

    if (bleCalibrationCharacteristic) {
      const syncPayload = JSON.stringify({
        cmd: "set_calibration",
        ina_gain: payload.inaGain,
        ina_offset: payload.inaOffset,
      });

      await bleCalibrationCharacteristic.writeValue(
        new TextEncoder().encode(syncPayload),
      );

      const syncedAt = new Date().toISOString();
      const syncedResponse = await $fetch<{
        ok: boolean;
        calibration: VoltMeterCalibrationRecord;
      }>("/api/voltmeter-calibrations", {
        method: "POST",
        body: {
          ...payload,
          syncedAt,
        },
      });

      savedDbCalibration.value = syncedResponse.calibration;
      await syncCalibrationFromBle();
      actionMessage.value = `บันทึก calibration ลง DB และ sync ไป ${deviceId} แล้ว`;
    } else {
      actionMessage.value = `บันทึก calibration ลง DB สำหรับ ${deviceId} แล้ว ยังไม่ได้ sync BLE`;
    }

    currentEspInaGain.value = calibrateResultGain.value;
    currentEspInaOffset.value = calibrateResultOffset.value;
    calibrateModalOpen.value = false;
  } catch (error) {
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "บันทึกหรือ sync calibration ไม่สำเร็จ";
  } finally {
    calibrateSubmitting.value = false;
  }
}

async function focusEmployeeInput() {
  await nextTick();
  const input = getFocusableInput("qc-employee-input");
  focusSelectableElement(input);
}

async function focusRackInput() {
  await nextTick();
  const input = getFocusableInput("qc-rack-input");
  focusSelectableElement(input);
}

async function focusQcJobInput() {
  await nextTick();
  const input = getFocusableInput("qc-job-input");
  focusSelectableElement(input);
}

async function focusRackInputAfterMenuClose() {
  await nextTick();
  scanTarget.value = "rack";

  for (let attempt = 0; attempt < 6; attempt += 1) {
    blurActiveElement();

    if (
      typeof window !== "undefined" &&
      typeof window.requestAnimationFrame === "function"
    ) {
      await new Promise<void>((resolve) =>
        window.requestAnimationFrame(() => resolve()),
      );
    } else {
      await wait(40);
    }

    const input = getFocusableInput("qc-rack-input");
    focusSelectableElement(input);

    if (isElementFocused("qc-rack-input")) {
      return;
    }

    await wait(60);
  }
}

async function focusQcJobInputAfterPhaseSelect() {
  await nextTick();

  if (
    typeof window !== "undefined" &&
    typeof window.requestAnimationFrame === "function"
  ) {
    await new Promise<void>((resolve) =>
      window.requestAnimationFrame(() => resolve()),
    );
    await new Promise<void>((resolve) =>
      window.requestAnimationFrame(() => resolve()),
    );
  } else {
    await wait(80);
  }

  const input = getFocusableInput("qc-job-input");
  focusSelectableElement(input);
}

function backToPhaseLanding() {
  phase.value = null;
  resetPhaseContext();
  actionMessage.value = "เลือกโหมด QC เพื่อเริ่มงาน";
}

async function focusActiveModalField() {
  await nextTick();

  if (!detailModalOpen.value) {
    return;
  }

  const activeMode =
    workflowActionMode.value ??
    getDefaultWorkflowActionMode(phase.value ?? "BEFORE_CHARGE");

  if (activeMode === "battery") {
    const input = getFocusableInput("qc-battery-input");
    focusSelectableElement(input);
    return;
  }

  const input = getFocusableInput("qc-voltage-input");
  focusSelectableElement(input);
}

async function focusWorkflowInput() {
  await nextTick();

  if (detailModalOpen.value) {
    return;
  }

  if (!workflowActionMode.value) {
    return;
  }

  if (workflowActionMode.value === "battery") {
    keyboardVoltageScanRef.value?.blur();
    batteryScanInput.value = "";
    if (keyboardBatteryScanRef.value) {
      keyboardBatteryScanRef.value.value = "";
    }
    keyboardBatteryScanRef.value?.focus({ preventScroll: true });
    keyboardBatteryScanRef.value?.select();
    return;
  }

  keyboardBatteryScanRef.value?.blur();
  voltageScanInput.value = "";
  if (keyboardVoltageScanRef.value) {
    keyboardVoltageScanRef.value.value = "";
  }
  keyboardVoltageScanRef.value?.focus({ preventScroll: true });
  keyboardVoltageScanRef.value?.select();
}

async function restoreWorkflowFocus() {
  if (typeof document !== "undefined") {
    const activeElement = document.activeElement as HTMLElement | null;
    activeElement?.blur?.();
  }

  if (workflowActionMode.value === "battery") {
    scanTarget.value = "battery";
  }

  await wait(180);
  await focusWorkflowInput();
  await wait(120);
  await focusWorkflowInput();
  await wait(160);
  await focusWorkflowInput();
}

function activateNextModeSlot() {
  if (!currentJob.value) {
    selectedSlotNumber.value = 1;
    editingSlotNumber.value = null;
    return;
  }

  if (workflowActionMode.value === "battery") {
    const nextBatterySlot = currentJob.value.slots.find(
      (slot) => !slot.batteryId.trim(),
    );
    if (nextBatterySlot) {
      selectedSlotNumber.value = nextBatterySlot.slotNumber;
      editingSlotNumber.value = null;
      syncWorkflowSlotInputs();
      return;
    }
  }

  const nextVoltageSlot = currentJob.value.slots.find(
    (slot) => getPhaseVoltage(slot, phase.value) === null,
  );
  if (nextVoltageSlot) {
    selectedSlotNumber.value = nextVoltageSlot.slotNumber;
    editingSlotNumber.value = null;
    syncWorkflowSlotInputs();
  }
}

function applyScannedValue(value: string) {
  const normalized = value.trim().toUpperCase();

  if (!normalized) {
    return;
  }

  if (scanTarget.value === "rack") {
    rackId.value = normalized;
    openedAt.value = toDateTimeLocalValue(new Date());
    actionMessage.value = `สแกน rack ${normalized} แล้ว และตั้งเวลาเปิดใบงานให้อัตโนมัติ`;
    return;
  }

  batteryScanInput.value = normalized;
}

function isAutoScanRackCode(value: string) {
  return value.startsWith("Z");
}

async function handleScannedRackWorkflow(scannedRackId: string) {
  isBusy.value = true;
  loadError.value = "";

  try {
    const response = await $fetch<ScanDecisionResponse>(
      "/api/battery-jobs/scan",
      {
        query: {
          rackId: scannedRackId,
        },
      },
    );

    rackId.value = response.rackId || response.palletId;
    openedAt.value = toDateTimeLocalValue(new Date());

    if (response.action === "LOAD_EXISTING" && response.job) {
      phase.value = response.recommendedPhase;
      applyJob(response.job);
      openedAt.value = toDateTimeLocalValue(new Date());

      if (hasPersistedRequiredJobDetails.value) {
        jobDetailsSaved.value = true;
        await armDefaultWorkflow();
      } else {
        jobDetailsSaved.value = false;
        workflowActionMode.value = null;
      }

      await revealRackLayout();
      actionMessage.value =
        response.recommendedPhase === "AFTER_CHARGE"
          ? hasPersistedRequiredJobDetails.value
            ? `พบ rack ${response.rackId || response.palletId} แล้ว ระบบเปิด phase 2 เพื่อวัด Voltage ต่อ`
            : `พบ rack ${response.rackId || response.palletId} แล้ว กรุณาเลือก Emp ID ของ QC After Charge แล้วกด Save`
          : response.recommendedPhase === "DELIVERY"
            ? hasPersistedRequiredJobDetails.value
              ? `พบ rack ${response.rackId || response.palletId} แล้ว ระบบเปิด phase Delivery เพื่อวัด Voltage ต่อ`
              : `พบ rack ${response.rackId || response.palletId} แล้ว กรุณาเลือก Emp ID ของ QC Before Delivery แล้วกด Save`
            : `พบ rack ${response.rackId || response.palletId} แล้ว ระบบกลับเข้า phase ก่อนชาร์จต่อให้อัตโนมัติ`;

      if (!hasPersistedRequiredJobDetails.value) {
        await focusEmployeeInput();
      }
      return;
    }

    resetCurrentJobState();
    rackId.value = response.rackId || response.palletId;
    openedAt.value = toDateTimeLocalValue(new Date());
    actionMessage.value =
      response.reason === "JOB_EXPIRED"
        ? `rack ${response.rackId || response.palletId} เป็นงานเก่า กรุณาให้ Admin สร้างใบงานใหม่`
        : response.reason === "ALL_PHASES_COMPLETED"
          ? `rack ${response.rackId || response.palletId} ปิดครบทุก phase แล้ว ถ้าจะเริ่มรอบใหม่ให้ Admin สร้างใบงานใหม่`
          : `ไม่พบ rack ${response.rackId || response.palletId} ในระบบ กรุณาให้ Admin สร้างใบงานก่อน`;
  } catch (error) {
    loadError.value =
      error instanceof Error ? error.message : "Unable to scan rack";
  } finally {
    isBusy.value = false;
  }
}

async function handleRackInput(value: string) {
  if (isBusy.value) {
    return;
  }

  rackId.value = sanitizeRackInput(value);

  if (!rackId.value.trim()) {
    actionMessage.value = "Rack # รับเฉพาะตัวอักษรอังกฤษและตัวเลข";
    return;
  }

  scanTarget.value = "rack";
  applyScannedValue(rackId.value);

  const isListedRack = rackOptions.value.some(
    (option) => option.value === rackId.value,
  );

  if (
    phase.value !== "BEFORE_CHARGE" ||
    isListedRack ||
    isAutoScanRackCode(rackId.value)
  ) {
    await handleScannedRackWorkflow(rackId.value);
    return;
  }

  await focusEmployeeInput();
}

async function selectRackFromPicker(selectedRackId: string) {
  rackPickerOpen.value = false;
  await handleRackInput(selectedRackId);
}

async function resolveEmployeeId(scannedValue: string) {
  const normalized = sanitizeEmployeeIdInput(scannedValue);
  employeeScanInput.value = normalized;

  if (!normalized) {
    actionMessage.value = "Employee ID รับเฉพาะตัวอักษรอังกฤษและตัวเลข";
    return;
  }

  const matched = employees.value.find(
    (employee) => employee.code.trim().toUpperCase() === normalized,
  );
  if (!matched) {
    actionMessage.value = `ไม่พบ Employee ID ${normalized} ในฐานข้อมูล`;
    return;
  }

  operatorId.value = matched.id;
  employeeScanInput.value = matched.code;
  actionMessage.value = `เลือกเจ้าหน้าที่ ${matched.code} - ${matched.name} แล้ว`;
  await focusRackInput();
}

function selectEmployeeById(employeeId: string) {
  operatorId.value = employeeId;

  const matched = employees.value.find(
    (employee) => employee.id === employeeId,
  );
  if (!matched) {
    employeeScanInput.value = "";
    return;
  }

  employeeScanInput.value = matched.code;
  actionMessage.value = `เลือกเจ้าหน้าที่ ${matched.code} - ${matched.name} แล้ว`;
}

function handleEmployeeSelection(
  employeeId: string | { value?: string; label?: string } | null,
) {
  const normalized =
    typeof employeeId === "object" && employeeId !== null
      ? String(employeeId.value ?? "").trim()
      : String(employeeId ?? "").trim();

  if (!normalized) {
    operatorId.value = "";
    employeeScanInput.value = "";
    return;
  }

  selectEmployeeById(normalized);
}

async function handleBatteryInput(value: string) {
  if (!hasRackAssignedForWorkflow.value) {
    actionMessage.value = "ยังไม่ได้กำหนด Rack # สำหรับใบงานนี้";
    await focusRackInput();
    return;
  }

  const normalized = sanitizeBatteryIdInput(value);
  batteryScanInput.value = normalized;

  if (!batteryScanInput.value.trim()) {
    actionMessage.value = "ไม่ได้รับ Battery ID ที่เป็นตัวอักษรอังกฤษ/ตัวเลข";
    return;
  }

  if (!isLikelyBatteryIdToken(batteryScanInput.value)) {
    actionMessage.value =
      "ค่าที่อ่านมาเป็นรูปแบบ Voltage หรือ Battery ID สั้นเกินไป";
    return;
  }

  scanTarget.value = "battery";
  applyScannedValue(batteryScanInput.value);

  if (currentJob.value && phase.value === "BEFORE_CHARGE") {
    await saveSelectedBatteryId();
  }
}

async function handleVoltageReaderInput(value: string) {
  if (!hasRackAssignedForWorkflow.value) {
    actionMessage.value = "ยังไม่ได้กำหนด Rack # สำหรับใบงานนี้";
    await focusRackInput();
    return;
  }

  const normalized = sanitizeVoltageReaderInput(value);
  if (!normalized) {
    actionMessage.value = "ไม่ได้รับค่า Voltage ที่เป็นตัวเลขจากเครื่องวัด";
    return;
  }

  if (!isLikelyVoltageToken(normalized)) {
    actionMessage.value = "ค่าที่อ่านมาไม่ใช่รูปแบบ Voltage ที่ระบบยอมรับ";
    return;
  }

  const parsedVoltage = Number.parseFloat(normalized);
  if (Number.isFinite(parsedVoltage)) {
    lastEspVoltReading.value = parsedVoltage;
  }

  voltageScanInput.value = "";
  workflowActionMode.value = "voltage";
  voltageInput.value = normalized;
  await saveActiveSlotVoltage(normalized);
}

async function startBatteryIdScanWorkflow() {
  if (!isBatteryIdWorkflowEnabled.value) {
    actionMessage.value = "phase นี้ไม่อนุญาตให้แก้ Battery ID";
    return;
  }

  if (!currentJob.value) {
    actionMessage.value = "เปิดใบงานก่อน แล้วค่อยเริ่ม scan Battery ID";
    return;
  }

  if (!hasRackInputReady.value) {
    actionMessage.value = "ต้องกำหนด Rack # ก่อน จึงจะเริ่มอ่าน Battery ID ได้";
    await focusRackInput();
    return;
  }

  if (!hasRackAssignedForWorkflow.value) {
    actionMessage.value =
      "บันทึกข้อมูล Job Details ก่อน แล้วค่อยเริ่มอ่าน Battery ID";
    return;
  }

  workflowActionMode.value = "battery";
  activateNextModeSlot();
  scanTarget.value = "battery";
  clearWorkflowCaptureInputs();
  actionMessage.value = `พร้อมรับค่า Battery ID สำหรับ slot ${selectedSlotNumber.value}`;
  await focusWorkflowInput();
}

async function startVoltageWorkflow() {
  if (!currentJob.value) {
    actionMessage.value = "เปิดงานก่อน แล้วค่อยเริ่มอ่านค่า Voltage";
    return;
  }

  if (!hasRackInputReady.value) {
    actionMessage.value = "ต้องกำหนด Rack # ก่อน จึงจะเริ่มอ่าน Voltage ได้";
    await focusRackInput();
    return;
  }

  if (!hasRackAssignedForWorkflow.value) {
    actionMessage.value =
      "บันทึกข้อมูล Job Details ก่อน แล้วค่อยเริ่มอ่าน Voltage";
    return;
  }

  workflowActionMode.value = "voltage";
  activateNextModeSlot();
  clearWorkflowCaptureInputs();
  actionMessage.value = `พร้อมรับค่า Voltage สำหรับ slot ${selectedSlotNumber.value}`;
  await focusWorkflowInput();
}

function handleBleDisconnect() {
  bleConnected.value = false;
  bleDeviceName.value = "BLE Volt Meter";
  bleDevice = null;
  bleStatusCharacteristic = null;
  bleCalibrationCharacteristic = null;
  bleMeterDeviceId.value = "";
  bleMeterFirmwareVersion.value = "";
  actionMessage.value = "BLE Volt Meter disconnected";
}

async function connectToBleVoltMeterDevice(
  device: BluetoothDevice,
  showConnectedMessage = true,
) {
  bleDevice = device;
  bleDeviceName.value = device.name || "BLE Volt Meter";
  bleDevice.addEventListener("gattserverdisconnected", handleBleDisconnect);

  const server = await bleDevice.gatt?.connect();
  if (!server) {
    throw new Error("BLE Volt Meter GATT connection failed");
  }

  const service = await server.getPrimaryService(BLE_SERVICE_UUID);
  bleStatusCharacteristic = await service.getCharacteristic(
    BLE_STATUS_CHARACTERISTIC_UUID,
  );
  bleCalibrationCharacteristic = await service.getCharacteristic(
    BLE_CALIBRATION_CHARACTERISTIC_UUID,
  );
  bleConnected.value = true;
  bleMeterDeviceId.value = deriveDeviceIdFromBleName(bleDeviceName.value);
  await syncCalibrationFromBle();
  await loadCalibrationFromDb();

  if (showConnectedMessage) {
    actionMessage.value = bleDeviceName.value.startsWith(BLE_DEVICE_NAME_PREFIX)
      ? `${bleDeviceName.value} connected`
      : `${bleDeviceName.value} connected (service matched)`;
  }
}

async function tryAutoReconnectBleVoltMeter() {
  if (typeof window === "undefined") {
    return;
  }

  const bluetooth = (navigator as any).bluetooth;
  if (!bluetooth?.getDevices) {
    return;
  }

  try {
    const devices = await bluetooth.getDevices();
    const matchedDevice = devices.find((device: BluetoothDevice) => {
      const name = device.name || "";
      return (
        name.startsWith(BLE_DEVICE_NAME_PREFIX) || name.includes("Voltmeter")
      );
    });

    if (!matchedDevice) {
      return;
    }

    await connectToBleVoltMeterDevice(matchedDevice, false);
    actionMessage.value = `${bleDeviceName.value} reconnected automatically`;
  } catch {
    bleConnected.value = false;
    bleDevice = null;
    bleDeviceName.value = "BLE Volt Meter";
  }
}

async function connectBleVoltMeter() {
  if (typeof window === "undefined") {
    return;
  }

  if (bleConnected.value && bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect();
    handleBleDisconnect();
    return;
  }

  const bluetooth = (navigator as any).bluetooth;
  if (!bluetooth?.requestDevice) {
    actionMessage.value = "Web Bluetooth is not supported in this browser";
    return;
  }

  try {
    actionMessage.value = "Select BLE Volt Meter device";

    const device = await bluetooth.requestDevice({
      acceptAllDevices: true,
      optionalServices: [BLE_SERVICE_UUID],
    });

    await connectToBleVoltMeterDevice(device);
  } catch (error) {
    if (bleDevice?.gatt?.connected) {
      bleDevice.gatt.disconnect();
    }

    bleDevice = null;
    bleConnected.value = false;
    bleDeviceName.value = "BLE Volt Meter";
    actionMessage.value =
      error instanceof Error
        ? error.message
        : "Unable to connect BLE Volt Meter";
  }
}

function getPhaseVoltage(slot: BatteryJobSlot, targetPhase: JobPhase) {
  if (targetPhase === "BEFORE_CHARGE") {
    return slot.beforeVoltage;
  }

  if (targetPhase === "AFTER_CHARGE") {
    return slot.afterVoltage;
  }

  return slot.deliveryVoltage;
}

function getPhaseMeasuredAt(slot: BatteryJobSlot, targetPhase: JobPhase) {
  if (targetPhase === "BEFORE_CHARGE") {
    return slot.beforeMeasuredAt;
  }

  if (targetPhase === "AFTER_CHARGE") {
    return slot.afterMeasuredAt;
  }

  return slot.deliveryMeasuredAt;
}

function getSlotVoltageLabel(slot: BatteryJobSlot, targetPhase: JobPhase) {
  const value = getPhaseVoltage(slot, targetPhase);
  return value === null ? "-" : `${value.toFixed(3)} V`;
}

function getSlotActiveMetric(targetPhase: JobPhase | null) {
  if (targetPhase === "AFTER_CHARGE") {
    return "after" as const;
  }

  if (targetPhase === "DELIVERY") {
    return "delivery" as const;
  }

  return "before" as const;
}

function formatSlotCardVoltage(value: number | null) {
  return value === null ? null : value.toFixed(2);
}

function getSlotCardUi(slot: BatteryJobSlot) {
  const targetPhase = phase.value ?? "BEFORE_CHARGE";
  const isSelected = selectedSlotNumber.value === slot.slotNumber;
  const isDone = getPhaseVoltage(slot, targetPhase) !== null;

  if (isSelected) {
    if (targetPhase === "AFTER_CHARGE") {
      return {
        titleClass: "bg-sky-700 text-white",
        bodyClass: "bg-sky-50 text-sky-950",
        borderClass: "border-sky-700",
        statusLabel: "HERE",
        statusClass: "bg-white/20 text-white",
        activeMetricClass: "bg-sky-700 text-white",
      };
    }

    if (targetPhase === "DELIVERY") {
      return {
        titleClass: "bg-amber-600 text-white",
        bodyClass: "bg-amber-50 text-amber-950",
        borderClass: "border-amber-600",
        statusLabel: "HERE",
        statusClass: "bg-white/20 text-white",
        activeMetricClass: "bg-amber-600 text-white",
      };
    }

    return {
      titleClass: "bg-emerald-800 text-white",
      bodyClass: "bg-emerald-50 text-emerald-950",
      borderClass: "border-emerald-800",
      statusLabel: "HERE",
      statusClass: "bg-white/20 text-white",
      activeMetricClass: "bg-emerald-800 text-white",
    };
  }

  if (isDone) {
    return {
      titleClass: "bg-slate-500 text-white",
      bodyClass: "bg-slate-100 text-slate-900",
      borderClass: "border-slate-400",
      statusLabel: "DONE",
      statusClass: "bg-white/20 text-white",
      activeMetricClass: "bg-slate-500 text-white",
    };
  }

  return {
    titleClass: "bg-slate-300 text-slate-800",
    bodyClass: "bg-white text-slate-900",
    borderClass: "border-slate-200",
    statusLabel: "WAIT",
    statusClass: "bg-slate-100 text-slate-700",
    activeMetricClass: "bg-slate-300 text-slate-800",
  };
}

function normalizeJob(job: any): BatteryJobRecord {
  const normalizedRackId = String(job.rackId ?? job.palletId ?? "");
  const normalizedOpenedAt = String(
    job.openedAt ?? job.workStartedAt ?? new Date().toISOString(),
  );

  return {
    id: String(job.id),
    jobRef: job.jobRef ? String(job.jobRef) : undefined,
    internalRackId: job.internalRackId ? String(job.internalRackId) : undefined,
    hasAssignedRack:
      typeof job.hasAssignedRack === "boolean"
        ? job.hasAssignedRack
        : undefined,
    rackLabel: job.rackLabel ? String(job.rackLabel) : undefined,
    phase: job.phase as JobPhase,
    status: job.status,
    workflowStage: job.workflowStage
      ? (String(job.workflowStage) as BatteryJobRecord["workflowStage"])
      : undefined,
    workflowLabel: job.workflowLabel ? String(job.workflowLabel) : undefined,
    recommendedPhase: job.recommendedPhase
      ? (String(job.recommendedPhase) as JobPhase)
      : undefined,
    isLocked: typeof job.isLocked === "boolean" ? job.isLocked : undefined,
    rackId: normalizedRackId,
    palletId: normalizedRackId,
    openedAt: normalizedOpenedAt,
    workStartedAt: normalizedOpenedAt,
    operatorId: job.operatorId ? String(job.operatorId) : null,
    operatorName: job.operatorName ? String(job.operatorName) : null,
    beforeChargeOperatorId: job.beforeChargeOperatorId
      ? String(job.beforeChargeOperatorId)
      : null,
    beforeChargeOperatorName: job.beforeChargeOperatorName
      ? String(job.beforeChargeOperatorName)
      : null,
    afterChargeOperatorId: job.afterChargeOperatorId
      ? String(job.afterChargeOperatorId)
      : null,
    afterChargeOperatorName: job.afterChargeOperatorName
      ? String(job.afterChargeOperatorName)
      : null,
    beforeDeliveryOperatorId: job.beforeDeliveryOperatorId
      ? String(job.beforeDeliveryOperatorId)
      : null,
    beforeDeliveryOperatorName: job.beforeDeliveryOperatorName
      ? String(job.beforeDeliveryOperatorName)
      : null,
    salesOrderId: job.salesOrderId ? String(job.salesOrderId) : null,
    salesOrderNumber: job.salesOrderNumber
      ? String(job.salesOrderNumber)
      : null,
    invoiceId: job.invoiceId ? String(job.invoiceId) : null,
    invoiceNumber: job.invoiceNumber ? String(job.invoiceNumber) : null,
    chargeChannelId: job.chargeChannelId ? String(job.chargeChannelId) : null,
    chargeChannelCode: job.chargeChannelCode
      ? String(job.chargeChannelCode)
      : null,
    chargeChannelName: job.chargeChannelName
      ? String(job.chargeChannelName)
      : null,
    chargeProgramId: job.chargeProgramId ? String(job.chargeProgramId) : null,
    chargeProgramCode: job.chargeProgramCode
      ? String(job.chargeProgramCode)
      : null,
    chargeProgramName: job.chargeProgramName
      ? String(job.chargeProgramName)
      : null,
    plannedDeliveryDate: job.plannedDeliveryDate
      ? String(job.plannedDeliveryDate)
      : null,
    shipTo: job.shipTo ? (String(job.shipTo) as "AAT" | "FTM") : null,
    beforeChargeCompletedAt: job.beforeChargeCompletedAt
      ? String(job.beforeChargeCompletedAt)
      : null,
    afterChargeCompletedAt: job.afterChargeCompletedAt
      ? String(job.afterChargeCompletedAt)
      : null,
    deliveryCompletedAt: job.deliveryCompletedAt
      ? String(job.deliveryCompletedAt)
      : null,
    lockedAt: job.lockedAt ? String(job.lockedAt) : null,
    slots: Array.isArray(job.slots)
      ? job.slots.map((slot: any) => ({
          id: String(slot.id),
          slotNumber: Number(slot.slotNumber),
          batteryId: String(slot.batteryId ?? ""),
          beforeVoltage:
            slot.beforeVoltage === null || slot.beforeVoltage === undefined
              ? null
              : Number(slot.beforeVoltage),
          beforeVoltageMv:
            slot.beforeVoltageMv === null || slot.beforeVoltageMv === undefined
              ? null
              : Number(slot.beforeVoltageMv),
          beforeMeasuredAt: slot.beforeMeasuredAt
            ? String(slot.beforeMeasuredAt)
            : null,
          afterVoltage:
            slot.afterVoltage === null || slot.afterVoltage === undefined
              ? null
              : Number(slot.afterVoltage),
          afterVoltageMv:
            slot.afterVoltageMv === null || slot.afterVoltageMv === undefined
              ? null
              : Number(slot.afterVoltageMv),
          afterMeasuredAt: slot.afterMeasuredAt
            ? String(slot.afterMeasuredAt)
            : null,
          deliveryVoltage:
            slot.deliveryVoltage === null || slot.deliveryVoltage === undefined
              ? null
              : Number(slot.deliveryVoltage),
          deliveryVoltageMv:
            slot.deliveryVoltageMv === null ||
            slot.deliveryVoltageMv === undefined
              ? null
              : Number(slot.deliveryVoltageMv),
          deliveryMeasuredAt: slot.deliveryMeasuredAt
            ? String(slot.deliveryMeasuredAt)
            : null,
        }))
      : [],
  };
}

function applyJob(job: any) {
  const previousJobId = currentJob.value?.id ?? null;
  const normalizedJob = normalizeJob(job);
  currentJob.value = normalizedJob;
  const jobOptionIndex = jobOptions.value.findIndex(
    (item) => item.id === normalizedJob.id,
  );
  if (jobOptionIndex >= 0) {
    jobOptions.value.splice(jobOptionIndex, 1, normalizedJob);
  } else {
    jobOptions.value.unshift(normalizedJob);
  }
  const switchedJob =
    previousJobId !== null && previousJobId !== currentJob.value.id;
  const activeSlotStillExists = currentJob.value.slots.some(
    (slot) => slot.slotNumber === selectedSlotNumber.value,
  );

  if (switchedJob) {
    workflowActionMode.value = null;
    detailModalOpen.value = false;
    editingSlotNumber.value = null;
    batteryScanInput.value = "";
    voltageInput.value = "";
    clearMeasurementPopupTimer();
    measurementPopupOpen.value = false;
  }

  adminJobId.value = currentJob.value.id;
  rackId.value = currentJob.value.rackId;
  openedAt.value = toDateTimeLocalValue(new Date(currentJob.value.openedAt));
  operatorId.value = getPhaseOperatorId(currentJob.value, phase.value);
  employeeScanInput.value = selectedEmployee.value?.code ?? "";
  salesOrderId.value = currentJob.value.salesOrderId ?? "";
  chargeChannelId.value = currentJob.value.chargeChannelId ?? "";
  chargeProgramId.value =
    currentJob.value.chargeProgramId ?? getDefaultNightChargeProgramId();
  // A save refreshes the complete job record. Keep the operator on the slot that
  // was just read; workflow navigation decides when it is time to move forward.
  if (switchedJob || !activeSlotStillExists) {
    selectedSlotNumber.value =
      currentJob.value.slots.find((slot) => !getPhaseVoltage(slot, phase.value))
        ?.slotNumber ?? 1;
  }
  syncSelectedSlot();
  if (detailModalOpen.value && editingSlotNumber.value) {
    syncModalSlotInputs();
    return;
  }

  syncWorkflowSlotInputs();
}

async function applyPendingAdminJob(
  jobSelection?: string | { value?: string; label?: string } | null,
) {
  const normalized =
    typeof jobSelection === "object" && jobSelection !== null
      ? String(jobSelection.value ?? jobSelection.label ?? "").trim()
      : String(jobSelection ?? "").trim();
  const match = pendingAdminJobOptions.value.find(
    (job) => job.value === normalized || job.label === normalized,
  );
  const jobId = match?.value ?? normalized;

  adminJobId.value = jobId;

  if (!jobId) {
    return;
  }

  const pendingJob = jobOptions.value.find((job) => job.id === jobId);

  if (!pendingJob) {
    return;
  }

  workflowActionMode.value = null;
  detailModalOpen.value = false;
  editingSlotNumber.value = null;
  jobDetailsSaved.value = false;
  batteryScanInput.value = "";
  voltageInput.value = "";
  applyJob(pendingJob);

  if (hasPersistedRequiredJobDetails.value) {
    jobDetailsSaved.value = true;
    scanTarget.value = "battery";
    await nextTick();
    blurActiveElement();

    if (
      typeof window !== "undefined" &&
      typeof window.requestAnimationFrame === "function"
    ) {
      await new Promise<void>((resolve) =>
        window.requestAnimationFrame(() => resolve()),
      );
      await new Promise<void>((resolve) =>
        window.requestAnimationFrame(() => resolve()),
      );
    }

    await armDefaultWorkflow();
    await revealRackLayout();
    const workflowLabel =
      workflowActionMode.value === "battery" ? "Battery ID" : "Voltage";
    actionMessage.value = `เปิดใบงาน ${pendingJob.jobRef ?? pendingJob.id.slice(-8).toUpperCase()} ต่อที่ slot ${selectedSlotNumber.value} พร้อมรับ ${workflowLabel}`;
    return;
  }

  actionMessage.value = `เลือกใบงาน ${pendingJob.jobRef ?? pendingJob.id.slice(-8).toUpperCase()} แล้ว กรุณาเลือก Emp ID และกด Save เพื่อเปิด Rack Layout`;
  await revealRackLayout();
  if (phase.value === "BEFORE_CHARGE") {
    await focusRackInputAfterMenuClose();
    return;
  }

  await focusEmployeeInput();
}

function syncSelectedSlot() {
  if (!currentJob.value) {
    selectedSlotNumber.value = 1;
    editingSlotNumber.value = null;
    return;
  }

  const slots = currentJob.value.slots;
  const selectedExists = slots.some(
    (slot) => slot.slotNumber === selectedSlotNumber.value,
  );

  if (selectedExists) {
    return;
  }

  selectedSlotNumber.value = firstIncompleteSlotNumber.value || 1;
}

function selectPhase(nextPhase: JobPhase) {
  phase.value = nextPhase;
  resetPhaseContext();
  jobDetailsSaved.value = false;
  workflowActionMode.value = null;
  selectedSlotNumber.value = 1;
  editingSlotNumber.value = null;
  batteryScanInput.value = "";
  voltageInput.value = "";
  actionMessage.value =
    nextPhase === "BEFORE_CHARGE"
      ? "เลือก Rack งานที่ Admin สร้างไว้ แล้วเริ่ม QC Before Charge"
      : nextPhase === "AFTER_CHARGE"
        ? "เลือก Rack เดิมเพื่อทำ QC After Charge"
        : "เลือก Rack เดิมเพื่อทำ QC Before Delivery";
  if (nextPhase === "BEFORE_CHARGE") {
    void focusQcJobInputAfterPhaseSelect();
    return;
  }

  void focusRackInputAfterMenuClose();
}

function selectSlot(slotNumber: number) {
  if (!currentJob.value) {
    return;
  }

  if (!workflowActionMode.value) {
    workflowActionMode.value = getDefaultWorkflowActionMode(
      phase.value ?? "BEFORE_CHARGE",
    );
  }
  editingSlotNumber.value = slotNumber;
  syncModalSlotInputs();
  detailModalOpen.value = true;
}

function activateNextWorkflowSlot() {
  if (!currentJob.value) {
    selectedSlotNumber.value = 1;
    return;
  }

  const nextSlot = currentJob.value.slots.find(
    (slot) => !getPhaseVoltage(slot, phase.value),
  );
  if (!nextSlot) {
    return;
  }

  selectedSlotNumber.value = nextSlot.slotNumber;
  batteryScanInput.value = nextSlot.batteryId ?? "";
}

function closeSlotDetail() {
  detailModalOpen.value = false;
  editingSlotNumber.value = null;
  activateNextModeSlot();
  syncWorkflowSlotInputs();
  void restoreWorkflowFocus();
}

async function advanceToSlot(nextSlotNumber: number | null) {
  detailModalOpen.value = false;
  editingSlotNumber.value = null;

  if (!nextSlotNumber) {
    syncWorkflowSlotInputs();
    await restoreWorkflowFocus();
    return;
  }

  selectedSlotNumber.value = nextSlotNumber;
  syncWorkflowSlotInputs();
  voltageInput.value = "";
  await nextTick();
  await restoreWorkflowFocus();
}

async function fetchMasterData<T>(url: string) {
  try {
    return await $fetch<T>(url);
  } catch {
    // A Nuxt dev-server restart can briefly return 500 while Prisma reconnects.
    await wait(350);
    return await $fetch<T>(url);
  }
}

async function loadMasters() {
  const [
    employeeResponse,
    salesOrderResponse,
    chargeChannelResponse,
    chargeProgramResponse,
    jobResponse,
  ] = await Promise.all([
    fetchMasterData<{ employees: EmployeeItem[] }>("/api/employees"),
    fetchMasterData<{ salesOrders: SalesOrderItem[] }>("/api/sales-orders"),
    fetchMasterData<{ chargeChannels: ChargeChannelItem[] }>(
      "/api/charge-channels",
    ),
    fetchMasterData<{ chargePrograms: ChargeProgramItem[] }>(
      "/api/charge-programs",
    ),
    fetchMasterData<{ jobs: BatteryJobRecord[] }>("/api/battery-jobs"),
  ]);

  employees.value = employeeResponse.employees;
  salesOrders.value = salesOrderResponse.salesOrders;
  chargeChannels.value = chargeChannelResponse.chargeChannels;
  chargePrograms.value = chargeProgramResponse.chargePrograms;
  jobOptions.value = jobResponse.jobs.map(normalizeJob);

  if (!chargeProgramId.value) {
    chargeProgramId.value = getDefaultNightChargeProgramId();
  }
}

async function createEmployee() {
  if (!newEmployeeCode.value.trim() || !newEmployeeName.value.trim()) {
    actionMessage.value = "กรอกรหัสและชื่อพนักงานก่อนเพิ่มข้อมูล";
    return;
  }

  const response = await $fetch<{ employee: EmployeeItem }>("/api/employees", {
    method: "POST",
    body: {
      code: newEmployeeCode.value,
      name: newEmployeeName.value,
    },
  });

  await loadMasters();
  selectEmployeeById(response.employee.id);
  newEmployeeCode.value = "";
  newEmployeeName.value = "";
  actionMessage.value = "เพิ่มพนักงานใหม่เรียบร้อยแล้ว";
}

async function createSalesOrder() {
  if (!newSoNumber.value.trim()) {
    actionMessage.value = "กรอก SO Number ก่อนเพิ่มข้อมูล";
    return;
  }

  const response = await $fetch<{ salesOrder: SalesOrderItem }>(
    "/api/sales-orders",
    {
      method: "POST",
      body: {
        soNumber: newSoNumber.value,
        description: newSoDescription.value,
      },
    },
  );

  await loadMasters();
  salesOrderId.value = response.salesOrder.id;
  newSoNumber.value = "";
  newSoDescription.value = "";
  actionMessage.value = "เพิ่ม SO ใหม่เรียบร้อยแล้ว";
}

async function openCurrentJob(skipRequiredValidation = false) {
  if (!phase.value) {
    actionMessage.value = "เลือก phase ก่อนเปิดงาน";
    return;
  }

  if (!currentJob.value) {
    actionMessage.value =
      "หน้านี้ใช้สำหรับทำงานจากใบงานที่ Admin สร้างไว้แล้ว กรุณาโหลด Rack ก่อน";
    return;
  }

  if (!rackId.value.trim()) {
    actionMessage.value = "กรอกหรือ scan rack ID ก่อน";
    return;
  }

  if (!skipRequiredValidation && !operatorId.value) {
    actionMessage.value = "เลือกเจ้าหน้าที่ก่อนเปิดงาน";
    return;
  }

  if (
    !skipRequiredValidation &&
    phase.value === "BEFORE_CHARGE" &&
    !chargeChannelId.value
  ) {
    actionMessage.value = "QC Before Charge ต้องเลือก Charging Channel ก่อน";
    return;
  }

  if (
    !skipRequiredValidation &&
    phase.value === "BEFORE_CHARGE" &&
    !chargeProgramId.value
  ) {
    actionMessage.value = "QC Before Charge ต้องเลือก Charging Program ก่อน";
    return;
  }

  isBusy.value = true;
  loadError.value = "";

  try {
    const response = await $fetch<{ job: BatteryJobRecord }>(
      "/api/battery-jobs",
      {
        method: "POST",
        body: {
          jobId: currentJob.value.id,
          phase: phase.value,
          rackId: rackId.value,
          operatorId: operatorId.value || null,
          salesOrderId: salesOrderId.value || null,
          chargeChannelId: chargeChannelId.value || null,
          chargeProgramId: chargeProgramId.value || null,
          openedAt: new Date(openedAt.value).toISOString(),
        },
      },
    );

    applyJob(response.job);
    jobDetailsSaved.value = true;
    await armDefaultWorkflow();
    await revealRackLayout();
    actionMessage.value =
      phase.value === "BEFORE_CHARGE"
        ? `เปิดใบงาน rack ${rackId.value.toUpperCase()} แล้ว เริ่มที่ slot 1`
        : `เปิด rack ${rackId.value.toUpperCase()} แล้ว เริ่มวัดตามลำดับเดิม`;
  } catch (error) {
    loadError.value =
      error instanceof Error ? error.message : "Unable to open job";
  } finally {
    isBusy.value = false;
  }
}

async function loadExistingRack() {
  if (!phase.value) {
    actionMessage.value = "เลือก phase ก่อนโหลด rack";
    return;
  }

  if (!rackId.value.trim()) {
    actionMessage.value = "กรอก rack ID ก่อนค้นหา";
    return;
  }

  isBusy.value = true;
  loadError.value = "";

  try {
    const response = await $fetch<{ job: BatteryJobRecord }>(
      "/api/battery-jobs/pallet",
      {
        query: {
          rackId: rackId.value,
          phase: phase.value,
        },
      },
    );

    applyJob(response.job);
    jobDetailsSaved.value = true;
    await armDefaultWorkflow();
    await revealRackLayout();
    actionMessage.value = `โหลด rack ${rackId.value.toUpperCase()} สำเร็จ`;
  } catch (error) {
    loadError.value =
      error instanceof Error ? error.message : "Unable to load rack";
  } finally {
    isBusy.value = false;
  }
}

async function saveSelectedBatteryId() {
  if (!currentJob.value) {
    actionMessage.value = "เปิดงานก่อนบันทึก battery ID";
    return;
  }

  if (!batteryScanInput.value.trim()) {
    actionMessage.value = "กรอกหรือ scan Battery ID ก่อน";
    return;
  }

  isSavingBattery.value = true;

  try {
    const scannedBatteryId = batteryScanInput.value.trim().toUpperCase();
    const savedSlotNumber = detailModalOpen.value
      ? (editingSlotNumber.value ?? selectedSlotNumber.value)
      : selectedSlotNumber.value;
    const duplicateSlot = checkDuplicateBatteryId.value
      ? currentJob.value.slots.find(
          (slot) =>
            slot.slotNumber !== savedSlotNumber &&
            slot.batteryId.trim().toUpperCase() === scannedBatteryId,
        )
      : null;

    if (duplicateSlot) {
      batteryScanInput.value = "";
      showDuplicateBatteryIdPopup(scannedBatteryId, duplicateSlot.slotNumber);
      actionMessage.value = `Battery ID ${scannedBatteryId} มีอยู่แล้วใน slot ${duplicateSlot.slotNumber}`;
      await focusWorkflowInput();
      return;
    }

    const response = await $fetch<{ job: BatteryJobRecord }>(
      `/api/battery-jobs/${currentJob.value.id}/slot-battery`,
      {
        method: "PATCH",
        body: {
          slotNumber: savedSlotNumber,
          batteryId: scannedBatteryId,
          checkDuplicateId: checkDuplicateBatteryId.value,
        },
      },
    );

    applyJob(response.job);
    const nextSlot =
      currentJob.value?.slots.find((slot) => !slot.batteryId.trim()) ?? null;

    if (!detailModalOpen.value) {
      if (nextSlot) {
        showBatteryIdPopup(scannedBatteryId, savedSlotNumber);
        actionMessage.value = `บันทึก Battery ID ให้ slot ${savedSlotNumber} สำเร็จ`;
        await wait(FEEDBACK_POPUP_DURATION);
        selectedSlotNumber.value = nextSlot.slotNumber;
        syncWorkflowSlotInputs();
        actionMessage.value = `บันทึก Battery ID ให้ slot ${savedSlotNumber} แล้ว เลื่อนไป slot ${nextSlot.slotNumber}`;
      } else {
        batteryScanInput.value = "";
        showBatteryIdCompletePopup();
        actionMessage.value = "บันทึก Battery ID ครบทุก slot แล้ว";
        await wait(WORKFLOW_COMPLETE_POPUP_DURATION);
        workflowActionMode.value = "voltage";
        selectedSlotNumber.value =
          currentJob.value?.slots.find(
            (slot) => getPhaseVoltage(slot, phase.value) === null,
          )?.slotNumber ?? 1;
        syncWorkflowSlotInputs();
        actionMessage.value =
          "บันทึก Battery ID ครบทุก slot แล้ว พร้อมวัด Voltage ที่ slot 1";
        await restoreWorkflowFocus();
      }
      return;
    }

    showBatteryIdPopup(scannedBatteryId, savedSlotNumber);
    await wait(FEEDBACK_POPUP_DURATION);
    actionMessage.value = nextSlot
      ? `บันทึก Battery ID ให้ slot ${savedSlotNumber} แล้ว เลื่อนไป slot ${nextSlot.slotNumber}`
      : `บันทึก Battery ID ครบทุก slot แล้ว`;
    await advanceToSlot(nextSlot?.slotNumber ?? null);
  } catch (error) {
    actionMessage.value =
      error instanceof Error ? error.message : "Unable to save battery ID";
  } finally {
    isSavingBattery.value = false;
  }
}

async function saveActiveSlotVoltage(voltageValue: string) {
  if (!phase.value) {
    actionMessage.value = "เลือก phase ก่อนบันทึกแรงดัน";
    return;
  }

  if (!currentJob.value) {
    actionMessage.value = "เปิดงานก่อนบันทึกแรงดัน";
    return;
  }

  if (!voltageValue.trim()) {
    actionMessage.value = "ไม่มีค่า Voltage ที่อ่านเข้ามา";
    return;
  }

  const parsedVoltage = Number.parseFloat(voltageValue);
  if (!Number.isFinite(parsedVoltage)) {
    actionMessage.value = "ค่า Voltage ที่อ่านมาไม่ถูกต้อง";
    return;
  }

  const targetSlotNumber = detailModalOpen.value
    ? (editingSlotNumber.value ?? selectedSlotNumber.value)
    : selectedSlotNumber.value;
  const activeSlot =
    currentJob.value.slots.find(
      (slot) => slot.slotNumber === targetSlotNumber,
    ) ?? null;
  if (!activeSlot) {
    actionMessage.value = "ไม่พบ active slot สำหรับบันทึกแรงดัน";
    return;
  }

  if (
    phase.value === "BEFORE_CHARGE" &&
    !(activeSlot.batteryId || batteryScanInput.value.trim())
  ) {
    actionMessage.value = `slot ${targetSlotNumber} ต้องมี Battery ID ก่อนวัดแรงดัน`;
    return;
  }

  isMeasuring.value = true;

  try {
    const measuredSlotNumber = targetSlotNumber;
    const normalizedVoltage = parsedVoltage.toFixed(3);
    const response = await $fetch<{ job: BatteryJobRecord }>(
      `/api/battery-jobs/${currentJob.value.id}/slot`,
      {
        method: "PATCH",
        body: {
          phase: phase.value,
          slotNumber: measuredSlotNumber,
          batteryId:
            phase.value === "BEFORE_CHARGE"
              ? activeSlot.batteryId || batteryScanInput.value || ""
              : null,
          voltage: normalizedVoltage,
          voltageUnit: "V",
          measuredAt: new Date().toISOString(),
        },
      },
    );

    applyJob(response.job);
    showMeasurementPopup(Number(normalizedVoltage), measuredSlotNumber);
    voltageInput.value = normalizedVoltage;
    voltageScanInput.value = "";

    if (!detailModalOpen.value) {
      const nextSlot = currentJob.value?.slots.find(
        (slot) => getPhaseVoltage(slot, phase.value) === null,
      );
      if (nextSlot) {
        actionMessage.value = `บันทึก Voltage slot ${measuredSlotNumber} สำเร็จ`;
        await wait(FEEDBACK_POPUP_DURATION);
        selectedSlotNumber.value = nextSlot.slotNumber;
        syncWorkflowSlotInputs();
        actionMessage.value = `บันทึก Voltage slot ${measuredSlotNumber} แล้ว เลื่อนไป slot ${nextSlot.slotNumber}`;
      } else {
        voltageInput.value = "";
        actionMessage.value = `บันทึก Voltage ครบทุก slot แล้ว`;
        await wait(FEEDBACK_POPUP_DURATION);
      }
      await restoreWorkflowFocus();
      return;
    }

    const nextSlot =
      currentJob.value?.slots.find(
        (slot) => getPhaseVoltage(slot, phase.value) === null,
      ) ?? null;
    await wait(FEEDBACK_POPUP_DURATION);
    actionMessage.value = nextSlot
      ? `บันทึก Voltage slot ${measuredSlotNumber} แล้ว เลื่อนไป slot ${nextSlot.slotNumber}`
      : `บันทึก Voltage ครบทุก slot แล้ว`;
    await advanceToSlot(nextSlot?.slotNumber ?? null);
  } catch (error) {
    actionMessage.value =
      error instanceof Error ? error.message : "Unable to save voltage";
  } finally {
    isMeasuring.value = false;
  }
}

async function confirmCurrentPhase() {
  if (!currentJob.value) {
    actionMessage.value = "ยังไม่มีงานให้ยืนยัน";
    return;
  }

  if (!phase.value) {
    actionMessage.value = "เลือก phase ก่อนยืนยันงาน";
    return;
  }

  isConfirming.value = true;

  try {
    const confirmedPhase = phase.value;
    const response = await $fetch<{ job: BatteryJobRecord }>(
      `/api/battery-jobs/${currentJob.value.id}/confirm`,
      {
        method: "PATCH",
        body: {
          phase: confirmedPhase,
          operatorId: operatorId.value || null,
        },
      },
    );

    phase.value = response.job.phase;
    applyJob(response.job);
    void loadPhaseQueueCounts().catch(() => undefined);
    actionMessage.value =
      confirmedPhase === "DELIVERY"
        ? "ยืนยัน QC Before Delivery สำเร็จแล้ว รอ Admin ปิดงานและ set Shipped"
        : confirmedPhase === "BEFORE_CHARGE"
          ? "ยืนยัน QC Before Charge สำเร็จแล้ว ระบบพร้อมไปต่อที่ After Charge"
          : "ยืนยัน QC After Charge สำเร็จแล้ว ระบบพร้อมไปต่อที่ QC Before Delivery";
  } catch (error) {
    actionMessage.value =
      error instanceof Error ? error.message : "Unable to confirm phase";
  } finally {
    isConfirming.value = false;
  }
}

function formatDateTime(value: string | null) {
  if (!value) {
    return "-";
  }

  return new Date(value).toLocaleString("th-TH", {
    dateStyle: "short",
    timeStyle: "short",
  });
}

onMounted(async () => {
  try {
    await loadMasters();
    await loadPhaseQueueCounts().catch(() => undefined);
    phaseQueueRefreshTimer = setInterval(() => {
      void loadPhaseQueueCounts();
    }, 30_000);
    await tryAutoReconnectBleVoltMeter();
  } catch (error) {
    loadError.value =
      error instanceof Error ? error.message : "Unable to load master data";
  }
});

onBeforeUnmount(() => {
  if (phaseQueueRefreshTimer) {
    clearInterval(phaseQueueRefreshTimer);
  }
});

watch([detailModalOpen, workflowActionMode], async ([isOpen]) => {
  if (!isOpen) {
    await restoreWorkflowFocus();
    return;
  }

  await focusActiveModalField();
});

watch(
  [
    calibratePointOneMeter,
    calibratePointOneEsp,
    calibratePointTwoMeter,
    calibratePointTwoEsp,
  ],
  () => {
    updateCalibrationPreview();
  },
);

// Native datalist selection in UInput does not consistently emit a change event.
// Loading an exact listed Rack here makes selecting and scanning follow the same flow.
onBeforeUnmount(() => {
  clearMeasurementPopupTimer();

  if (bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect();
  }
});
</script>

<template>
  <main
    class="min-h-screen bg-[linear-gradient(135deg,_#eef7e6_0%,_#e8f0ff_50%,_#fff4dc_100%)] px-4 py-3 text-slate-950"
  >
    <section class="mx-auto flex max-w-[1600px] flex-col gap-3">
      <template v-if="hasPhaseSelected">
        <div
          class="overflow-hidden rounded-[22px] border shadow-[0_18px_48px_rgba(15,23,42,0.08)]"
          :class="
            phase === 'BEFORE_CHARGE'
              ? 'border-lime-500 bg-[linear-gradient(180deg,_#4d8f00_0%,_#65a30d_100%)] text-white'
              : phase === 'AFTER_CHARGE'
                ? 'border-sky-500 bg-[linear-gradient(180deg,_#1874b8_0%,_#0ea5e9_100%)] text-white'
                : 'border-amber-600 bg-[linear-gradient(180deg,_#d97706_0%,_#f59e0b_100%)] text-white'
          "
        >
          <div
            class="grid items-center gap-4 px-5 py-4 lg:grid-cols-[minmax(0,1fr)_auto_minmax(0,1fr)] lg:px-6 lg:py-4"
          >
            <div class="flex items-center gap-4">
              <div
                class="flex h-12 w-12 items-center justify-center rounded-xl bg-white/18 ring-1 ring-white/20 lg:h-14 lg:w-14"
              >
                <UIcon
                  :name="currentPhaseMeta?.icon ?? 'i-lucide-battery'"
                  class="size-6 text-white lg:size-7"
                />
              </div>
              <div>
                <div
                  class="text-[1.55rem] font-black leading-none lg:text-[1.8rem]"
                >
                  {{ currentPhaseMeta?.label }}
                </div>
                <div
                  class="mt-1 text-xs font-semibold text-white/85 lg:text-sm"
                >
                  {{ currentPhaseMeta?.detail }}
                </div>
              </div>
            </div>

            <div class="flex justify-center lg:justify-self-center">
              <img
                src="/branding/logo-puma-battery.png"
                alt="PUMA Battery"
                class="h-14 w-auto object-contain lg:h-16"
              />
            </div>

            <div class="flex items-center justify-end gap-3">
              <UButton
                color="neutral"
                variant="solid"
                class="rounded-full border border-white/30 bg-white/12 px-4 py-2.5 text-sm font-black text-white hover:bg-white/20 active:bg-white/25"
                @click="backToPhaseLanding"
              >
                Change Phase
              </UButton>
            </div>
          </div>
        </div>

        <div ref="jobDetailsCard">
          <UCard
            :ui="{
              root: 'rounded-[22px] border-0 bg-white/92 shadow-[0_18px_48px_rgba(15,23,42,0.08)]',
              body: 'p-4',
            }"
          >
            <div class="flex items-center justify-between gap-3">
              <div
                class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500"
              >
                {{ currentPhaseJobDetailsTitle }}
              </div>
              <div class="flex items-center justify-end gap-3">
                <div
                  v-if="jobDetailsCollapsed"
                  class="text-xs font-semibold text-slate-500"
                >
                  Workflow Active
                </div>
                <UButton
                  color="neutral"
                  variant="solid"
                  :loading="isBusy"
                  :disabled="!canSaveJobDetails"
                  class="rounded-full bg-emerald-700 px-4 py-2 text-sm font-black text-white hover:bg-emerald-800 active:bg-emerald-900 disabled:cursor-not-allowed disabled:bg-slate-300 disabled:text-slate-500"
                  @click="openCurrentJob()"
                >
                  Save
                </UButton>
              </div>
            </div>

            <template v-if="jobDetailsCollapsed">
              <div class="mt-3 grid gap-2 xl:grid-cols-6">
                <UInput
                  :model-value="currentJob?.jobRef || '-'"
                  readonly
                  color="neutral"
                  variant="ghost"
                  size="md"
                  class="w-full"
                  :ui="qcReadonlyUi"
                />
                <UInput
                  :model-value="currentJob?.rackId || '-'"
                  readonly
                  color="neutral"
                  variant="ghost"
                  size="md"
                  class="w-full"
                  :ui="qcReadonlyUi"
                />
                <UInput
                  :model-value="selectedEmployee?.code || '-'"
                  readonly
                  color="neutral"
                  variant="ghost"
                  size="md"
                  class="w-full"
                  :ui="qcReadonlyUi"
                />
                <UInput
                  :model-value="selectedEmployee?.name || '-'"
                  readonly
                  color="neutral"
                  variant="ghost"
                  size="md"
                  class="w-full"
                  :ui="qcReadonlyUi"
                />
                <UInput
                  :model-value="currentJob?.chargeChannelName || '-'"
                  readonly
                  color="neutral"
                  variant="ghost"
                  size="md"
                  class="w-full"
                  :ui="qcReadonlyUi"
                />
                <UInput
                  :model-value="currentJob?.chargeProgramName || '-'"
                  readonly
                  color="neutral"
                  variant="ghost"
                  size="md"
                  class="w-full"
                  :ui="qcReadonlyUi"
                />
              </div>
            </template>

            <template v-else-if="phase === 'BEFORE_CHARGE'">
              <div
                class="mt-4 grid gap-3 xl:grid-cols-[1.15fr_repeat(4,minmax(0,1fr))]"
              >
                <UFormField
                  label="QC Job"
                  name="admin-qc-job"
                  required
                  :ui="qcFieldUi"
                >
                  <div id="qc-job-input">
                    <USelectMenu
                      v-model="adminJobId"
                      :items="pendingAdminJobOptions"
                      value-key="value"
                      label-key="label"
                      placeholder="เลือกใบงานที่ Admin สร้างไว้"
                      color="neutral"
                      variant="outline"
                      size="md"
                      class="w-full"
                      :disabled="jobDetailsLocked"
                      :search-input="false"
                      :reset-search-term-on-blur="true"
                      :reset-search-term-on-select="true"
                      :ui="qcSelectUi"
                      @update:model-value="applyPendingAdminJob"
                    >
                      <template #default>
                        <span
                          v-if="selectedPendingAdminJobOption"
                          class="block truncate"
                        >
                          <span class="font-black text-slate-950">{{
                            selectedPendingAdminJobOption.jobRef
                          }}</span>
                          <span class="ml-2 text-slate-500"
                            >{{
                              selectedPendingAdminJobOption.salesOrderNumber
                            }}
                            ·
                            {{
                              selectedPendingAdminJobOption.invoiceNumber
                            }}</span
                          >
                        </span>
                        <span v-else class="text-slate-400"
                          >เลือกใบงานที่ Admin สร้างไว้</span
                        >
                      </template>

                      <template #item-label="{ item }">
                        <span class="font-black text-slate-950">{{
                          item.jobRef
                        }}</span>
                        <span class="ml-2 text-slate-500"
                          >{{ item.salesOrderNumber }} ·
                          {{ item.invoiceNumber }}</span
                        >
                      </template>
                    </USelectMenu>
                  </div>
                </UFormField>

                <UFormField label="SO" name="sales-order" :ui="qcFieldUi">
                  <UInput
                    :model-value="currentJob?.salesOrderNumber || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField
                  label="Invoice"
                  name="invoice-number"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="currentJob?.invoiceNumber || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField label="Ship To" name="ship-to" :ui="qcFieldUi">
                  <UInput
                    :model-value="currentJob?.shipTo || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField
                  label="Ship Date"
                  name="planned-delivery-date"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="
                      currentJob?.plannedDeliveryDate
                        ? formatDateTime(currentJob.plannedDeliveryDate)
                        : '-'
                    "
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>
              </div>

              <div
                class="mt-3 grid gap-3 xl:grid-cols-[1.15fr_repeat(4,minmax(0,1fr))]"
              >
                <UFormField
                  label="Rack #"
                  name="rack-id"
                  required
                  :ui="qcFieldUi"
                >
                  <div class="flex gap-2">
                    <UInput
                      id="qc-rack-input"
                      v-model="rackId"
                      type="text"
                      placeholder="Scan Rack QR"
                      :readonly="rackDetailsLocked"
                      color="neutral"
                      variant="outline"
                      size="md"
                      class="min-w-0 flex-1"
                      :ui="qcInputUpperUi"
                      autocomplete="off"
                      autocapitalize="characters"
                      autocorrect="off"
                      spellcheck="false"
                      lang="en"
                      @update:model-value="rackId = sanitizeRackInput($event)"
                      @keyup.enter="handleRackInput(rackId)"
                    />
                    <UPopover v-model:open="rackPickerOpen">
                      <UButton
                        icon="i-lucide-list"
                        color="neutral"
                        variant="outline"
                        size="md"
                        aria-label="เลือก Rack จากรายการ"
                        :disabled="rackDetailsLocked"
                      />
                      <template #content>
                        <div class="w-72 p-2">
                          <p
                            class="px-2 pb-2 text-xs font-bold uppercase tracking-[0.16em] text-slate-500"
                          >
                            Racks in progress
                          </p>
                          <div class="max-h-64 space-y-1 overflow-y-auto">
                            <UButton
                              v-for="job in rackOptions"
                              :key="job.value"
                              :label="job.label"
                              color="neutral"
                              variant="ghost"
                              class="w-full justify-start text-left"
                              @click="selectRackFromPicker(job.value)"
                            />
                            <p
                              v-if="!rackOptions.length"
                              class="px-2 py-3 text-sm text-slate-500"
                            >
                              ไม่มี Rack ที่ทำต่อได้ใน phase นี้
                            </p>
                          </div>
                        </div>
                      </template>
                    </UPopover>
                  </div>
                </UFormField>

                <UFormField
                  label="Emp ID"
                  name="employee-id"
                  required
                  :ui="qcFieldUi"
                >
                  <div id="qc-employee-input">
                    <USelectMenu
                      v-model="operatorId"
                      :items="employeeSelectOptions"
                      value-key="value"
                      label-key="label"
                      placeholder="เลือก Emp ID"
                      :disabled="jobDetailsLocked"
                      color="neutral"
                      variant="outline"
                      size="md"
                      class="w-full"
                      :search-input="true"
                      :reset-search-term-on-blur="true"
                      :reset-search-term-on-select="true"
                      :ui="qcSelectUi"
                      @update:model-value="handleEmployeeSelection"
                    >
                      <template #default>
                        <span
                          v-if="selectedEmployeeOption"
                          class="block truncate"
                        >
                          <span class="font-black text-slate-950">{{
                            selectedEmployeeOption.code
                          }}</span>
                          <span class="ml-2 text-slate-500">{{
                            selectedEmployeeOption.name
                          }}</span>
                        </span>
                        <span v-else class="text-slate-400">เลือก Emp ID</span>
                      </template>

                      <template #item-label="{ item }">
                        <span class="font-black text-slate-950">{{
                          item.code
                        }}</span>
                        <span class="ml-2 text-slate-500">{{ item.name }}</span>
                      </template>
                    </USelectMenu>
                  </div>
                </UFormField>

                <UFormField
                  label="Employee Name"
                  name="employee-name"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="selectedEmployee?.name || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField
                  label="Charge Channel"
                  name="charge-channel"
                  required
                  :ui="qcFieldUi"
                >
                  <USelectMenu
                    v-model="chargeChannelId"
                    :items="chargeChannelOptions"
                    value-key="value"
                    label-key="label"
                    placeholder="เลือก Channel"
                    :disabled="jobDetailsLocked"
                    color="neutral"
                    variant="outline"
                    size="md"
                    class="w-full"
                    :search-input="false"
                    :reset-search-term-on-blur="true"
                    :reset-search-term-on-select="true"
                    :ui="qcSelectUi"
                  />
                </UFormField>

                <UFormField
                  label="Charge Program"
                  name="charge-program"
                  required
                  :ui="qcFieldUi"
                >
                  <USelectMenu
                    v-model="chargeProgramId"
                    :items="chargeProgramOptions"
                    value-key="value"
                    label-key="label"
                    placeholder="เลือก Program"
                    :disabled="jobDetailsLocked"
                    color="neutral"
                    variant="outline"
                    size="md"
                    class="w-full"
                    :search-input="false"
                    :reset-search-term-on-blur="true"
                    :reset-search-term-on-select="true"
                    :ui="qcSelectUi"
                  />
                </UFormField>
              </div>

              <div
                v-if="jobDetailsLocked"
                class="mt-3 rounded-[14px] border border-amber-300 bg-amber-50 px-4 py-3 text-sm font-semibold text-amber-950"
              >
                Job Details ถูกล็อกแล้ว เพราะ phase นี้ถูก Confirm แล้ว
              </div>
              <div
                v-else-if="rackDetailsLocked"
                class="mt-3 rounded-[14px] border border-sky-300 bg-sky-50 px-4 py-3 text-sm font-semibold text-sky-950"
              >
                Rack # และ QC Job ถูกล็อกแล้วหลังเริ่มบันทึก slot แต่ยังแก้ Emp
                ID, Charge Channel และ Charge Program ได้จนกว่าจะ Confirm
              </div>
              <div
                v-else-if="jobHasRecordedSlotData && canEditRackDuringBeforeCharge"
                class="mt-3 rounded-[14px] border border-sky-300 bg-sky-50 px-4 py-3 text-sm font-semibold text-sky-950"
              >
                ระหว่าง QC Before Charge ยังแก้ Rack # ได้จนกว่าจะ Confirm phase นี้
              </div>
            </template>

            <template v-else>
              <div
                class="mt-4 grid gap-3 xl:grid-cols-[1.15fr_repeat(4,minmax(0,1fr))]"
              >
                <UFormField
                  label="QC Job"
                  name="phase-qc-job"
                  required
                  :ui="qcFieldUi"
                >
                  <div id="qc-job-input">
                    <USelectMenu
                      v-model="adminJobId"
                      :items="pendingAdminJobOptions"
                      value-key="value"
                      label-key="label"
                      placeholder="เลือกใบงานที่ทำ phase ก่อนหน้าแล้ว"
                      color="neutral"
                      variant="outline"
                      size="md"
                      class="w-full"
                      :disabled="jobDetailsLocked"
                      :search-input="false"
                      :reset-search-term-on-blur="true"
                      :reset-search-term-on-select="true"
                      :ui="qcSelectUi"
                      @update:model-value="applyPendingAdminJob"
                    >
                      <template #default>
                        <span
                          v-if="selectedPendingAdminJobOption"
                          class="block truncate"
                        >
                          <span class="font-black text-slate-950">{{
                            selectedPendingAdminJobOption.jobRef
                          }}</span>
                          <span class="ml-2 text-slate-500"
                            >{{
                              selectedPendingAdminJobOption.salesOrderNumber
                            }}
                            ·
                            {{
                              selectedPendingAdminJobOption.invoiceNumber
                            }}</span
                          >
                        </span>
                        <span v-else class="text-slate-400">เลือกใบงาน</span>
                      </template>

                      <template #item-label="{ item }">
                        <span class="font-black text-slate-950">{{
                          item.jobRef
                        }}</span>
                        <span class="ml-2 text-slate-500"
                          >{{ item.salesOrderNumber }} ·
                          {{ item.invoiceNumber }}</span
                        >
                      </template>
                    </USelectMenu>
                  </div>
                </UFormField>

                <UFormField label="SO" name="sales-order" :ui="qcFieldUi">
                  <UInput
                    :model-value="currentJob?.salesOrderNumber || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField
                  label="Invoice"
                  name="invoice-number"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="currentJob?.invoiceNumber || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField label="Ship To" name="ship-to" :ui="qcFieldUi">
                  <UInput
                    :model-value="currentJob?.shipTo || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField
                  label="Ship Date"
                  name="planned-delivery-date"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="
                      currentJob?.plannedDeliveryDate
                        ? formatDateTime(currentJob.plannedDeliveryDate)
                        : '-'
                    "
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>
              </div>

              <div
                class="mt-3 grid gap-3 xl:grid-cols-[1.15fr_repeat(4,minmax(0,1fr))]"
              >
                <UFormField
                  label="Rack #"
                  name="rack-id"
                  required
                  :ui="qcFieldUi"
                >
                  <div class="flex gap-2">
                    <UInput
                      id="qc-rack-input"
                      v-model="rackId"
                      type="text"
                      placeholder="Scan Rack QR เพื่อค้นหาใบงาน"
                      :readonly="jobDetailsLocked"
                      color="neutral"
                      variant="outline"
                      size="md"
                      class="min-w-0 flex-1"
                      :ui="qcInputUpperUi"
                      autocomplete="off"
                      autocapitalize="characters"
                      autocorrect="off"
                      spellcheck="false"
                      lang="en"
                      @update:model-value="rackId = sanitizeRackInput($event)"
                      @keyup.enter="handleRackInput(rackId)"
                    />
                    <UPopover v-model:open="rackPickerOpen">
                      <UButton
                        icon="i-lucide-list"
                        color="neutral"
                        variant="outline"
                        size="md"
                        aria-label="เลือก Rack จากรายการ"
                        :disabled="jobDetailsLocked"
                      />
                      <template #content>
                        <div class="w-72 p-2">
                          <p
                            class="px-2 pb-2 text-xs font-bold uppercase tracking-[0.16em] text-slate-500"
                          >
                            Racks in progress
                          </p>
                          <div class="max-h-64 space-y-1 overflow-y-auto">
                            <UButton
                              v-for="job in rackOptions"
                              :key="job.value"
                              :label="job.label"
                              color="neutral"
                              variant="ghost"
                              class="w-full justify-start text-left"
                              @click="selectRackFromPicker(job.value)"
                            />
                            <p
                              v-if="!rackOptions.length"
                              class="px-2 py-3 text-sm text-slate-500"
                            >
                              ไม่มี Rack ที่ทำต่อได้ใน phase นี้
                            </p>
                          </div>
                        </div>
                      </template>
                    </UPopover>
                  </div>
                </UFormField>

                <UFormField
                  label="Emp ID"
                  name="employee-id"
                  required
                  :ui="qcFieldUi"
                >
                  <div id="qc-employee-input">
                    <USelectMenu
                      v-model="operatorId"
                      :items="employeeSelectOptions"
                      value-key="value"
                      label-key="label"
                      placeholder="เลือก Emp ID"
                      :disabled="jobDetailsLocked"
                      color="neutral"
                      variant="outline"
                      size="md"
                      class="w-full"
                      :search-input="true"
                      :reset-search-term-on-blur="true"
                      :reset-search-term-on-select="true"
                      :ui="qcSelectUi"
                      @update:model-value="handleEmployeeSelection"
                    >
                      <template #default>
                        <span
                          v-if="selectedEmployeeOption"
                          class="block truncate"
                        >
                          <span class="font-black text-slate-950">{{
                            selectedEmployeeOption.code
                          }}</span>
                          <span class="ml-2 text-slate-500">{{
                            selectedEmployeeOption.name
                          }}</span>
                        </span>
                        <span v-else class="text-slate-400">เลือก Emp ID</span>
                      </template>

                      <template #item-label="{ item }">
                        <span class="font-black text-slate-950">{{
                          item.code
                        }}</span>
                        <span class="ml-2 text-slate-500">{{ item.name }}</span>
                      </template>
                    </USelectMenu>
                  </div>
                </UFormField>

                <UFormField
                  label="Employee Name"
                  name="employee-name"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="selectedEmployee?.name || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField
                  label="Charge Channel"
                  name="charge-channel"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="currentJob?.chargeChannelName || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>

                <UFormField
                  label="Charge Program"
                  name="charge-program"
                  :ui="qcFieldUi"
                >
                  <UInput
                    :model-value="currentJob?.chargeProgramName || '-'"
                    readonly
                    placeholder="-"
                    color="neutral"
                    variant="ghost"
                    size="md"
                    class="w-full"
                    :ui="qcReadonlyUi"
                  />
                </UFormField>
              </div>

              <div
                v-if="jobDetailsLocked"
                class="mt-3 rounded-[14px] border border-amber-300 bg-amber-50 px-4 py-3 text-sm font-semibold text-amber-950"
              >
                Job Details ถูกล็อกแล้ว เพราะ phase นี้ถูก Confirm แล้ว
              </div>
            </template>
          </UCard>
        </div>

        <div class="grid gap-3">
          <!-- Native hidden inputs are kept here intentionally for HID scanner focus/capture behavior. -->
          <input
            ref="keyboardBatteryScan"
            v-model="batteryScanInput"
            type="text"
            inputmode="text"
            autocomplete="off"
            autocapitalize="characters"
            autocorrect="off"
            spellcheck="false"
            lang="en"
            class="pointer-events-none fixed left-0 top-0 h-px w-px opacity-0"
            @keyup.enter="handleBatteryInput(batteryScanInput)"
          />
          <input
            ref="keyboardVoltageScan"
            v-model="voltageScanInput"
            type="text"
            inputmode="decimal"
            autocomplete="off"
            autocapitalize="off"
            autocorrect="off"
            spellcheck="false"
            lang="en"
            class="pointer-events-none fixed left-0 top-0 h-px w-px opacity-0"
            @keyup.enter="handleVoltageReaderInput(voltageScanInput)"
          />

          <UCard
            v-if="showRackLayoutCard"
            :ui="{
              root: 'rounded-[22px] border-0 bg-white/92 shadow-[0_18px_48px_rgba(15,23,42,0.08)]',
              body: 'p-4',
            }"
          >
            <div class="flex flex-wrap items-center justify-between gap-3">
              <div>
                <div
                  class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500"
                >
                  Rack Layout
                </div>
                <div class="mt-1 text-2xl font-black text-slate-950">
                  {{ currentJob?.rackId || "ยังไม่มี Rack" }}
                </div>
              </div>
              <div class="flex flex-wrap items-center justify-end gap-3">
                <div
                  class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-3"
                >
                  <div class="flex flex-wrap items-center gap-4">
                    <div class="text-sm font-black text-slate-950">Mode</div>
                    <UButton
                      color="neutral"
                      :variant="
                        workflowActionMode === 'battery' ? 'solid' : 'soft'
                      "
                      :disabled="!canStartBatteryWorkflow"
                      class="min-w-[180px] justify-center rounded-full px-4 py-3 text-center text-sm font-black"
                      :class="
                        !canStartBatteryWorkflow
                          ? 'border border-slate-300 bg-slate-100 text-slate-400'
                          : workflowActionMode === 'battery'
                            ? workflowActionActiveClass
                            : workflowActionIdleClass
                      "
                      @click="startBatteryIdScanWorkflow"
                    >
                      Battery ID
                    </UButton>
                    <UButton
                      color="neutral"
                      :variant="
                        workflowActionMode === 'voltage' ? 'solid' : 'soft'
                      "
                      :disabled="!canStartVoltageWorkflow"
                      class="min-w-[180px] justify-center rounded-full px-4 py-3 text-center text-sm font-black"
                      :class="
                        !canStartVoltageWorkflow
                          ? 'border border-slate-300 bg-slate-100 text-slate-400'
                          : workflowActionMode === 'voltage'
                            ? workflowActionActiveClass
                            : workflowActionIdleClass
                      "
                      @click="startVoltageWorkflow"
                    >
                      Voltage
                    </UButton>
                    <label
                      class="flex cursor-pointer items-center gap-2 whitespace-nowrap text-xs font-black text-slate-700"
                    >
                      <UCheckbox v-model="checkDuplicateBatteryId" />
                      Check duplicate ID
                    </label>
                  </div>
                </div>

                <div
                  class="rounded-[14px] border border-slate-200 bg-white px-4 py-3 shadow-sm"
                >
                  <div
                    class="text-xs font-bold uppercase tracking-[0.16em] text-slate-500"
                  >
                    Voltage {{ progressCount }} / 21
                  </div>
                  <UButton
                    color="neutral"
                    variant="solid"
                    :loading="isConfirming"
                    :disabled="!canConfirmPhase"
                    class="mt-2 min-w-[230px] justify-center rounded-full px-4 py-3 text-sm font-black disabled:cursor-not-allowed"
                    :class="
                      canConfirmPhase
                        ? phase === 'BEFORE_CHARGE'
                          ? 'bg-emerald-700 text-white hover:bg-emerald-800 active:bg-emerald-900'
                          : phase === 'AFTER_CHARGE'
                            ? 'bg-sky-700 text-white hover:bg-sky-800 active:bg-sky-900'
                            : 'bg-amber-600 text-white hover:bg-amber-700 active:bg-amber-900'
                        : 'bg-slate-200 text-slate-500'
                    "
                    @click="confirmCurrentPhase"
                  >
                    Confirm {{ currentPhaseMeta?.label ?? "QC" }}
                  </UButton>
                </div>
              </div>
            </div>

            <div v-if="hasJob" class="mt-4 space-y-3">
              <div
                v-for="(row, rowIndex) in slotRows"
                :key="rowIndex"
                class="grid grid-cols-7 gap-2"
              >
                <UButton
                  v-for="slot in row"
                  :key="slot.slotNumber"
                  :id="`qc-slot-${slot.slotNumber}`"
                  color="neutral"
                  variant="ghost"
                  class="block w-full rounded-[10px] p-0 text-left transition-all duration-150 hover:-translate-y-0.5 hover:bg-transparent"
                  @click="selectSlot(slot.slotNumber)"
                >
                  <BatteryQcSlotCard
                    size="overview"
                    :slot-id="slot.slotNumber"
                    :batt-id="slot.batteryId || '-'"
                    :volt-b="formatSlotCardVoltage(slot.beforeVoltage)"
                    :volt-a="formatSlotCardVoltage(slot.afterVoltage)"
                    :volt-d="formatSlotCardVoltage(slot.deliveryVoltage)"
                    header-icon="ph:car-battery"
                    :title-class="getSlotCardUi(slot).titleClass"
                    :body-class="getSlotCardUi(slot).bodyClass"
                    :border-class="getSlotCardUi(slot).borderClass"
                    :status-label="getSlotCardUi(slot).statusLabel"
                    :status-class="getSlotCardUi(slot).statusClass"
                    :active-metric="getSlotActiveMetric(phase)"
                    :active-metric-class="getSlotCardUi(slot).activeMetricClass"
                  />
                </UButton>
              </div>
            </div>
            <div
              v-else
              class="mt-4 rounded-[22px] border border-dashed border-slate-300 bg-slate-50 px-6 py-12 text-center"
            >
              <div class="text-lg font-black text-slate-900">
                ยังไม่มีข้อมูล rack
              </div>
              <div class="mt-2 text-sm text-slate-600">
                เปิดใบงานใหม่หรือโหลด rack เดิมก่อน แล้วระบบจะแสดงตำแหน่ง
                battery ทั้ง 21 slot ที่นี่
              </div>
            </div>
          </UCard>

          <UCard
            v-else
            :ui="{
              root: 'rounded-[22px] border-0 bg-white/92 shadow-[0_18px_48px_rgba(15,23,42,0.08)]',
              body: 'p-6',
            }"
          >
            <div
              class="rounded-[22px] border border-dashed border-slate-300 bg-slate-50 px-6 py-12 text-center"
            >
              <div class="text-lg font-black text-slate-900">
                Rack Layout จะเปิดหลังบันทึก Job Details
              </div>
              <div class="mt-2 text-sm text-slate-600">
                กรอกข้อมูลที่จำเป็นให้ครบแล้วกด Save เพื่อแสดง 21 slot ของ rack
                นี้
              </div>
            </div>
          </UCard>

          <UAlert
            v-if="loadError"
            class="mt-3"
            color="error"
            variant="soft"
            title="Load Error"
            :description="loadError"
          />
        </div>
      </template>

      <template v-else>
        <UCard
          :ui="{
            root: 'rounded-[24px] border-0 bg-[#2f2f32] shadow-[0_24px_80px_rgba(15,23,42,0.16)] ring-1 ring-slate-950/10',
            body: 'px-5 py-3',
          }"
        >
          <div
            class="grid items-center gap-3 lg:grid-cols-[minmax(0,1fr)_auto_minmax(0,1fr)]"
          >
            <div>
              <div class="text-[11px] text-white/80 sm:text-xs">
                MF Auto Workspace
              </div>
              <h1
                class="mt-1.5 text-[1.9rem] font-black leading-none tracking-tight text-white sm:text-[2.35rem]"
              >
                Battery QC System
              </h1>
            </div>

            <div class="flex justify-center lg:justify-self-center">
              <img
                src="/branding/logo-puma-battery.png"
                alt="PUMA Battery"
                class="h-12 w-auto object-contain sm:h-16"
              />
            </div>

            <div />
          </div>
        </UCard>

        <div class="flex min-h-[52vh] items-center justify-center">
          <div class="grid w-full max-w-[1180px] gap-5 lg:grid-cols-3">
            <BatteryQcModeCard
              v-for="option in phaseOptions"
              :key="option.value"
              :label="option.label"
              :detail="option.detail"
              :icon="option.icon"
              :theme="option.theme"
              :queue-count="getPhaseQueueCount(option.value)"
              @select="selectPhase(option.value)"
            />
          </div>
        </div>
      </template>

      <UModal
        v-model:open="calibrateModalOpen"
        :dismissible="true"
        :close="false"
        :content="{
          class:
            'w-[min(68vw,920px)] max-w-none rounded-[24px] bg-white shadow-[0_24px_80px_rgba(15,23,42,0.28)]',
        }"
        :ui="{
          body: 'max-h-[88vh] overflow-y-auto p-6',
          overlay: 'bg-slate-950/35 backdrop-blur-[2px]',
        }"
      >
        <template #body>
          <div class="space-y-5">
            <div class="flex items-start justify-between gap-3">
              <div>
                <div
                  class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500"
                >
                  ESP32 Calibration
                </div>
                <div class="mt-1 text-3xl font-black text-slate-950">
                  INA226 Gain / Offset
                </div>
                <div class="mt-2 text-sm text-slate-600">
                  เทียบค่าแรงดัน 2 จุด ระหว่างมิเตอร์จริงกับ ESP Volt Meter
                  แล้วส่งค่ากลับไปเก็บที่ ESP32
                </div>
              </div>
              <UButton
                color="neutral"
                variant="ghost"
                icon="i-lucide-x"
                class="rounded-full"
                @click="calibrateModalOpen = false"
              />
            </div>

            <div
              class="grid gap-4 lg:grid-cols-[minmax(0,1.2fr)_minmax(0,1fr)]"
            >
              <UCard
                :ui="{
                  root: 'rounded-[18px] ring-0 bg-slate-50 shadow-[0_8px_20px_rgba(15,23,42,0.06)]',
                  body: 'p-4',
                }"
              >
                <div class="flex items-start justify-between gap-3">
                  <div>
                    <div
                      class="text-xs font-bold uppercase tracking-[0.2em] text-slate-500"
                    >
                      Device Sync
                    </div>
                    <div
                      class="mt-2 text-sm font-semibold"
                      :class="
                        bleConnected ? 'text-emerald-700' : 'text-rose-600'
                      "
                    >
                      {{ calibrationBleStatusLabel }}
                    </div>
                    <div
                      v-if="!bleConnected"
                      class="mt-1 text-xs text-slate-500"
                    >
                      การ pair แบบคีย์บอร์ดหรือ HID ของระบบ ยังใช้คาลิเบรตไม่ได้
                      ต้องเชื่อมต่อ BLE จากหน้านี้อีกครั้ง
                    </div>
                    <div class="mt-2 text-sm font-semibold text-slate-600">
                      {{ currentEspCalibrationLabel }}
                    </div>
                    <div class="mt-1 text-sm font-semibold text-slate-600">
                      {{ savedDbCalibrationLabel }}
                    </div>
                    <div class="mt-2 text-sm text-slate-500">
                      Device:
                      <span class="font-bold text-slate-900">{{
                        bleMeterDeviceId || bleDeviceName
                      }}</span>
                    </div>
                    <div class="mt-1 text-sm text-slate-500">
                      Firmware:
                      <span class="font-bold text-slate-900">{{
                        bleMeterFirmwareVersion || "-"
                      }}</span>
                    </div>
                  </div>
                  <div class="flex flex-col gap-2">
                    <UButton
                      color="neutral"
                      variant="soft"
                      class="h-11 rounded-full border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50"
                      @click="connectBleVoltMeter"
                    >
                      {{ bleConnected ? "Disconnect BLE" : "Connect BLE" }}
                    </UButton>
                    <UButton
                      color="neutral"
                      variant="soft"
                      :loading="calibrateSyncing"
                      :disabled="!bleConnected"
                      class="h-11 rounded-full border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50"
                      @click="syncCalibrationFromBle"
                    >
                      Read BLE
                    </UButton>
                    <UButton
                      color="neutral"
                      variant="soft"
                      class="h-11 rounded-full border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50"
                      @click="loadCalibrationFromDb"
                    >
                      Load latest DB
                    </UButton>
                    <UButton
                      color="neutral"
                      variant="soft"
                      :loading="calibrateResyncing"
                      :disabled="!canResyncCalibrationToBle"
                      class="h-11 rounded-full border border-slate-300 bg-white px-4 text-sm font-black text-slate-800 hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-50"
                      @click="syncSavedCalibrationToBle"
                    >
                      Re-sync to device
                    </UButton>
                  </div>
                </div>
              </UCard>

              <UCard
                :ui="{
                  root: 'rounded-[18px] ring-0 bg-slate-950 text-white shadow-[0_14px_32px_rgba(15,23,42,0.18)]',
                  body: 'p-4',
                }"
              >
                <div
                  class="text-xs font-bold uppercase tracking-[0.2em] text-emerald-200"
                >
                  Calculated Result
                </div>
                <div class="mt-3 text-lg font-black">
                  {{ calibrateResultLabel }}
                </div>
                <div class="mt-3 text-sm text-white/75">
                  {{
                    calibratePreviewMessage ||
                    "พร้อมคำนวณเมื่อกรอกข้อมูลครบทั้ง 4 ช่อง"
                  }}
                </div>
                <div class="mt-4 grid grid-cols-2 gap-3 text-sm">
                  <div class="rounded-xl bg-white/10 px-3 py-3">
                    <div
                      class="text-xs font-bold uppercase tracking-[0.18em] text-white/60"
                    >
                      Gain
                    </div>
                    <div class="mt-1 text-xl font-black">
                      {{
                        calibrateResultGain !== null
                          ? calibrateResultGain.toFixed(5)
                          : "-"
                      }}
                    </div>
                  </div>
                  <div class="rounded-xl bg-white/10 px-3 py-3">
                    <div
                      class="text-xs font-bold uppercase tracking-[0.18em] text-white/60"
                    >
                      Offset
                    </div>
                    <div class="mt-1 text-xl font-black">
                      {{
                        calibrateResultOffset !== null
                          ? `${calibrateResultOffset >= 0 ? "+" : ""}${calibrateResultOffset.toFixed(5)}`
                          : "-"
                      }}
                    </div>
                  </div>
                </div>
              </UCard>
            </div>

            <div class="grid gap-4 lg:grid-cols-2">
              <UCard
                :ui="{
                  root: 'rounded-[18px] ring-0 bg-white shadow-[0_10px_28px_rgba(15,23,42,0.06)]',
                  body: 'p-4',
                }"
              >
                <div class="flex items-center justify-between gap-2">
                  <div class="text-base font-black text-slate-950">Point 1</div>
                  <UButton
                    color="neutral"
                    variant="soft"
                    class="rounded-full border border-slate-300 bg-slate-50 px-3 py-2 text-xs font-black text-slate-800 hover:bg-slate-100"
                    @click="fillCalibrationEspInput(1)"
                  >
                    Use Last ESP Read
                  </UButton>
                </div>
                <div class="mt-4 grid gap-4 md:grid-cols-2">
                  <UFormField
                    label="Meter Voltage"
                    name="cal-point-one-meter"
                    :ui="qcFieldUi"
                  >
                    <UInput
                      v-model="calibratePointOneMeter"
                      type="number"
                      step="0.001"
                      placeholder="5.220"
                      color="neutral"
                      variant="outline"
                      class="w-full"
                      :ui="qcInputUi"
                    />
                  </UFormField>
                  <UFormField
                    label="ESP Volt Meter"
                    name="cal-point-one-esp"
                    :ui="qcFieldUi"
                  >
                    <UInput
                      v-model="calibratePointOneEsp"
                      type="number"
                      step="0.001"
                      placeholder="5.303"
                      color="neutral"
                      variant="outline"
                      class="w-full"
                      :ui="qcInputUi"
                    />
                  </UFormField>
                </div>
              </UCard>

              <UCard
                :ui="{
                  root: 'rounded-[18px] ring-0 bg-white shadow-[0_10px_28px_rgba(15,23,42,0.06)]',
                  body: 'p-4',
                }"
              >
                <div class="flex items-center justify-between gap-2">
                  <div class="text-base font-black text-slate-950">Point 2</div>
                  <UButton
                    color="neutral"
                    variant="soft"
                    class="rounded-full border border-slate-300 bg-slate-50 px-3 py-2 text-xs font-black text-slate-800 hover:bg-slate-100"
                    @click="fillCalibrationEspInput(2)"
                  >
                    Use Last ESP Read
                  </UButton>
                </div>
                <div class="mt-4 grid gap-4 md:grid-cols-2">
                  <UFormField
                    label="Meter Voltage"
                    name="cal-point-two-meter"
                    :ui="qcFieldUi"
                  >
                    <UInput
                      v-model="calibratePointTwoMeter"
                      type="number"
                      step="0.001"
                      placeholder="8.530"
                      color="neutral"
                      variant="outline"
                      class="w-full"
                      :ui="qcInputUi"
                    />
                  </UFormField>
                  <UFormField
                    label="ESP Volt Meter"
                    name="cal-point-two-esp"
                    :ui="qcFieldUi"
                  >
                    <UInput
                      v-model="calibratePointTwoEsp"
                      type="number"
                      step="0.001"
                      placeholder="8.669"
                      color="neutral"
                      variant="outline"
                      class="w-full"
                      :ui="qcInputUi"
                    />
                  </UFormField>
                </div>
              </UCard>
            </div>

            <div
              class="flex items-center justify-between gap-3 rounded-[18px] border border-slate-200 bg-slate-50 px-4 py-3"
            >
              <div class="text-sm text-slate-600">
                ถ้ามี calibration ล่าสุดในฐานข้อมูลอยู่แล้ว
                สามารถโหลดกลับมาใส่ฟอร์ม แล้วกด sync เข้าอุปกรณ์ได้ทันที
              </div>
              <UButton
                color="neutral"
                variant="soft"
                :disabled="!savedDbCalibration"
                class="rounded-full border border-slate-300 bg-white px-4 py-2.5 text-sm font-black text-slate-800 hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-50"
                @click="applySavedCalibrationToForm"
              >
                Use DB Values
              </UButton>
            </div>

            <div class="flex items-center justify-end gap-3">
              <UButton
                color="neutral"
                variant="soft"
                class="rounded-full border border-slate-300 bg-white px-4 py-2.5 text-sm font-black text-slate-800 hover:bg-slate-50"
                @click="calibrateModalOpen = false"
              >
                Cancel
              </UButton>
              <UButton
                color="neutral"
                variant="solid"
                :loading="calibrateSubmitting"
                class="rounded-full bg-emerald-700 px-4 py-2.5 text-sm font-black text-white hover:bg-emerald-800 active:bg-emerald-900"
                @click="submitCalibrationToDbAndBle"
              >
                {{ calibrationSaveButtonLabel }}
              </UButton>
            </div>
          </div>
        </template>
      </UModal>

      <UModal
        v-model:open="detailModalOpen"
        :dismissible="true"
        :close="false"
        :content="{
          class:
            'w-[min(50vw,740px)] max-w-none rounded-[24px] bg-white shadow-[0_24px_80px_rgba(15,23,42,0.28)]',
        }"
        :ui="{
          body: 'max-h-[88vh] overflow-y-auto p-6',
          overlay: 'bg-slate-950/35 backdrop-blur-[2px]',
        }"
      >
        <template #body>
          <div class="space-y-6">
            <div class="space-y-4">
              <div class="grid grid-cols-[1fr_auto] items-start gap-4">
                <div class="text-center">
                  <div
                    class="text-sm font-bold uppercase tracking-[0.24em] text-slate-500"
                  >
                    Active Slot
                  </div>
                  <div
                    class="mt-2 text-5xl font-black tracking-tight text-slate-950"
                  >
                    Slot {{ editingSlotNumber ?? selectedSlotNumber }}
                  </div>
                </div>
                <div class="flex items-start justify-self-end gap-2">
                  <UButton
                    color="neutral"
                    variant="ghost"
                    icon="i-lucide-x"
                    class="rounded-full"
                    @click="closeSlotDetail"
                  />
                </div>
              </div>
            </div>

            <div class="grid gap-4 xl:grid-cols-2">
              <UFormField
                label="Battery ID"
                name="slot-battery-id"
                :ui="qcFieldUi"
              >
                <UInput
                  id="qc-battery-input"
                  v-model="batteryScanInput"
                  type="text"
                  placeholder="Battery ID / QR scan"
                  color="neutral"
                  variant="outline"
                  class="w-full"
                  :readonly="workflowActionMode !== 'battery'"
                  :ui="{
                    base: `h-14 rounded-[8px] border border-slate-300 px-4 text-base font-semibold uppercase ring-1 ring-inset transition focus:ring-2 focus:ring-inset ${workflowActionMode === 'battery' ? 'bg-white text-slate-950 ring-slate-300 hover:border-slate-400 hover:ring-slate-400 focus:border-slate-500 focus:ring-slate-400' : 'bg-slate-100 text-slate-400 ring-slate-200'}`,
                  }"
                  @keyup.enter="handleBatteryInput(batteryScanInput)"
                />
              </UFormField>

              <UFormField
                label="Voltage (Volt)"
                name="slot-voltage"
                :ui="qcFieldUi"
              >
                <UInput
                  id="qc-voltage-input"
                  v-model="voltageInput"
                  type="number"
                  step="0.001"
                  placeholder="4.470"
                  color="neutral"
                  variant="outline"
                  class="w-full"
                  :readonly="workflowActionMode !== 'voltage'"
                  :ui="{
                    base: `h-14 rounded-[8px] border border-slate-300 px-4 text-base font-semibold ring-1 ring-inset transition focus:ring-2 focus:ring-inset ${workflowActionMode === 'voltage' ? 'bg-white text-slate-950 ring-slate-300 hover:border-slate-400 hover:ring-slate-400 focus:border-slate-500 focus:ring-slate-400' : 'bg-slate-100 text-slate-400 ring-slate-200'}`,
                  }"
                  @keyup.enter="handleVoltageReaderInput(voltageInput)"
                />
              </UFormField>
            </div>
          </div>
        </template>
      </UModal>

      <div
        v-if="measurementPopupOpen"
        :key="measurementPopupKey"
        class="pointer-events-none fixed inset-0 z-[70] flex items-center justify-center bg-slate-950/28 px-6 backdrop-blur-[1px]"
      >
        <div
          class="min-w-[320px] max-w-[min(42vw,680px)] rounded-[18px] bg-slate-950/96 px-8 py-6 text-center text-white shadow-[0_30px_80px_rgba(15,23,42,0.5)] ring-1 ring-white/15"
        >
          <div
            class="text-sm font-bold uppercase tracking-[0.22em] text-emerald-200"
          >
            {{ measurementPopupLabel }}
          </div>
          <div
            v-if="measurementPopupContext"
            class="mt-1 text-lg font-black text-white/80"
          >
            {{ measurementPopupContext }}
          </div>
          <div class="mt-3 text-5xl font-black tracking-tight">
            {{ measurementPopupValue }}
          </div>
          <div
            v-if="measurementPopupUnit"
            class="mt-2 text-xl font-bold text-emerald-100"
          >
            {{ measurementPopupUnit }}
          </div>
        </div>
      </div>
    </section>
  </main>
</template>
