<script setup lang="ts">
import { CalendarDate, Time } from '@internationalized/date'

type MovementOperation = 'STOCK_TO_CHARGE' | 'CHARGE_TO_DELIVERY' | 'DELIVERY_TRANSFER'

interface MovementRecord {
  id: string
  operation: MovementOperation
  fromRack: string
  fromSlot: string
  batterySn: string
  voltage: number
  voltageMeasuredAt: string | null
  toRack: string
  toSlot: string
  scanSource: string | null
  voltageSource: string | null
  deviceId: string | null
  notes: string | null
  createdAt: string
  updatedAt: string
}

interface MovementForm {
  id: string | null
  operation: MovementOperation
  fromRack: string
  fromSlot: string
  batterySn: string
  voltage: string
  voltageMeasuredAt: string
  toRack: string
  toSlot: string
  scanSource: string
  voltageSource: string
  deviceId: string
  notes: string
}

const API_ENDPOINT = '/api/battery-movements'
const RACKS_ENDPOINT = '/api/battery-movements/racks'

const search = ref('')
const batteryFilter = ref<string | undefined>()
const fromRackFilter = ref<string | undefined>()
const toRackFilter = ref<string | undefined>()
const selectedOperation = ref<'ALL' | MovementOperation>('ALL')
const startDateValue = ref<CalendarDate>()
const startTimeValue = ref<Time>()
const endDateValue = ref<CalendarDate>()
const endTimeValue = ref<Time>()
const isRefreshing = ref(false)
const isSaving = ref(false)
const isDeleting = ref(false)
const loadError = ref('')
const formError = ref('')
const editorOpen = ref(false)
const currentPage = ref(1)
const pageSize = ref(25)
const pageSizeOptions = [25, 50, 100]
const pageSizeItems = pageSizeOptions.map(value => ({ label: String(value), value }))
const filterSelectUi = {
  base: 'min-h-10 rounded-full border border-slate-300 bg-white text-sm text-slate-950 hover:bg-white focus:bg-white data-[state=open]:bg-white',
  leadingIcon: 'text-slate-500',
  placeholder: 'text-slate-500',
  value: 'text-slate-950',
  trailingIcon: 'text-slate-500',
  content: 'bg-white',
  item: 'text-slate-800 data-highlighted:not-data-disabled:text-slate-950 data-highlighted:not-data-disabled:before:bg-slate-100',
} as const
const pageSizeSelectUi = {
  base: 'min-h-9 rounded-full border border-slate-300 bg-white text-sm text-slate-950 hover:bg-white focus:bg-white data-[state=open]:bg-white',
  value: 'text-slate-950',
  trailingIcon: 'text-slate-500',
  content: 'bg-white',
  item: 'text-slate-800 data-highlighted:not-data-disabled:text-slate-950 data-highlighted:not-data-disabled:before:bg-slate-100',
} as const

const operationOptions = [
  { label: 'All', value: 'ALL' },
  { label: 'Stock to Charge', value: 'STOCK_TO_CHARGE' },
  { label: 'Charge to Delivery', value: 'CHARGE_TO_DELIVERY' },
  { label: 'Delivery Transfer', value: 'DELIVERY_TRANSFER' },
] as const

const operationMeta: Record<MovementOperation, { badge: string }> = {
  STOCK_TO_CHARGE: { badge: 'bg-lime-100 text-lime-950 ring-lime-300/80' },
  CHARGE_TO_DELIVERY: { badge: 'bg-emerald-100 text-emerald-950 ring-emerald-300/80' },
  DELIVERY_TRANSFER: { badge: 'bg-sky-100 text-sky-950 ring-sky-300/80' },
}

const emptyForm = (): MovementForm => ({
  id: null,
  operation: 'STOCK_TO_CHARGE',
  fromRack: '',
  fromSlot: '',
  batterySn: '',
  voltage: '',
  voltageMeasuredAt: '',
  toRack: '',
  toSlot: '',
  scanSource: '',
  voltageSource: '',
  deviceId: '',
  notes: '',
})

const form = reactive<MovementForm>(emptyForm())

const { data, pending, refresh } = await useFetch<{ ok: boolean, records: Array<MovementRecord & { stage?: string }> }>(API_ENDPOINT, {
  default: () => ({ ok: true, records: [] }),
})

function normalizeOperation(value?: string) {
  const normalized = {
    PRE_CHARGE: 'STOCK_TO_CHARGE',
    AFTER_CHARGE: 'CHARGE_TO_DELIVERY',
    DELIVERY: 'DELIVERY_TRANSFER',
  }[String(value ?? '').trim()] ?? String(value ?? '').trim()

  if (!['STOCK_TO_CHARGE', 'CHARGE_TO_DELIVERY', 'DELIVERY_TRANSFER'].includes(normalized)) {
    return 'STOCK_TO_CHARGE' as MovementOperation
  }

  return normalized as MovementOperation
}

