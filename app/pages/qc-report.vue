<script setup lang="ts">
type JobPhase = 'BEFORE_CHARGE' | 'AFTER_CHARGE' | 'DELIVERY'

interface BatteryJobSlot {
  id: string
  jobRef: string
  batchId: string | null
  batchRef: string
  slotNumber: number
  batteryId: string
  beforeVoltage: number | null
  beforeVoltageMv: number | null
  beforeMeasuredAt: string | null
  afterVoltage: number | null
  afterVoltageMv: number | null
  afterMeasuredAt: string | null
  deliveryVoltage: number | null
  deliveryVoltageMv: number | null
  deliveryMeasuredAt: string | null
}

interface BatteryJobRecord {
  id: string
  phase: JobPhase
  status: string
  rackId: string
  beforeChargeOperatorName: string | null
  afterChargeOperatorName: string | null
  beforeDeliveryOperatorName: string | null
  salesOrderNumber: string | null
  invoiceNumber: string | null
  chargeChannelName: string | null
  chargeProgramName: string | null
  plannedDeliveryDate: string | null
  shipTo: 'AAT' | 'FTM' | null
  beforeChargeCompletedAt: string | null
  afterChargeCompletedAt: string | null
  deliveryCompletedAt: string | null
  lockedAt: string | null
  createdAt: string
  updatedAt: string
  slots: BatteryJobSlot[]
}

const route = useRoute()
const router = useRouter()
const selectedJobId = ref('')
const isExportingExcel = ref(false)
const isExportingPdf = ref(false)
let pdfLogoDataUrlPromise: Promise<string | null> | null = null

const { data, pending, refresh } = await useFetch<{ ok: boolean, jobs: BatteryJobRecord[] }>('/api/battery-jobs', {
  default: () => ({ ok: true, jobs: [] }),
})

const jobs = computed(() => data.value?.jobs ?? [])

const selectedJob = computed(() => {
  if (!selectedJobId.value) {
    return jobs.value[0] ?? null
  }

  return jobs.value.find(job => job.id === selectedJobId.value) ?? jobs.value[0] ?? null
})

const jobOptions = computed(() => {
  return jobs.value.map(job => ({
    label: `${job.jobRef} · Rack ${job.rackId || '-'}`,
    value: job.id,
  }))
})

const reportDisplayId = computed(() => {
  const jobRef = selectedJob.value?.jobRef?.trim()
  const salesOrderNumber = selectedJob.value?.salesOrderNumber?.trim()

  if (jobRef && salesOrderNumber) {
    return `${jobRef}-${salesOrderNumber}`
  }

  return jobRef || salesOrderNumber || '-'
})

watchEffect(() => {
  const jobFromQuery = typeof route.query.jobId === 'string' ? route.query.jobId : ''
  const rackFromQuery = typeof route.query.rackId === 'string' ? route.query.rackId : ''

  if (!selectedJobId.value) {
    selectedJobId.value = jobFromQuery || jobs.value.find(job => job.rackId === rackFromQuery)?.id || jobs.value[0]?.id || ''
  }
})

watch(selectedJobId, async (value) => {
  if (!value) {
    return
  }

  if (route.query.jobId === value) {
    return
  }

  await router.replace({
    query: {
      ...route.query,
      jobId: value,
    },
  })
})

function getSectionCompletedAt(job: BatteryJobRecord | null, phase: JobPhase) {
  if (!job) {
    return null
  }

  if (phase === 'BEFORE_CHARGE') {
    return job.beforeChargeCompletedAt ?? getLatestMeasuredAt(job.slots, 'beforeMeasuredAt')
  }

  if (phase === 'AFTER_CHARGE') {
    return job.afterChargeCompletedAt ?? getLatestMeasuredAt(job.slots, 'afterMeasuredAt')
  }

  return job.deliveryCompletedAt ?? getLatestMeasuredAt(job.slots, 'deliveryMeasuredAt')
}

function getPhaseOperatorName(job: BatteryJobRecord | null, phase: JobPhase) {
  if (!job) {
    return '-'
  }

  if (phase === 'BEFORE_CHARGE') {
    return job.beforeChargeOperatorName || '-'
  }

  if (phase === 'AFTER_CHARGE') {
    return job.afterChargeOperatorName || '-'
  }

  return job.beforeDeliveryOperatorName || '-'
}

