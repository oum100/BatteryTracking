<script setup lang="ts">
interface BatteryJobSlot {
  slotNumber: number
  batteryId: string
  beforeVoltage: number | null
  afterVoltage: number | null
  deliveryVoltage: number | null
}

interface BatteryJobRecord {
  id: string
  jobRef: string
  batchId: string | null
  batchRef: string
  workflowLabel: string
  rackId: string
  salesOrderNumber: string | null
  invoiceNumber: string | null
  shipTo: string | null
  plannedDeliveryDate: string | null
  createdAt: string
  slots: BatteryJobSlot[]
}

const route = useRoute()
const router = useRouter()
const selectedBatchId = ref('')
const selectedJobIds = ref<string[]>([])
const previewJobId = ref('')
const isExportingExcel = ref(false)
const isExportingPdf = ref(false)

const { data, pending, refresh } = await useFetch<{ ok: boolean, jobs: BatteryJobRecord[] }>('/api/battery-jobs', {
  default: () => ({ ok: true, jobs: [] }),
})

const jobs = computed(() => data.value?.jobs ?? [])
const groupOptions = computed(() => {
  const groups = new Map<string, string>()
  for (const job of jobs.value) {
    if (job.batchId) groups.set(job.batchId, job.batchRef)
  }

  return Array.from(groups, ([value, label]) => ({ label: `Group ${label}`, value }))
})
const groupJobs = computed(() => jobs.value
  .filter(job => job.batchId === selectedBatchId.value)
  .slice()
  .sort((left, right) => left.rackId.localeCompare(right.rackId)))
const groupRef = computed(() => groupJobs.value[0]?.batchRef || '-')
const selectedJobs = computed(() => groupJobs.value.filter(job => selectedJobIds.value.includes(job.id)))
const allGroupJobsSelected = computed(() => groupJobs.value.length > 0 && groupJobs.value.every(job => selectedJobIds.value.includes(job.id)))
const previewJob = computed(() => selectedJobs.value.find(job => job.id === previewJobId.value) ?? selectedJobs.value[0] ?? null)
const previewJobOptions = computed(() => selectedJobs.value.map(job => ({
  label: `${job.jobRef} · Rack ${job.rackId || '-'}`,
  value: job.id,
})))

watchEffect(() => {
  const batchFromQuery = typeof route.query.batchId === 'string' ? route.query.batchId : ''
  if (!selectedBatchId.value) {
    selectedBatchId.value = batchFromQuery || groupOptions.value[0]?.value || ''
  }
})

watch(selectedBatchId, async (value) => {
  if (value && route.query.batchId !== value) {
    await router.replace({ query: { ...route.query, batchId: value } })
  }
})

watch(groupJobs, (value) => {
  selectedJobIds.value = value.map(job => job.id)
}, { immediate: true })

watch(selectedJobs, (value) => {
  if (!value.some(job => job.id === previewJobId.value)) {
    previewJobId.value = value[0]?.id ?? ''
  }
}, { immediate: true })

function formatDate(value: string | null) {
  if (!value) return '-'
  return new Intl.DateTimeFormat('th-TH', { dateStyle: 'medium' }).format(new Date(value))
}

function getMeasuredCount(job: BatteryJobRecord, field: 'beforeVoltage' | 'afterVoltage' | 'deliveryVoltage') {
  return job.slots.filter(slot => slot[field] !== null && slot[field] !== undefined).length
}

function getReportFileBaseName(extension: 'pdf' | 'xlsx') {
  const safeGroupRef = groupRef.value.replace(/[^a-zA-Z0-9-_]/g, '-')
  return `battery-qc-group-${safeGroupRef}-${new Date().toISOString().slice(0, 10)}.${extension}`
}

function toggleAllJobs(checked: boolean | 'indeterminate') {
  selectedJobIds.value = checked === true ? groupJobs.value.map(job => job.id) : []
}

function toggleJob(id: string, checked: boolean | 'indeterminate') {
  if (checked === true) {
    selectedJobIds.value = selectedJobIds.value.includes(id) ? selectedJobIds.value : [...selectedJobIds.value, id]
    return
  }

  selectedJobIds.value = selectedJobIds.value.filter(jobId => jobId !== id)
}