const { data: racksData, refresh: refreshRacks } = await useFetch<{ ok: boolean, racks: string[] }>(RACKS_ENDPOINT, {
  default: () => ({ ok: true, racks: [] }),
})

const records = computed<MovementRecord[]>(() => (data.value?.records ?? []).map(record => ({
  ...record,
  operation: normalizeOperation(record.operation ?? record.stage),
})))
const batteryOptions = computed(() => {
  const values = Array.from(new Set(records.value.map(record => record.batterySn).filter(Boolean)))
  return values.map(battery => ({
    label: battery,
    value: battery,
  }))
})
const rackOptions = computed(() => (racksData.value?.racks ?? []).map(rack => ({
  label: rack,
  value: rack,
})))

const startDateTime = computed(() => buildFilterDateTime(startDateValue.value, startTimeValue.value))
const endDateTime = computed(() => buildFilterDateTime(endDateValue.value, endTimeValue.value))
const startDateLabel = computed(() => formatFilterDisplay(startDateValue.value, startTimeValue.value, 'From date & time'))
const endDateLabel = computed(() => formatFilterDisplay(endDateValue.value, endTimeValue.value, 'To date & time'))
const formMeasuredLabel = computed(() => formatFilterDisplay(formMeasuredDate.value, formMeasuredTime.value, 'Date & time'))

const filteredRecords = computed(() => {
  const query = search.value.trim().toLowerCase()
  const batteryQuery = batteryFilter.value?.trim().toLowerCase() ?? ''
  const fromRackQuery = fromRackFilter.value?.trim().toLowerCase() ?? ''
  const toRackQuery = toRackFilter.value?.trim().toLowerCase() ?? ''
  const start = startDateTime.value ? new Date(startDateTime.value).getTime() : null
  const end = endDateTime.value ? new Date(endDateTime.value).getTime() : null

  return records.value.filter((record) => {
    if (selectedOperation.value !== 'ALL' && record.operation !== selectedOperation.value) {
      return false
    }

    if (batteryQuery && !record.batterySn.toLowerCase().includes(batteryQuery)) {
      return false
    }

    if (fromRackQuery && !record.fromRack.toLowerCase().includes(fromRackQuery)) {
      return false
    }

    if (toRackQuery && !record.toRack.toLowerCase().includes(toRackQuery)) {
      return false
    }

    const createdAt = new Date(record.createdAt).getTime()
    if (start && createdAt < start) {
      return false
    }
    if (end && createdAt > end) {
      return false
    }

    if (!query) {
      return true
    }

    const haystack = [
      record.batterySn,
      record.fromRack,
      record.fromSlot,
      record.toRack,
      record.toSlot,
      record.operation,
      record.scanSource ?? '',
      record.voltageSource ?? '',
      record.deviceId ?? '',
      record.notes ?? '',
    ].join(' ').toLowerCase()

    return haystack.includes(query)
  })
})

const totalCount = computed(() => filteredRecords.value.length)
const totalPages = computed(() => Math.max(1, Math.ceil(totalCount.value / pageSize.value)))
const paginatedRecords = computed(() => {
  const start = (currentPage.value - 1) * pageSize.value
  return filteredRecords.value.slice(start, start + pageSize.value)
})
const pageStart = computed(() => totalCount.value === 0 ? 0 : (currentPage.value - 1) * pageSize.value + 1)
const pageEnd = computed(() => Math.min(currentPage.value * pageSize.value, totalCount.value))
const visiblePages = computed(() => {
  const start = Math.max(1, currentPage.value - 2)
  const end = Math.min(totalPages.value, start + 4)
  const adjustedStart = Math.max(1, end - 4)
  return Array.from({ length: end - adjustedStart + 1 }, (_, index) => adjustedStart + index)
})
const formMeasuredDate = computed(() => {
  if (!form.voltageMeasuredAt) {
    return undefined
  }

  const [datePart] = form.voltageMeasuredAt.split('T')
  const [year, month, day] = datePart.split('-').map(Number)

  if (!year || !month || !day) {
    return undefined
  }

  return new CalendarDate(year, month, day)
})
const formMeasuredTime = computed(() => {
  if (!form.voltageMeasuredAt) {
    return undefined
  }

  const [, timePart = ''] = form.voltageMeasuredAt.split('T')
  const [hour, minute] = timePart.split(':').map(Number)

  if (Number.isNaN(hour) || Number.isNaN(minute)) {
    return undefined
  }

  return new Time(hour, minute)
})