function getLatestMeasuredAt(
  slots: BatteryJobSlot[],
  field: 'beforeMeasuredAt' | 'afterMeasuredAt' | 'deliveryMeasuredAt',
) {
  const timestamps = slots
    .map(slot => slot[field])
    .filter((value): value is string => Boolean(value))
    .sort((left, right) => new Date(right).getTime() - new Date(left).getTime())

  return timestamps[0] ?? null
}

function formatDateTime(value: string | null) {
  if (!value) {
    return '-'
  }

  return new Intl.DateTimeFormat('th-TH', {
    dateStyle: 'medium',
    timeStyle: 'short',
  }).format(new Date(value))
}

function formatDateOnly(value: string | null) {
  if (!value) {
    return '-'
  }

  return new Intl.DateTimeFormat('th-TH', {
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
  }).format(new Date(value))
}

function formatTimeOnly(value: string | null) {
  if (!value) {
    return '-'
  }

  return new Intl.DateTimeFormat('th-TH', {
    hour: '2-digit',
    minute: '2-digit',
  }).format(new Date(value))
}

function formatVoltage(value: number | null) {
  if (value === null || value === undefined) {
    return '-'
  }

  return value.toFixed(2)
}

function getReportFileBaseName(job: BatteryJobRecord | null) {
  if (!job) {
    return `battery-qc-summary-${new Date().toISOString().slice(0, 10)}`
  }

  const safeJobRef = job.jobRef.replace(/[^a-zA-Z0-9-_]/g, '-')
  return `battery-qc-summary-${safeJobRef}-${new Date().toISOString().slice(0, 10)}`
}

async function getPdfLogoDataUrl() {
  if (pdfLogoDataUrlPromise) {
    return pdfLogoDataUrlPromise
  }

  pdfLogoDataUrlPromise = (async () => {
    try {
      const response = await fetch('/branding/logo-puma-battery.png')

      if (!response.ok) {
        return null
      }

      const blob = await response.blob()

      return await new Promise<string>((resolve, reject) => {
        const reader = new FileReader()
        reader.onload = () => resolve(String(reader.result))
        reader.onerror = () => reject(reader.error)
        reader.readAsDataURL(blob)
      })
    }
    catch {
      return null
    }
  })()

  return pdfLogoDataUrlPromise
}

async function exportExcel() {
  if (!selectedJob.value || isExportingExcel.value) {
    return
  }

  isExportingExcel.value = true

  try {
    const XLSX = await import('xlsx')
    const job = selectedJob.value

    const summaryRows = [
      { Field: 'Rack #', Value: job.rackId || '-' },
      { Field: 'SO #', Value: job.salesOrderNumber || '-' },
      { Field: 'Invoice #', Value: job.invoiceNumber || '-' },
      { Field: 'Delivery Date', Value: formatDateOnly(job.plannedDeliveryDate) },
      { Field: 'Channel', Value: job.chargeChannelName || '-' },
      { Field: 'Program Charge', Value: job.chargeProgramName || '-' },
      { Field: 'Ship to', Value: job.shipTo || '-' },
      { Field: 'QC Before Charge DateTime', Value: formatDateTime(getSectionCompletedAt(job, 'BEFORE_CHARGE')) },
      { Field: 'QC Before Charge Operator', Value: getPhaseOperatorName(job, 'BEFORE_CHARGE') },
      { Field: 'QC After Charge DateTime', Value: formatDateTime(getSectionCompletedAt(job, 'AFTER_CHARGE')) },
      { Field: 'QC After Charge Operator', Value: getPhaseOperatorName(job, 'AFTER_CHARGE') },
      { Field: 'QC Before Delivery DateTime', Value: formatDateTime(getSectionCompletedAt(job, 'DELIVERY')) },
      { Field: 'QC Before Delivery Operator', Value: getPhaseOperatorName(job, 'DELIVERY') },
    ]

    const slotRows = job.slots.map(slot => ({
      'Slot #': slot.slotNumber,
      'Batt ID': slot.batteryId || '-',
      'QC Before Charge': formatVoltage(slot.beforeVoltage),
      'QC After Charge': formatVoltage(slot.afterVoltage),
      'QC Before Delivery': formatVoltage(slot.deliveryVoltage),
    }))

    const workbook = XLSX.utils.book_new()
    const summarySheet = XLSX.utils.json_to_sheet(summaryRows)
    const slotsSheet = XLSX.utils.json_to_sheet(slotRows)

    XLSX.utils.book_append_sheet(workbook, summarySheet, 'Summary')
    XLSX.utils.book_append_sheet(workbook, slotsSheet, 'Slots')
    XLSX.writeFile(workbook, `${getReportFileBaseName(job)}.xlsx`)
  }
  finally {
    isExportingExcel.value = false
  }
}