async function exportExcel() {
  if (!selectedJobs.value.length || isExportingExcel.value) return

  isExportingExcel.value = true
  try {
    const XLSX = await import('xlsx')
    const workbook = XLSX.utils.book_new()
    const summaryRows = selectedJobs.value.map(job => ({
      'Group ID': groupRef.value,
      'Job ID': job.jobRef,
      Rack: job.rackId || '-',
      'SO #': job.salesOrderNumber || '-',
      'Invoice #': job.invoiceNumber || '-',
      'Ship To': job.shipTo || '-',
      'Ship Date': formatDate(job.plannedDeliveryDate),
      State: job.workflowLabel,
      'Before Charge': `${getMeasuredCount(job, 'beforeVoltage')}/21`,
      'After Charge': `${getMeasuredCount(job, 'afterVoltage')}/21`,
      Delivery: `${getMeasuredCount(job, 'deliveryVoltage')}/21`,
    }))
    XLSX.utils.book_append_sheet(workbook, XLSX.utils.json_to_sheet(summaryRows), 'Group Summary')

    selectedJobs.value.forEach((job, index) => {
      const slotRows = job.slots.map(slot => ({
        Slot: slot.slotNumber,
        'Batt ID': slot.batteryId || '-',
        'Before Charge (V)': slot.beforeVoltage ?? '-',
        'After Charge (V)': slot.afterVoltage ?? '-',
        'Before Delivery (V)': slot.deliveryVoltage ?? '-',
      }))
      XLSX.utils.book_append_sheet(workbook, XLSX.utils.json_to_sheet(slotRows), `Job ${index + 1}`)
    })

    XLSX.writeFile(workbook, getReportFileBaseName('xlsx'))
  }
  finally {
    isExportingExcel.value = false
  }
}

async function exportPdf() {
  if (!selectedJobs.value.length || isExportingPdf.value) return

  isExportingPdf.value = true
  try {
    const { jsPDF } = await import('jspdf')
    const autoTableModule = await import('jspdf-autotable')
    const autoTable = autoTableModule.default
    const doc = new jsPDF({ orientation: 'landscape', unit: 'pt', format: 'a4' })

    selectedJobs.value.forEach((job, index) => {
      if (index > 0) doc.addPage()

      doc.setFontSize(16)
      doc.text('Battery QC Job Report', 36, 34)
      doc.setFontSize(8)
      doc.text(`Group ID: ${groupRef.value}   |   Job ID: ${job.jobRef}`, 36, 50)
      doc.text(`Page ${index + 1} of ${selectedJobs.value.length}`, 806, 34, { align: 'right' })
      doc.setFontSize(9)
      doc.text('RACK #', 806, 46, { align: 'right' })
      doc.setFontSize(20)
      doc.text(job.rackId || '-', 806, 64, { align: 'right' })

      autoTable(doc, {
        startY: 78,
        head: [['SO #', 'Invoice #', 'Ship To', 'Ship Date']],
        body: [[
          job.salesOrderNumber || '-',
          job.invoiceNumber || '-',
          job.shipTo || '-',
          formatDate(job.plannedDeliveryDate),
        ]],
        styles: { fontSize: 7.5, cellPadding: 3 },
        headStyles: { fillColor: [109, 40, 217], fontSize: 7.5 },
        margin: { left: 36, right: 36 },
      })

      autoTable(doc, {
        startY: (doc as any).lastAutoTable.finalY + 10,
        head: [['Slot', 'Batt ID', 'Before Chg (V)', 'After Chg (V)', 'Delivery (V)']],
        body: job.slots.map(slot => [
          String(slot.slotNumber),
          slot.batteryId || '-',
          slot.beforeVoltage?.toFixed(2) ?? '-',
          slot.afterVoltage?.toFixed(2) ?? '-',
          slot.deliveryVoltage?.toFixed(2) ?? '-',
        ]),
        styles: { fontSize: 7.5, cellPadding: 3, minCellHeight: 14 },
        headStyles: { fillColor: [71, 85, 105], fontSize: 7.5, cellPadding: 3 },
        columnStyles: {
          0: { cellWidth: 45 },
          1: { cellWidth: 365 },
          2: { cellWidth: 120 },
          3: { cellWidth: 120 },
          4: { cellWidth: 120 },
        },
        margin: { top: 30, bottom: 28, left: 36, right: 36 },
      })
    })

    doc.save(getReportFileBaseName('pdf'))
  }
  finally {
    isExportingPdf.value = false
  }
}
</script>