watch(filteredRecords, () => {
  currentPage.value = 1
})

watch(pageSize, () => {
  currentPage.value = 1
})

watch(totalPages, (value) => {
  if (currentPage.value > value) {
    currentPage.value = value
  }
})

function formatDateTime(value: string | null) {
  if (!value) {
    return '-'
  }

  return new Intl.DateTimeFormat('th-TH', {
    dateStyle: 'short',
    timeStyle: 'short',
  }).format(new Date(value))
}

function formatDateTimeLocal(value: string | null) {
  if (!value) {
    return ''
  }

  const date = new Date(value)
  const offset = date.getTimezoneOffset()
  const localDate = new Date(date.getTime() - offset * 60000)
  return localDate.toISOString().slice(0, 16)
}

function syncMeasuredDateTime(dateValue?: CalendarDate, timeValue?: Time) {
  if (!dateValue) {
    form.voltageMeasuredAt = ''
    return
  }

  const hours = timeValue?.hour ?? 0
  const minutes = timeValue?.minute ?? 0

  form.voltageMeasuredAt = [
    `${dateValue.year}-${padDateSegment(dateValue.month)}-${padDateSegment(dateValue.day)}`,
    `${padDateSegment(hours)}:${padDateSegment(minutes)}`,
  ].join('T')
}

function setFormMeasuredDate(value?: CalendarDate) {
  syncMeasuredDateTime(value, value ? (formMeasuredTime.value ?? new Time(0, 0)) : undefined)
}

function setFormMeasuredTime(value?: Time) {
  if (!formMeasuredDate.value) {
    return
  }

  syncMeasuredDateTime(formMeasuredDate.value, value)
}

function padDateSegment(value: number) {
  return String(value).padStart(2, '0')
}

function buildFilterDateTime(dateValue?: CalendarDate, timeValue?: Time) {
  if (!dateValue) {
    return ''
  }

  const hours = timeValue?.hour ?? 0
  const minutes = timeValue?.minute ?? 0

  return [
    `${dateValue.year}-${padDateSegment(dateValue.month)}-${padDateSegment(dateValue.day)}`,
    `${padDateSegment(hours)}:${padDateSegment(minutes)}`,
  ].join('T')
}

function formatFilterDisplay(dateValue: CalendarDate | undefined, timeValue: Time | undefined, fallback: string) {
  const localValue = buildFilterDateTime(dateValue, timeValue)

  if (!localValue) {
    return fallback
  }

  return new Intl.DateTimeFormat('en-GB', {
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false,
  }).format(new Date(localValue)).replace(',', '')
}

function setFilterDate(kind: 'start' | 'end', value?: CalendarDate) {
  if (kind === 'start') {
    startDateValue.value = value
    startTimeValue.value = value ? (startTimeValue.value ?? new Time(0, 0)) : undefined
    return
  }

  endDateValue.value = value
  endTimeValue.value = value ? (endTimeValue.value ?? new Time(23, 59)) : undefined
}

function setFilterTime(kind: 'start' | 'end', value?: Time) {
  if (kind === 'start') {
    startTimeValue.value = value
    return
  }

  endTimeValue.value = value
}

function clearDateFilter(kind: 'start' | 'end') {
  if (kind === 'start') {
    startDateValue.value = undefined
    startTimeValue.value = undefined
    return
  }

  endDateValue.value = undefined
  endTimeValue.value = undefined
}

function getOperationLabel(operation: MovementOperation) {
  const labels: Record<MovementOperation, string> = {
    STOCK_TO_CHARGE: 'Stock to Charge',
    CHARGE_TO_DELIVERY: 'Charge to Delivery',
    DELIVERY_TRANSFER: 'Delivery Transfer',
  }

  return labels[operation]
}

function resetForm() {
  Object.assign(form, emptyForm())
  formError.value = ''
}

function openCreate() {
  resetForm()
  editorOpen.value = true
}

function closeEditor() {
  editorOpen.value = false
  formError.value = ''
}

function openEdit(record: MovementRecord) {
  Object.assign(form, {
    id: record.id,
    operation: record.operation,
    fromRack: record.fromRack,
    fromSlot: record.fromSlot,
    batterySn: record.batterySn,
    voltage: String(record.voltage),
    voltageMeasuredAt: formatDateTimeLocal(record.voltageMeasuredAt),
    toRack: record.toRack,
    toSlot: record.toSlot,
    scanSource: record.scanSource ?? '',
    voltageSource: record.voltageSource ?? '',
    deviceId: record.deviceId ?? '',
    notes: record.notes ?? '',
  })
  formError.value = ''
  editorOpen.value = true
}