async function exportPdf() {
  if (!selectedJob.value || isExportingPdf.value) {
    return
  }

  isExportingPdf.value = true

  try {
    const { jsPDF } = await import('jspdf')
    const autoTableModule = await import('jspdf-autotable')
    const autoTable = autoTableModule.default
    const job = selectedJob.value
    const logoDataUrl = await getPdfLogoDataUrl()

    const doc = new jsPDF({
      orientation: 'landscape',
      unit: 'pt',
      format: 'a4',
    })

    if (logoDataUrl) {
      doc.addImage(logoDataUrl, 'PNG', 40, 20, 110, 44)
    }

    doc.setFontSize(14)
    doc.text(`Battery QC Summary of Rack ${job.rackId || '-'}`, 420, 36, { align: 'center' })
    doc.setFontSize(10)
    doc.text(`Date Time ${new Date().toLocaleString('th-TH')}`, 802, 54, { align: 'right' })

    const beforeCompletedAt = getSectionCompletedAt(job, 'BEFORE_CHARGE')
    const afterCompletedAt = getSectionCompletedAt(job, 'AFTER_CHARGE')
    const deliveryCompletedAt = getSectionCompletedAt(job, 'DELIVERY')

    const beforeHeader = [
      'QC Before Charge',
      `DateTime: ${formatDateOnly(beforeCompletedAt)} ${formatTimeOnly(beforeCompletedAt)}`,
      `Operator: ${getPhaseOperatorName(job, 'BEFORE_CHARGE')}`,
    ].join('\n')

    const afterHeader = [
      'QC After Charge',
      `DateTime: ${formatDateOnly(afterCompletedAt)} ${formatTimeOnly(afterCompletedAt)}`,
      `Operator: ${getPhaseOperatorName(job, 'AFTER_CHARGE')}`,
    ].join('\n')

    const deliveryHeader = [
      'QC Before Delivery',
      `DateTime: ${formatDateOnly(deliveryCompletedAt)} ${formatTimeOnly(deliveryCompletedAt)}`,
      `Operator: ${getPhaseOperatorName(job, 'DELIVERY')}`,
    ].join('\n')

    autoTable(doc, {
      startY: 82,
      head: [[
        'Rack #',
        'SO #',
        'Invoice #',
        'Delivery Date',
        'Channel',
        'Program Charge',
        'Ship to',
      ]],
      body: [[
        job.rackId || '-',
        job.salesOrderNumber || '-',
        job.invoiceNumber || '-',
        formatDateOnly(job.plannedDeliveryDate),
        job.chargeChannelName || '-',
        job.chargeProgramName || '-',
        job.shipTo || '-',
      ]],
      styles: {
        fontSize: 9,
        cellPadding: 5,
      },
      headStyles: {
        fillColor: [59, 130, 246],
      },
      margin: { left: 40, right: 40 },
    })

    autoTable(doc, {
      startY: (doc as any).lastAutoTable.finalY + 14,
      head: [[
        'Slot #',
        'Batt ID',
        beforeHeader,
        afterHeader,
        deliveryHeader,
      ]],
      body: job.slots.map(slot => ([
        String(slot.slotNumber),
        slot.batteryId || '-',
        formatVoltage(slot.beforeVoltage),
        formatVoltage(slot.afterVoltage),
        formatVoltage(slot.deliveryVoltage),
      ])),
      styles: {
        fontSize: 8,
        cellPadding: 4,
        valign: 'middle',
      },
      headStyles: {
        fillColor: [71, 85, 105],
        fontSize: 7,
        halign: 'left',
        valign: 'middle',
        cellPadding: 6,
      },
      margin: { left: 40, right: 40 },
    })

    doc.save(`${getReportFileBaseName(job)}.pdf`)
  }
  finally {
    isExportingPdf.value = false
  }
}