<template>
  <main class="min-h-screen bg-[radial-gradient(circle_at_top_right,_rgba(196,181,253,0.46),_transparent_34%),linear-gradient(180deg,_#faf8ff_0%,_#f2effb_100%)] px-4 py-6 sm:px-6 lg:px-8">
    <section class="mx-auto max-w-[1500px] space-y-4">
      <UCard :ui="{ root: 'rounded-[18px] border-0 bg-white/92 shadow-[0_18px_40px_rgba(76,29,149,0.12)] ring-1 ring-violet-950/10', body: 'p-5 sm:p-6' }">
        <div class="flex flex-col gap-4 xl:flex-row xl:items-end xl:justify-between">
          <div>
            <p class="text-xs font-black uppercase tracking-[0.22em] text-violet-700">Group Report</p>
            <h1 class="mt-1 text-3xl font-black tracking-tight text-slate-950 sm:text-4xl">Battery QC Group Summary</h1>
            <p class="mt-1 text-sm font-semibold text-slate-600">สรุปผล QC ของทุก Rack ใน Group เดียวกัน</p>
          </div>
          <div class="flex flex-wrap gap-2">
            <UButton to="/battery-qc-admin" color="neutral" variant="soft" icon="i-lucide-briefcase-business" class="rounded-full border border-rose-200 bg-rose-50 px-4 text-rose-900 hover:bg-rose-100">Admin</UButton>
            <UButton color="neutral" variant="soft" icon="i-lucide-file-spreadsheet" :loading="isExportingExcel" :disabled="!selectedJobs.length" class="rounded-full border border-emerald-200 bg-emerald-50 px-4 text-emerald-900 hover:bg-emerald-100" @click="exportExcel">Export Excel</UButton>
            <UButton color="neutral" variant="soft" icon="i-lucide-file-text" :loading="isExportingPdf" :disabled="!selectedJobs.length" class="rounded-full border border-sky-200 bg-sky-50 px-4 text-sky-900 hover:bg-sky-100" @click="exportPdf">Export PDF</UButton>
            <UButton color="neutral" variant="soft" icon="i-lucide-refresh-cw" class="rounded-full border border-amber-200 bg-amber-50 px-4 text-amber-900 hover:bg-amber-100" @click="refresh">Refresh</UButton>
          </div>
        </div>
      </UCard>

      <div v-if="pending" class="rounded-[18px] border border-violet-100 bg-white/80 px-6 py-10 text-center font-bold text-slate-600">Loading group report...</div>
      <div v-else-if="!groupJobs.length" class="rounded-[18px] border border-dashed border-slate-300 bg-white/80 px-6 py-12 text-center"><div class="text-2xl font-black text-slate-950">ไม่พบข้อมูล Group</div><div class="mt-2 text-slate-600">เลือก Group ID ที่ต้องการออกรายงาน</div></div>

      <UCard v-else :ui="{ root: 'rounded-[18px] border-0 bg-white/94 shadow-[0_16px_36px_rgba(15,23,42,0.08)] ring-1 ring-slate-200', body: 'p-0' }">
        <template #header><div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between"><div><p class="text-xs font-black uppercase tracking-[0.2em] text-violet-700">Group {{ groupRef }}</p><h2 class="mt-1 text-xl font-black text-slate-950">เลือก {{ selectedJobs.length }} จาก {{ groupJobs.length }} QC Jobs</h2></div><div class="flex flex-wrap items-center gap-2"><USelectMenu v-model="selectedBatchId" :items="groupOptions" value-key="value" label-key="label" searchable placeholder="Select group" class="w-full sm:w-64" /><UBadge color="secondary" variant="soft" class="rounded-full px-3 py-1 font-black">SELECT FOR EXPORT</UBadge></div></div></template>
        <div class="overflow-x-auto">
          <table class="min-w-[1050px] w-full text-left text-sm"><thead class="bg-violet-50 text-xs font-black uppercase tracking-[0.12em] text-violet-900"><tr><th class="px-4 py-3"><UCheckbox :model-value="allGroupJobsSelected" aria-label="เลือกทุกใบงาน" @update:model-value="toggleAllJobs" /></th><th class="px-4 py-3">Job ID</th><th class="px-4 py-3">Rack</th><th class="px-4 py-3">SO / Invoice</th><th class="px-4 py-3">Ship</th><th class="px-4 py-3">State</th><th class="px-4 py-3">Progress</th></tr></thead><tbody><tr v-for="job in groupJobs" :key="job.id" class="border-t border-slate-100 hover:bg-violet-50/40"><td class="px-4 py-3"><UCheckbox :model-value="selectedJobIds.includes(job.id)" :aria-label="`เลือกรายงาน ${job.jobRef}`" @update:model-value="value => toggleJob(job.id, value)" /></td><td class="px-4 py-3 font-black text-slate-950">{{ job.jobRef }}</td><td class="px-4 py-3 font-bold text-slate-800">{{ job.rackId || '-' }}</td><td class="px-4 py-3"><div class="font-bold text-slate-900">{{ job.salesOrderNumber || '-' }}</div><div class="text-xs text-slate-500">{{ job.invoiceNumber || 'No Invoice' }}</div></td><td class="px-4 py-3"><div class="font-bold text-slate-800">{{ job.shipTo || '-' }}</div><div class="text-xs text-slate-500">{{ formatDate(job.plannedDeliveryDate) }}</div></td><td class="px-4 py-3"><UBadge color="neutral" variant="soft" class="font-black">{{ job.workflowLabel }}</UBadge></td><td class="px-4 py-3 text-xs font-bold text-slate-700">B {{ getMeasuredCount(job, 'beforeVoltage') }}/21 · A {{ getMeasuredCount(job, 'afterVoltage') }}/21 · D {{ getMeasuredCount(job, 'deliveryVoltage') }}/21</td></tr></tbody></table>
        </div>
      </UCard>

      <UCard v-if="previewJob" :ui="{ root: 'rounded-[18px] border-0 bg-white/94 shadow-[0_16px_36px_rgba(15,23,42,0.08)] ring-1 ring-slate-200', body: 'p-4 sm:p-5' }">
        <div class="flex flex-col gap-3 sm:flex-row sm:items-end sm:justify-between">
          <div><p class="text-xs font-black uppercase tracking-[0.2em] text-sky-700">PDF Preview</p><h2 class="mt-1 text-xl font-black text-slate-950">ตัวอย่าง 1 หน้า ต่อ 1 ใบงาน</h2></div>
          <USelectMenu v-model="previewJobId" :items="previewJobOptions" value-key="value" label-key="label" class="w-full sm:w-72" />
        </div>

        <div class="mt-4 overflow-auto rounded-[14px] border border-slate-300 bg-slate-200 p-3">
          <article class="box-border mx-auto min-h-[792px] w-[1120px] bg-white px-10 py-8 text-slate-950 shadow-[0_10px_28px_rgba(15,23,42,0.18)]">
            <header class="flex items-start justify-between border-b border-slate-200 pb-3">
              <div><div class="text-lg font-black">Battery QC Job Report</div><div class="mt-1 text-xs font-bold text-slate-600">Group ID: {{ groupRef }} | Job ID: {{ previewJob.jobRef }}</div></div>
              <div class="text-right"><div class="text-lg font-black tracking-tight text-slate-950">RACK #</div><div class="mt-1 text-3xl font-black tracking-tight text-violet-800">{{ previewJob.rackId || '-' }}</div></div>
            </header>
            <table class="mt-4 w-full border-collapse text-[11px]"><thead><tr class="bg-violet-700 text-left text-white"><th class="px-3 py-2">SO #</th><th class="px-3 py-2">Invoice #</th><th class="px-3 py-2">Ship To</th><th class="px-3 py-2">Ship Date</th></tr></thead><tbody><tr class="border border-slate-200"><td class="px-3 py-2">{{ previewJob.salesOrderNumber || '-' }}</td><td class="px-3 py-2">{{ previewJob.invoiceNumber || '-' }}</td><td class="px-3 py-2">{{ previewJob.shipTo || '-' }}</td><td class="px-3 py-2">{{ formatDate(previewJob.plannedDeliveryDate) }}</td></tr></tbody></table>
            <table class="mt-4 w-full table-fixed border-collapse text-[11px]"><colgroup><col class="w-[60px]"><col class="w-[530px]"><col class="w-[150px]"><col class="w-[150px]"><col class="w-[150px]"></colgroup><thead><tr class="bg-slate-700 text-left text-white"><th class="px-3 py-2">Slot</th><th class="px-3 py-2">Batt ID</th><th class="whitespace-nowrap px-3 py-2">Before Chg (V)</th><th class="whitespace-nowrap px-3 py-2">After Chg (V)</th><th class="whitespace-nowrap px-3 py-2">Delivery (V)</th></tr></thead><tbody><tr v-for="slot in previewJob.slots" :key="slot.slotNumber" class="border-b border-x border-slate-200"><td class="px-3 py-1.5">{{ slot.slotNumber }}</td><td class="px-3 py-1.5 font-semibold">{{ slot.batteryId || '-' }}</td><td class="px-3 py-1.5">{{ slot.beforeVoltage?.toFixed(2) ?? '-' }}</td><td class="px-3 py-1.5">{{ slot.afterVoltage?.toFixed(2) ?? '-' }}</td><td class="px-3 py-1.5">{{ slot.deliveryVoltage?.toFixed(2) ?? '-' }}</td></tr></tbody></table>
          </article>
        </div>
      </UCard>
    </section>
  </main>
</template>