async function reloadRecords() {
  isRefreshing.value = true
  loadError.value = ''

  try {
    await Promise.all([refresh(), refreshRacks()])
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to load movement history'
  }
  finally {
    isRefreshing.value = false
  }
}

async function saveRecord() {
  formError.value = ''
  isSaving.value = true

  try {
    const payload = {
      operation: form.operation,
      fromRack: form.fromRack,
      fromSlot: form.fromSlot,
      batterySn: form.batterySn,
      voltage: Number.parseFloat(form.voltage),
      voltageMeasuredAt: form.voltageMeasuredAt ? new Date(form.voltageMeasuredAt).toISOString() : null,
      toRack: form.toRack,
      toSlot: form.toSlot,
      scanSource: form.scanSource || null,
      voltageSource: form.voltageSource || null,
      deviceId: form.deviceId || null,
      notes: form.notes || null,
    }

    if (form.id) {
      await $fetch(`${API_ENDPOINT}/${form.id}`, {
        method: 'PATCH',
        body: payload,
      })
    }
    else {
      await $fetch(API_ENDPOINT, {
        method: 'POST',
        body: payload,
      })
    }

    await reloadRecords()
    editorOpen.value = false
    resetForm()
  }
  catch (error) {
    formError.value = error instanceof Error ? error.message : 'Unable to save record'
  }
  finally {
    isSaving.value = false
  }
}

async function deleteRecord(id: string) {
  isDeleting.value = true

  try {
    await $fetch(`${API_ENDPOINT}/${id}`, {
      method: 'DELETE',
    })

    if (form.id === id) {
      editorOpen.value = false
      resetForm()
    }

    await reloadRecords()
  }
  finally {
    isDeleting.value = false
  }
}

async function exportExcel() {
  const XLSX = await import('xlsx')
  const rows = filteredRecords.value.map(record => ({
    Operation: getOperationLabel(record.operation),
    BatterySN: record.batterySn,
    FromRack: record.fromRack,
    FromSlot: record.fromSlot,
    ToRack: record.toRack,
    ToSlot: record.toSlot,
    Voltage: record.voltage,
    VoltageMeasuredAt: record.voltageMeasuredAt ?? '',
    ScanSource: record.scanSource ?? '',
    VoltageSource: record.voltageSource ?? '',
    DeviceId: record.deviceId ?? '',
    Notes: record.notes ?? '',
    CreatedAt: record.createdAt,
  }))

  const workbook = XLSX.utils.book_new()
  const worksheet = XLSX.utils.json_to_sheet(rows)
  XLSX.utils.book_append_sheet(workbook, worksheet, 'Movements')
  XLSX.writeFile(workbook, `battery-movements-${new Date().toISOString().slice(0, 10)}.xlsx`)
}

async function exportPdf() {
  const { jsPDF } = await import('jspdf')
  const autoTableModule = await import('jspdf-autotable')
  const autoTable = autoTableModule.default

  const doc = new jsPDF({
    orientation: 'landscape',
    unit: 'pt',
    format: 'a4',
  })

  doc.setFontSize(18)
  doc.text('Battery Movement Records', 40, 38)
  doc.setFontSize(10)
  doc.text(`Exported ${new Date().toLocaleString('th-TH')}`, 40, 56)

  autoTable(doc, {
    startY: 72,
    head: [[
      'Operation',
      'Battery',
      'From',
      'To',
      'Volt',
      'Scan',
      'Voltage',
      'Created',
    ]],
    body: filteredRecords.value.map(record => ([
      getOperationLabel(record.operation),
      record.batterySn,
      `${record.fromRack}/${record.fromSlot}`,
      `${record.toRack}/${record.toSlot}`,
      record.voltage.toFixed(2),
      record.scanSource ?? '-',
      record.voltageSource ?? '-',
      formatDateTime(record.createdAt),
    ])),
    styles: {
      fontSize: 8,
      cellPadding: 5,
    },
    headStyles: {
      fillColor: [15, 23, 42],
    },
  })

  doc.save(`battery-movements-${new Date().toISOString().slice(0, 10)}.pdf`)
}
</script>