function refreshReports() {
  return refresh()
}
</script>

<template>
  <main class="min-h-screen bg-[radial-gradient(circle_at_top,_rgba(217,249,157,0.35),_transparent_34%),linear-gradient(180deg,_#f7faef_0%,_#eef4e1_100%)] px-4 py-6 [font-family:'IBM_Plex_Sans_Thai','Inter',ui-sans-serif,sans-serif] sm:px-6 lg:px-8">
    <section class="mx-auto max-w-[1800px] space-y-4">
      <UCard
        :ui="{
          root: 'rounded-[18px] border-0 bg-white/92 shadow-[0_18px_40px_rgba(70,95,32,0.10)] ring-1 ring-lime-950/8',
          body: 'p-5 sm:p-6'
        }"
      >
        <div class="flex flex-col gap-4 xl:flex-row xl:items-end xl:justify-between">
          <div>
            <h1 class="text-3xl font-black tracking-tight text-slate-950 sm:text-4xl">
              Battery QC Summary
            </h1>
            <p class="mt-1 text-sm font-medium text-slate-600 sm:text-base">
              Report ID: {{ reportDisplayId }}
            </p>
          </div>

          <div class="flex items-end">
            <div class="flex flex-wrap items-stretch justify-start gap-2 sm:justify-end">
              <UButton
                to="/battery-qc-admin"
                size="sm"
                color="neutral"
                variant="soft"
                icon="i-lucide-briefcase-business"
                class="min-h-10 rounded-full border border-rose-200 bg-rose-50 px-4 text-sm font-semibold text-rose-900 shadow-sm hover:bg-rose-100"
                :ui="{ leadingIcon: 'size-4 text-rose-600' }"
              >
                Admin
              </UButton>
              <UButton
                size="sm"
                color="neutral"
                variant="soft"
                icon="i-lucide-file-spreadsheet"
                :loading="isExportingExcel"
                class="min-h-10 rounded-full border border-emerald-200 bg-emerald-50 px-4 text-sm font-semibold text-emerald-900 shadow-sm hover:bg-emerald-100"
                :ui="{ leadingIcon: 'size-4 text-emerald-600' }"
                @click="exportExcel"
              >
                Export Excel
              </UButton>
              <UButton
                size="sm"
                color="neutral"
                variant="soft"
                icon="i-lucide-file-text"
                :loading="isExportingPdf"
                class="min-h-10 rounded-full border border-sky-200 bg-sky-50 px-4 text-sm font-semibold text-sky-900 shadow-sm hover:bg-sky-100"
                :ui="{ leadingIcon: 'size-4 text-sky-600' }"
                @click="exportPdf"
              >
                Export PDF
              </UButton>
              <UButton
                size="sm"
                color="neutral"
                variant="soft"
                icon="i-lucide-refresh-cw"
                class="min-h-10 rounded-full border border-amber-200 bg-amber-50 px-4 text-sm font-semibold text-amber-900 shadow-sm hover:bg-amber-100"
                :ui="{ leadingIcon: 'size-4 text-amber-600' }"
                @click="refreshReports"
              >
                Refresh
              </UButton>
            </div>
          </div>
        </div>
      </UCard>

      <UCard
        v-if="selectedJob"
        :ui="{
          root: 'rounded-[18px] border-0 bg-white/92 shadow-[0_16px_36px_rgba(15,23,42,0.08)] ring-1 ring-slate-200',
          body: 'p-3 sm:p-4'
        }"
      >
            <div class="grid gap-4 md:grid-cols-2 xl:grid-cols-7">
          <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-1">
            <div class="text-[14px] font-normal tracking-[0.08em] text-slate-500">Job #</div>
            <USelectMenu
              v-model="selectedJobId"
              :items="jobOptions"
              value-key="value"
              label-key="label"
              searchable
              color="neutral"
              variant="outline"
              size="lg"
              placeholder="Select job"
              :search-input="{ placeholder: 'Search job...' }"
              class="mt-1 w-full"
              :ui="{
                base: 'h-[40px] w-full rounded-[12px] border-slate-200 bg-white px-3 text-[0.95rem] font-bold text-slate-950',
                content: 'rounded-[12px]',
                viewport: 'max-h-[200px]',
                item: 'text-sm text-slate-900'
              }"
            />
          </div>

          <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-1">
            <div class="text-[14px] font-normal tracking-[0.08em] text-slate-500">SO #</div>
            <div class="mt-1 flex h-[40px] items-center text-[0.95rem] font-bold tracking-tight text-slate-950">{{ selectedJob.salesOrderNumber || '-' }}</div>
          </div>

          <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-1">
            <div class="text-[14px] font-normal tracking-[0.08em] text-slate-500">Invoice #</div>
            <div class="mt-1 flex h-[40px] items-center text-[0.95rem] font-bold tracking-tight text-slate-950">{{ selectedJob.invoiceNumber || '-' }}</div>
          </div>

          <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-1">
            <div class="text-[14px] font-normal tracking-[0.08em] text-slate-500">Delivery Date</div>
            <div class="mt-1 flex h-[40px] items-center text-[0.95rem] font-bold tracking-tight text-slate-950">{{ formatDateOnly(selectedJob.plannedDeliveryDate) }}</div>
          </div>

          <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-1">
            <div class="text-[14px] font-normal tracking-[0.08em] text-slate-500">Channel</div>
            <div class="mt-1 flex h-[40px] items-center text-[0.95rem] font-bold tracking-tight text-slate-950">{{ selectedJob.chargeChannelName || '-' }}</div>
          </div>

          <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-1">
            <div class="text-[14px] font-normal tracking-[0.08em] text-slate-500">Program Charge</div>
            <div class="mt-1 flex h-[40px] items-center text-[0.95rem] font-bold tracking-tight text-slate-950">{{ selectedJob.chargeProgramName || '-' }}</div>
          </div>

          <div class="rounded-[14px] border border-slate-200 bg-slate-50 px-4 py-1">
            <div class="text-[14px] font-normal tracking-[0.08em] text-slate-500">Ship to</div>
            <div class="mt-1 flex h-[40px] items-center text-[0.95rem] font-bold tracking-tight text-slate-950">{{ selectedJob.shipTo || '-' }}</div>
          </div>
        </div>
      </UCard>

      <div v-if="pending" class="rounded-[28px] border border-slate-200 bg-white/85 px-6 py-10 text-center text-lg font-bold text-slate-600">
        Loading QC report...
      </div>

      <div v-else-if="!selectedJob" class="rounded-[28px] border border-dashed border-slate-300 bg-white/80 px-6 py-12 text-center">
        <div class="text-2xl font-black text-slate-950">ยังไม่มีข้อมูล QC report</div>
        <div class="mt-2 text-slate-600">เมื่อมีงาน QC ถูกบันทึกแล้ว รายงานของแต่ละ Rack จะมาแสดงที่หน้านี้</div>
      </div>

      <UCard
        v-else
        :ui="{
          root: 'rounded-[18px] border-0 bg-white/92 shadow-[0_16px_36px_rgba(15,23,42,0.08)] ring-1 ring-slate-200',
          body: 'p-0'
        }"
      >
        <div class="overflow-x-auto">
          <table class="min-w-[1180px] w-full border-separate border-spacing-0 text-[14px] text-slate-950">
            <thead>
              <tr>
                <th class="border-b border-slate-200 bg-slate-100 px-4 py-1 text-left text-xs font-black uppercase tracking-[0.2em] text-slate-500" rowspan="2">
                  Slot #
                </th>
                <th class="border-b border-slate-200 bg-slate-100 px-4 py-1 text-left text-xs font-black uppercase tracking-[0.2em] text-slate-500" rowspan="2">
                  Batt ID
                </th>
                <th class="border-b border-l border-emerald-200 bg-emerald-50 px-4 py-1 text-left" colspan="1">
                  <div class="text-xs font-black uppercase tracking-[0.2em] text-emerald-700">QC Before Charge</div>
                  <div class="mt-2 grid grid-cols-2 gap-3 text-sm">
                    <div>
                      <div class="text-[14px] font-bold uppercase tracking-[0.16em] text-emerald-700/80">วันเวลา</div>
                      <div class="mt-1 font-bold text-slate-900">{{ formatDateOnly(getSectionCompletedAt(selectedJob, 'BEFORE_CHARGE')) }} {{ formatTimeOnly(getSectionCompletedAt(selectedJob, 'BEFORE_CHARGE')) }}</div>
                    </div>
                    <div>
                      <div class="text-[14px] font-bold uppercase tracking-[0.16em] text-emerald-700/80">เจ้าหน้าที่</div>
                      <div class="mt-1 font-bold text-slate-900">{{ getPhaseOperatorName(selectedJob, 'BEFORE_CHARGE') }}</div>
                    </div>
                  </div>
                </th>
                <th class="border-b border-l border-sky-200 bg-sky-50 px-4 py-1 text-left" colspan="1">
                  <div class="text-xs font-black uppercase tracking-[0.2em] text-sky-700">QC After Charge</div>
                  <div class="mt-2 grid grid-cols-2 gap-3 text-sm">
                    <div>
                      <div class="text-[14px] font-bold uppercase tracking-[0.16em] text-sky-700/80">วันเวลา</div>
                      <div class="mt-1 font-bold text-slate-900">{{ formatDateOnly(getSectionCompletedAt(selectedJob, 'AFTER_CHARGE')) }} {{ formatTimeOnly(getSectionCompletedAt(selectedJob, 'AFTER_CHARGE')) }}</div>
                    </div>
                    <div>
                      <div class="text-[14px] font-bold uppercase tracking-[0.16em] text-sky-700/80">เจ้าหน้าที่</div>
                      <div class="mt-1 font-bold text-slate-900">{{ getPhaseOperatorName(selectedJob, 'AFTER_CHARGE') }}</div>
                    </div>
                  </div>
                </th>
                <th class="border-b border-l border-amber-200 bg-amber-50 px-4 py-1 text-left" colspan="1">
                  <div class="text-xs font-black uppercase tracking-[0.2em] text-amber-700">QC Before Delivery</div>
                  <div class="mt-2 grid grid-cols-2 gap-3 text-sm">
                    <div>
                      <div class="text-[14px] font-bold uppercase tracking-[0.16em] text-amber-700/80">วันเวลา</div>
                      <div class="mt-1 font-bold text-slate-900">{{ formatDateOnly(getSectionCompletedAt(selectedJob, 'DELIVERY')) }} {{ formatTimeOnly(getSectionCompletedAt(selectedJob, 'DELIVERY')) }}</div>
                    </div>
                    <div>
                      <div class="text-[14px] font-bold uppercase tracking-[0.16em] text-amber-700/80">เจ้าหน้าที่</div>
                      <div class="mt-1 font-bold text-slate-900">{{ getPhaseOperatorName(selectedJob, 'DELIVERY') }}</div>
                    </div>
                  </div>
                </th>
              </tr>

            </thead>

            <tbody>
              <tr
                v-for="slot in selectedJob.slots"
                :key="slot.id"
                class="odd:bg-white even:bg-slate-50/70"
              >
                <td class="border-b border-slate-100 px-4 py-1 font-bold text-slate-950">
                  {{ slot.slotNumber }}
                </td>
                <td class="border-b border-slate-100 px-4 py-1 font-bold text-slate-950">
                  {{ slot.batteryId || '-' }}
                </td>
                <td class="border-b border-l border-emerald-100 px-4 py-1 text-base font-black text-slate-950">
                  {{ formatVoltage(slot.beforeVoltage) }}
                </td>
                <td class="border-b border-l border-sky-100 px-4 py-1 text-base font-black text-slate-950">
                  {{ formatVoltage(slot.afterVoltage) }}
                </td>
                <td class="border-b border-l border-amber-100 px-4 py-1 text-base font-black text-slate-950">
                  {{ formatVoltage(slot.deliveryVoltage) }}
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </UCard>
    </section>
  </main>
</template>