<template>
  <main class="min-h-screen bg-[linear-gradient(180deg,#eff6df_0%,#f9fbf2_100%)] px-6 py-6 text-slate-950">
    <section class="mx-auto flex max-w-7xl flex-col gap-4">
      <UCard
        :ui="{
          root: 'rounded-md ring-0 bg-white/92 shadow-[0_18px_40px_rgba(15,23,42,0.08)]',
          body: 'px-5 py-5',
        }"
      >
        <div class="min-w-0">
          <div class="min-w-0">
            <h1 class="text-4xl leading-none font-black tracking-tight">
              Battery Movement History
            </h1>
            <p class="mt-3 block text-sm text-slate-600">
              ดูข้อมูล, filter, แก้ไข, ลบ และ export รายการ movement จาก Postgres
            </p>
          </div>
        </div>
      </UCard>

      <UCard
        :ui="{
          root: 'rounded-md ring-0 bg-white/90 shadow-[0_14px_32px_rgba(15,23,42,0.06)]',
          body: 'p-4',
        }"
      >
        <div class="flex flex-col gap-3">
          <div class="flex flex-col gap-3 lg:flex-row lg:items-center lg:justify-between">
            <div class="flex flex-wrap items-stretch gap-2 lg:flex-nowrap">
              <UButton
                v-for="option in operationOptions"
                :key="option.value"
                size="sm"
                color="neutral"
                variant="soft"
                :class="[
                  'min-h-11',
                  selectedOperation === option.value ? 'bg-sky-600 text-white hover:bg-sky-700' : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-50'
                ]"
                @click="selectedOperation = option.value"
              >
                {{ option.label }}
              </UButton>
            </div>

            <div class="flex flex-wrap items-stretch gap-2 lg:flex-nowrap lg:justify-end">
              <UButton size="sm" color="neutral" variant="soft" icon="i-lucide-file-spreadsheet" class="min-h-10 border border-slate-300 bg-white text-slate-700 hover:bg-slate-50" @click="exportExcel">
                Export Excel
              </UButton>
              <UButton size="sm" color="neutral" variant="soft" icon="i-lucide-file-text" class="min-h-10 border border-slate-300 bg-white text-slate-700 hover:bg-slate-50" @click="exportPdf">
                Export PDF
              </UButton>
              <UButton size="sm" color="neutral" variant="soft" icon="i-lucide-plus" class="min-h-10 bg-lime-100 text-lime-950 hover:bg-lime-200" @click="openCreate">
                New Record
              </UButton>
              <UButton size="sm" color="neutral" variant="soft" icon="i-lucide-refresh-cw" :loading="isRefreshing" class="min-h-10 border border-slate-300 bg-white text-slate-700 hover:bg-slate-50" @click="reloadRecords">
                Refresh
              </UButton>
            </div>
          </div>

          <div class="flex flex-col gap-3 lg:flex-row lg:flex-nowrap lg:items-end lg:gap-2">
            <UFormField label="Search" name="search" class="lg:min-w-0 lg:flex-1" :ui="{ label: 'text-xs font-semibold text-slate-700' }">
              <UInput
                v-model="search"
                leading-icon="i-lucide-search"
                size="md"
                placeholder="Battery, device, note"
                :ui="{ base: 'min-h-10 bg-white text-sm text-slate-950 placeholder:text-slate-500', leadingIcon: 'text-slate-500' }"
              />
            </UFormField>

            <UFormField label="Battery" name="battery-filter" class="lg:min-w-0 lg:flex-1" :ui="{ label: 'text-xs font-semibold text-slate-700' }">
              <USelectMenu
                v-model="batteryFilter"
                :items="batteryOptions"
                value-key="value"
                label-key="label"
                searchable
                clear
                color="neutral"
                variant="outline"
                leading-icon="i-lucide-battery-full"
                size="lg"
                placeholder="Select battery S/N"
                :search-input="{ placeholder: 'Search battery S/N...' }"
                :ui="filterSelectUi"
              />
            </UFormField>

            <UFormField label="From Rack" name="from-rack-filter" class="lg:min-w-0 lg:flex-1" :ui="{ label: 'text-xs font-semibold text-slate-700' }">
              <USelectMenu
                v-model="fromRackFilter"
                :items="rackOptions"
                value-key="value"
                label-key="label"
                searchable
                clear
                color="neutral"
                variant="outline"
                leading-icon="i-lucide-map-pinned"
                size="lg"
                placeholder="Select from rack"
                :search-input="{ placeholder: 'Search from rack...' }"
                :ui="filterSelectUi"
              />
            </UFormField>

            <UFormField label="To Rack" name="to-rack-filter" class="lg:min-w-0 lg:flex-1" :ui="{ label: 'text-xs font-semibold text-slate-700' }">
              <USelectMenu
                v-model="toRackFilter"
                :items="rackOptions"
                value-key="value"
                label-key="label"
                searchable
                clear
                color="neutral"
                variant="outline"
                leading-icon="i-lucide-map-pinned"
                size="lg"
                placeholder="Select to rack"
                :search-input="{ placeholder: 'Search to rack...' }"
                :ui="filterSelectUi"
              />
            </UFormField>

            <UFormField label="From Date Time" name="from-date-time" class="lg:min-w-0 lg:flex-1" :ui="{ label: 'text-xs font-semibold text-slate-700' }">
              <UPopover :content="{ align: 'start', side: 'bottom', sideOffset: 8 }">
                <UButton
                  color="neutral"
                  variant="outline"
                  class="h-10 w-full justify-between rounded-md border-slate-300 bg-white px-3 text-sm font-medium text-slate-700 hover:bg-slate-50"
                >
                  <span :class="startDateValue ? 'text-slate-950' : 'text-slate-500'">
                    {{ startDateLabel }}
                  </span>
                  <UIcon name="i-lucide-calendar-days" class="size-4 text-slate-500" />
                </UButton>

                <template #content>
                  <div class="w-[320px] space-y-3 rounded-md bg-white p-3 shadow-xl">
                    <UCalendar
                      :model-value="startDateValue"
                      color="neutral"
                      variant="soft"
                      :year-controls="true"
                      @update:model-value="setFilterDate('start', $event as CalendarDate | undefined)"
                    />

                    <UFormField label="Time" :ui="{ label: 'text-sm font-semibold text-slate-700' }">
                      <UInputTime
                        :model-value="startTimeValue"
                        size="lg"
                        :ui="{ base: 'min-h-10 bg-white text-sm text-slate-950' }"
                        @update:model-value="setFilterTime('start', $event as Time | undefined)"
                      />
                    </UFormField>

                    <div class="flex justify-end">
                      <UButton color="neutral" variant="ghost" size="sm" @click="clearDateFilter('start')">
                        Clear
                      </UButton>
                    </div>
                  </div>
                </template>
              </UPopover>
            </UFormField>

            <UFormField label="To Date Time" name="to-date-time" class="lg:min-w-0 lg:flex-1" :ui="{ label: 'text-xs font-semibold text-slate-700' }">
              <UPopover :content="{ align: 'start', side: 'bottom', sideOffset: 8 }">
                <UButton
                  color="neutral"
                  variant="outline"
                  class="h-10 w-full justify-between rounded-md border-slate-300 bg-white px-3 text-sm font-medium text-slate-700 hover:bg-slate-50"
                >
                  <span :class="endDateValue ? 'text-slate-950' : 'text-slate-500'">
                    {{ endDateLabel }}
                  </span>
                  <UIcon name="i-lucide-calendar-days" class="size-4 text-slate-500" />
                </UButton>

                <template #content>
                  <div class="w-[320px] space-y-3 rounded-md bg-white p-3 shadow-xl">
                    <UCalendar
                      :model-value="endDateValue"
                      color="neutral"
                      variant="soft"
                      :year-controls="true"
                      @update:model-value="setFilterDate('end', $event as CalendarDate | undefined)"
                    />

                    <UFormField label="Time" :ui="{ label: 'text-sm font-semibold text-slate-700' }">
                      <UInputTime
                        :model-value="endTimeValue"
                        size="lg"
                        :ui="{ base: 'min-h-10 bg-white text-sm text-slate-950' }"
                        @update:model-value="setFilterTime('end', $event as Time | undefined)"
                      />
                    </UFormField>

                    <div class="flex justify-end">
                      <UButton color="neutral" variant="ghost" size="sm" @click="clearDateFilter('end')">
                        Clear
                      </UButton>
                    </div>
                  </div>
                </template>
              </UPopover>
            </UFormField>
          </div>
        </div>
      </UCard>

      <UAlert
        v-if="loadError"
        color="error"
        variant="soft"
        title="Load Error"
        :description="loadError"
      />

      <UModal
        v-model:open="editorOpen"
        :dismissible="true"
        :close="false"
        :overlay="true"
        :scrollable="true"
        :content="{
          class: 'w-[min(96vw,1600px)] max-w-none rounded-2xl bg-white shadow-[0_24px_80px_rgba(15,23,42,0.25)]'
        }"
        :ui="{
          body: 'p-5 md:p-8',
          overlay: 'bg-slate-950/35 backdrop-blur-[2px]'
        }"
      >
        <template #body>
          <div>
              <div class="flex items-center justify-between gap-3">
                <div>
                  <div class="text-sm font-bold uppercase tracking-[0.18em] text-slate-500">
                    {{ form.id ? 'Edit Record' : 'Create Record' }}
                  </div>
                  <div class="mt-1 text-2xl font-black text-slate-950">
                    {{ form.id ? form.batterySn || form.id : 'New Battery Movement' }}
                  </div>
                </div>
                <UButton color="neutral" variant="ghost" icon="i-lucide-x" @click="closeEditor" />
              </div>

              <div class="mt-5 grid gap-3 md:grid-cols-2 xl:grid-cols-4">
                <div class="flex flex-wrap gap-2 md:col-span-2 xl:col-span-3">
                  <UButton
                    v-for="option in operationOptions.filter(option => option.value !== 'ALL')"
                    :key="option.value"
                    color="neutral"
                    variant="soft"
                    :class="[
                      'min-h-11 px-5',
                      form.operation === option.value ? 'bg-sky-600 text-white hover:bg-sky-700' : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-50'
                    ]"
                    @click="form.operation = option.value"
                  >
                    {{ option.label }}
                  </UButton>
                </div>

                <div class="w-full">
                  <UPopover :content="{ align: 'end', side: 'bottom', sideOffset: 8 }">
                    <UButton
                      color="neutral"
                      variant="outline"
                      class="h-11 w-full justify-between rounded-md border-slate-300 bg-white px-3 text-sm font-medium text-slate-700 hover:bg-slate-50"
                    >
                      <span :class="formMeasuredDate ? 'text-slate-950' : 'text-slate-500'">
                        {{ formMeasuredLabel }}
                      </span>
                      <UIcon name="i-lucide-calendar-days" class="size-4 text-slate-500" />
                    </UButton>

                    <template #content>
                      <div class="w-[320px] space-y-3 rounded-md bg-white p-3 shadow-xl">
                        <UCalendar
                          :model-value="formMeasuredDate"
                          color="neutral"
                          variant="soft"
                          :year-controls="true"
                          @update:model-value="setFormMeasuredDate($event as CalendarDate | undefined)"
                        />

                        <UFormField label="Time" :ui="{ label: 'text-sm font-semibold text-slate-700' }">
                          <UInputTime
                            :model-value="formMeasuredTime"
                            size="lg"
                            :ui="{ base: 'min-h-10 bg-white text-sm text-slate-950' }"
                            @update:model-value="setFormMeasuredTime($event as Time | undefined)"
                          />
                        </UFormField>

                        <div class="flex justify-end">
                          <UButton color="neutral" variant="ghost" size="sm" @click="setFormMeasuredDate(undefined)">
                            Clear
                          </UButton>
                        </div>
                      </div>
                    </template>
                  </UPopover>
                </div>
              </div>

              <div class="mt-5 grid gap-3 md:grid-cols-2 xl:grid-cols-4">
                <UInput v-model="form.fromRack" placeholder="From Rack" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
                <UInput v-model="form.fromSlot" placeholder="From Slot" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
                <UInput v-model="form.batterySn" placeholder="Battery S/N" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
                <UInput v-model="form.voltage" type="number" step="0.01" placeholder="Voltage" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
                <UInput v-model="form.toRack" placeholder="To Rack" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
                <UInput v-model="form.toSlot" placeholder="To Slot" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
                <UInput v-model="form.scanSource" placeholder="Scan Source" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
                <UInput v-model="form.voltageSource" placeholder="Voltage Source" :ui="{ base: '!min-h-11 !bg-white !px-3 !py-2 !text-sm !text-slate-950' }" />
              </div>

              <UAlert
                v-if="formError"
                class="mt-4"
                color="error"
                variant="soft"
                title="Save Error"
                :description="formError"
              />

              <div class="mt-5 flex flex-wrap justify-end gap-2">
                <UButton color="neutral" variant="soft" :loading="isSaving" class="bg-lime-100 text-lime-950 hover:bg-lime-200" @click="saveRecord">
                  {{ form.id ? 'Update Record' : 'Create Record' }}
                </UButton>
                <UButton color="neutral" variant="soft" class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50" @click="closeEditor">
                  Cancel
                </UButton>
                <UButton color="neutral" variant="soft" class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50" @click="resetForm">
                  Reset
                </UButton>
              </div>
          </div>
        </template>
      </UModal>

      <UCard
        :ui="{
          root: 'rounded-md ring-0 bg-white/95 shadow-[0_18px_40px_rgba(15,23,42,0.08)]',
          body: 'p-0',
        }"
      >
        <div class="overflow-x-auto">
          <table class="min-w-full text-sm">
            <thead class="bg-slate-950 text-white">
              <tr>
                <th class="px-3 py-3 text-left font-bold">Operation</th>
                <th class="px-3 py-3 text-left font-bold">Battery</th>
                <th class="px-3 py-3 text-left font-bold">From</th>
                <th class="px-3 py-3 text-left font-bold">To</th>
                <th class="px-3 py-3 text-left font-bold">Voltage</th>
                <th class="px-3 py-3 text-left font-bold">Measured</th>
                <th class="px-3 py-3 text-left font-bold">Created</th>
                <th class="px-3 py-3 text-left font-bold">Sources</th>
                <th class="px-3 py-3 text-left font-bold">Actions</th>
              </tr>
            </thead>
            <tbody>
              <tr v-if="pending">
                <td colspan="9" class="px-4 py-6 text-center text-slate-500">
                  Loading records...
                </td>
              </tr>
              <tr v-else-if="filteredRecords.length === 0">
                <td colspan="9" class="px-4 py-6 text-center text-slate-500">
                  No records found
                </td>
              </tr>
              <tr
                v-for="record in paginatedRecords"
                :key="record.id"
                class="border-b border-slate-200 align-top hover:bg-slate-50/80"
              >
                <td class="px-3 py-3">
                  <UBadge class="rounded-md px-2.5 py-1 text-[11px] font-bold uppercase tracking-[0.16em] ring-1" :class="operationMeta[record.operation].badge">
                    {{ getOperationLabel(record.operation) }}
                  </UBadge>
                </td>
                <td class="px-3 py-3">
                  <div class="font-bold text-slate-950">{{ record.batterySn }}</div>
                  <div class="mt-1 text-xs text-slate-500">{{ record.deviceId || '-' }}</div>
                </td>
                <td class="px-3 py-3">
                  <div class="font-semibold text-slate-950">{{ record.fromRack }}</div>
                  <div class="text-xs text-slate-500">{{ record.fromSlot }}</div>
                </td>
                <td class="px-3 py-3">
                  <div class="font-semibold text-slate-950">{{ record.toRack }}</div>
                  <div class="text-xs text-slate-500">{{ record.toSlot }}</div>
                </td>
                <td class="px-3 py-3 font-bold text-slate-950">
                  {{ record.voltage.toFixed(2) }} V
                </td>
                <td class="px-3 py-3 text-slate-600">
                  {{ formatDateTime(record.voltageMeasuredAt) }}
                </td>
                <td class="px-3 py-3 text-slate-600">
                  {{ formatDateTime(record.createdAt) }}
                </td>
                <td class="px-3 py-3">
                  <div class="text-xs text-slate-600">Scan: {{ record.scanSource || '-' }}</div>
                  <div class="mt-1 text-xs text-slate-600">Volt: {{ record.voltageSource || '-' }}</div>
                </td>
                <td class="px-3 py-3">
                  <div class="flex flex-wrap gap-2">
                    <UButton size="sm" color="neutral" variant="soft" class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50" @click="openEdit(record)">
                      Edit
                    </UButton>
                    <UButton size="sm" color="error" variant="soft" :loading="isDeleting" @click="deleteRecord(record.id)">
                      Delete
                    </UButton>
                  </div>
                </td>
              </tr>
            </tbody>
          </table>
        </div>

        <div class="flex flex-col gap-3 border-t border-slate-200 px-4 py-3 lg:flex-row lg:items-center lg:justify-between">
          <div class="flex flex-col gap-2 text-sm text-slate-600 sm:flex-row sm:items-center">
            <span>Showing {{ pageStart }}-{{ pageEnd }} of {{ totalCount }}</span>
            <div class="flex items-center gap-2">
              <span>Rows per page</span>
              <USelectMenu
                v-model="pageSize"
                :items="pageSizeItems"
                value-key="value"
                label-key="label"
                size="sm"
                color="neutral"
                variant="outline"
                class="w-20"
                :search-input="false"
                :ui="pageSizeSelectUi"
              />
            </div>
          </div>

          <div v-if="totalPages > 1" class="flex flex-wrap items-center gap-2">
            <UButton
              size="sm"
              color="neutral"
              variant="soft"
              :class="currentPage === 1 ? 'border border-slate-300 bg-slate-100 text-slate-500 opacity-100' : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-50'"
              :disabled="currentPage === 1"
              @click="currentPage -= 1"
            >
              Prev
            </UButton>

            <UButton
              v-for="page in visiblePages"
              :key="page"
              size="sm"
              color="neutral"
              variant="soft"
              :class="page === currentPage ? 'bg-sky-600 text-white hover:bg-sky-700' : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-50'"
              @click="currentPage = page"
            >
              {{ page }}
            </UButton>

            <UButton
              size="sm"
              color="neutral"
              variant="soft"
              :class="currentPage === totalPages ? 'border border-slate-300 bg-slate-100 text-slate-500 opacity-100' : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-50'"
              :disabled="currentPage === totalPages"
              @click="currentPage += 1"
            >
              Next
            </UButton>
          </div>
        </div>
      </UCard>
    </section>
  </main>
</template>
