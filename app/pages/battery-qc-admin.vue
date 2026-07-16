<script setup lang="ts">
import { parseDate } from '@internationalized/date'
import type { DateValue } from '@internationalized/date'

definePageMeta({
  middleware: 'admin-auth',
})

type ShipToFactory = 'AAT' | 'FTM'
type FilterShipTo = 'ALL' | ShipToFactory
type AdminJobViewState = 'NEW_JOB' | 'BEFORE_CHARGING' | 'AFTER_CHARGING' | 'QC_FOR_DELIVERY' | 'SHIPPED'
type ActionTone = 'info' | 'success' | 'warning' | 'error'
type JobTableSortKey = 'createdAt' | 'batchRef' | 'jobRef' | 'salesOrderNumber' | 'invoiceNumber' | 'shipTo' | 'plannedDeliveryDate' | 'viewState'
type SortDirection = 'asc' | 'desc'

interface SalesOrderItem {
  id: string
  soNumber: string
  description: string | null
}

interface InvoiceItem {
  id: string
  invoiceNo: string
  description: string | null
}

interface BatteryJobRecord {
  id: string
  rackId: string
  batchId: string | null
  batchRef: string
  jobRef: string
  status: string
  workflowStage?: AdminJobViewState
  workflowLabel?: string
  isLocked?: boolean
  salesOrderId: string | null
  salesOrderNumber: string | null
  invoiceId: string | null
  invoiceNumber: string | null
  plannedDeliveryDate: string | null
  shipTo: ShipToFactory | null
  beforeChargeCompletedAt: string | null
  afterChargeCompletedAt: string | null
  deliveryCompletedAt: string | null
  lockedAt: string | null
  createdAt: string
}

interface BatteryJobsCreateResponse {
  job: BatteryJobRecord
  jobs?: BatteryJobRecord[]
}

interface BatteryJobBulkResponse {
  ok: true
  action: 'UPDATE' | 'DELETE' | 'SET_SHIPPED'
  affectedCount: number
}

interface AdminJobTableRow {
  rowNumber?: number
  id: string
  rackId: string
  batchId: string | null
  batchRef: string
  createdAt: string
  createdAtValue: string
  createdAtRaw: string
  jobRef: string
  salesOrderId: string
  salesOrderNumber: string
  invoiceId: string
  invoiceNumber: string
  shipToRaw: FilterShipTo
  shipTo: string
  plannedDeliveryDateRaw: string
  plannedDeliveryDate: string
  viewState: AdminJobViewState
  viewStateLabel: string
}

const salesOrders = ref<SalesOrderItem[]>([])
const invoices = ref<InvoiceItem[]>([])
const jobs = ref<BatteryJobRecord[]>([])
const selectedIds = ref<string[]>([])
const applyWholeGroup = ref(true)
const isBusy = ref(false)
const currentDateTime = ref(new Date())
const currentPage = ref(1)
const pageSize = ref(10)
let currentDateTimeTimer: ReturnType<typeof setInterval> | null = null
const colorMode = useColorMode()

const isCreateModalOpen = ref(false)
const isUpdateModalOpen = ref(false)
const createShipDatePopoverOpen = ref(false)
const updateShipDatePopoverOpen = ref(false)
const createdDateFilterPopoverOpen = ref(false)
const shipDateFilterPopoverOpen = ref(false)
const deleteConfirmOpen = ref(false)
const pendingDelete = ref<{
  ids: string[]
  applyToGroups: boolean
  jobs: Array<Pick<AdminJobTableRow, 'jobRef' | 'rackId' | 'salesOrderNumber'>>
} | null>(null)
const toast = useToast()

const createSalesOrderId = ref('')
const createInvoiceId = ref('')
const createShipTo = ref<ShipToFactory>('AAT')
const createCreatedDate = ref('')
const createPlannedDeliveryDate = ref('')
const createRackCount = ref<number | null>(1)

const updateSalesOrderId = ref('')
const updateInvoiceId = ref('')
const updateShipTo = ref<ShipToFactory>('AAT')
const updatePlannedDeliveryDate = ref('')

const statusFilter = ref<'ALL' | AdminJobViewState>('ALL')
const salesOrderFilter = ref('ALL')
const invoiceFilter = ref('ALL')
const groupFilter = ref('ALL')
const shipToFilter = ref<FilterShipTo>('ALL')
const createdDateFilter = ref('')
const shipDateFilter = ref('')
const sortKey = ref<JobTableSortKey>('createdAt')
const sortDirection = ref<SortDirection>('desc')

const shipToOptions = [
  { label: 'AAT', value: 'AAT' },
  { label: 'FTM', value: 'FTM' },
] as const

const statusFilterOptions = [
  { label: 'All Status', value: 'ALL' },
  { label: 'New Job', value: 'NEW_JOB' },
  { label: 'Before Charge', value: 'BEFORE_CHARGING' },
  { label: 'After Charge', value: 'AFTER_CHARGING' },
  { label: 'QC for Delivery', value: 'QC_FOR_DELIVERY' },
  { label: 'Shipped', value: 'SHIPPED' },
] as const

const salesOrderOptions = computed(() => salesOrders.value.map(item => ({
  label: item.soNumber,
  value: item.id,
})))

const invoiceOptions = computed(() => invoices.value.map(item => ({
  label: item.invoiceNo,
  value: item.id,
})))

const salesOrderFilterOptions = computed(() => [{ label: 'All SO', value: 'ALL' }, ...salesOrderOptions.value])
const invoiceFilterOptions = computed(() => [{ label: 'All Invoice', value: 'ALL' }, ...invoiceOptions.value])
const groupFilterOptions = computed(() => [
  { label: 'All Group ID', value: 'ALL' },
  ...Array.from(new Set(jobs.value.map(job => job.batchRef).filter(Boolean)))
    .sort((left, right) => left.localeCompare(right))
    .map(batchRef => ({ label: batchRef, value: batchRef })),
])
const shipToFilterOptions = [
  { label: 'All Ship To', value: 'ALL' },
  { label: 'AAT', value: 'AAT' },
  { label: 'FTM', value: 'FTM' },
] as const
const pageSizeOptions = [
  { label: '10 รายการ', value: 10 },
  { label: '20 รายการ', value: 20 },
  { label: '50 รายการ', value: 50 },
  { label: '100 รายการ', value: 100 },
] as const

const currentDateTimeLabel = computed(() => new Intl.DateTimeFormat('th-TH', {
  dateStyle: 'medium',
  timeStyle: 'medium',
}).format(currentDateTime.value))

const isDarkMode = computed({
  get: () => colorMode.value === 'dark',
  set: value => colorMode.preference = value ? 'dark' : 'light',
})

const themeIcon = computed(() => isDarkMode.value ? 'i-lucide-sun-medium' : 'i-lucide-moon-star')

const pageBgClass = computed(() => isDarkMode.value
  ? 'min-h-screen bg-[radial-gradient(circle_at_top,_rgba(59,130,246,0.16),_transparent_35%),linear-gradient(180deg,_#0f172a_0%,_#111827_100%)]'
  : 'min-h-screen bg-[radial-gradient(circle_at_top_left,_rgba(37,99,235,0.14),_transparent_28%),linear-gradient(180deg,_#ccd7e4_0%,_#bfccdb_48%,_#b7c4d2_100%)]')

const headerCardUi = computed(() => ({
  root: isDarkMode.value
    ? 'rounded-[15px] border-0 bg-slate-900/92 ring-1 ring-slate-700 shadow-[0_18px_40px_rgba(2,6,23,0.45)]'
    : 'rounded-[15px] border-0 bg-[linear-gradient(180deg,_rgba(248,251,254,0.97)_0%,_rgba(236,242,248,0.98)_100%)] ring-1 ring-slate-300 shadow-[0_20px_44px_rgba(15,23,42,0.14)]',
  body: 'p-5 sm:p-6',
}))

const sectionCardUi = computed(() => ({
  root: isDarkMode.value
    ? 'rounded-[15px] border-0 bg-slate-900/92 ring-1 ring-slate-700 shadow-[0_16px_36px_rgba(2,6,23,0.4)]'
    : 'rounded-[15px] border-0 bg-[linear-gradient(180deg,_rgba(245,249,253,0.97)_0%,_rgba(233,240,247,0.99)_100%)] ring-1 ring-slate-300 shadow-[0_18px_40px_rgba(15,23,42,0.13)]',
  body: 'p-5',
}))

const modalCardUi = computed(() => ({
  root: isDarkMode.value
    ? 'rounded-[15px] border-0 bg-slate-900 ring-1 ring-slate-700 shadow-[0_24px_60px_rgba(2,6,23,0.55)]'
    : 'rounded-[15px] border-0 bg-[linear-gradient(180deg,_#f7fafe_0%,_#eaf1f8_100%)] ring-1 ring-slate-300 shadow-[0_24px_60px_rgba(15,23,42,0.16)]',
  header: 'p-5 pb-0',
  body: 'p-5',
  footer: isDarkMode.value ? 'p-5 border-t border-slate-700' : 'p-5 border-t border-slate-200',
}))

const workspaceLabelClass = computed(() => isDarkMode.value ? 'text-slate-400' : 'text-slate-600')
const headingClass = computed(() => isDarkMode.value ? 'text-slate-50' : 'text-slate-950')
const bodyTextClass = computed(() => isDarkMode.value ? 'text-slate-300' : 'text-slate-700')
const tableWrapClass = computed(() => isDarkMode.value ? 'mt-4 overflow-hidden rounded-[15px] border border-slate-700' : 'mt-4 overflow-hidden rounded-[15px] border border-slate-300 bg-white/92 shadow-[0_16px_32px_rgba(15,23,42,0.12)]')

const tableUi = computed(() => ({
  tbody: isDarkMode.value
    ? '[&_tr[data-selected=true]]:bg-slate-800/95 [&_tr[data-selected=true]_td]:!text-slate-100 [&_tr[data-selected=true]_button]:!text-slate-100'
    : '[&_tr[data-selected=true]]:bg-sky-100/90 [&_tr[data-selected=true]_td]:!text-slate-900 [&_tr[data-selected=true]_button]:!text-slate-900',
  th: isDarkMode.value
    ? 'bg-slate-800 text-[13px] font-black uppercase tracking-[0.12em] text-slate-300'
    : 'bg-slate-200/95 text-[13px] font-black uppercase tracking-[0.12em] text-slate-700',
  td: isDarkMode.value
    ? 'py-3 text-sm font-semibold !text-slate-200 align-middle'
    : 'py-3 text-sm font-semibold !text-slate-800 align-middle',
  tr: isDarkMode.value
    ? 'transition-colors hover:bg-slate-800/70'
    : 'transition-colors hover:bg-slate-100/80',
}))

const themeToggleButtonClass = computed(() => isDarkMode.value
  ? 'rounded-full border border-slate-600 bg-slate-800 text-slate-100 hover:bg-slate-700'
  : 'rounded-full border border-slate-300 bg-white/92 text-slate-800 shadow-[0_8px_18px_rgba(15,23,42,0.08)] hover:bg-slate-100')

const outlineActionButtonClass = computed(() => isDarkMode.value
  ? 'rounded-full border border-sky-700/70 bg-slate-800 px-4 text-sky-100 hover:bg-slate-700'
  : 'rounded-full border border-slate-300 bg-white/90 px-4 text-slate-800 shadow-[0_8px_18px_rgba(15,23,42,0.06)] hover:bg-slate-100')

const checkboxWrapClass = computed(() => isDarkMode.value
  ? 'border-slate-600 bg-slate-800/80 text-slate-100'
  : 'border-slate-300 bg-white/92 text-slate-900 shadow-[0_8px_18px_rgba(15,23,42,0.06)]')

const jobsBadgeClass = computed(() => isDarkMode.value
  ? 'rounded-full bg-sky-500/15 px-4 py-1.5 text-sm font-black uppercase tracking-[0.14em] text-sky-200 ring-1 ring-sky-500/30'
  : 'rounded-full bg-white/90 px-4 py-1.5 text-sm font-black uppercase tracking-[0.14em] text-sky-800 ring-1 ring-sky-200 shadow-[0_8px_18px_rgba(15,23,42,0.05)]')

const adminHeaderLogoSrc = computed(() => isDarkMode.value
  ? '/branding/logo-puma-battery.png'
  : '/branding/logo-puma-battery-white-bg.png')


const adminFieldUi = computed(() => ({
  root: 'w-full',
  label: `mb-1.5 text-xs font-semibold ${isDarkMode.value ? 'text-slate-300' : 'text-slate-700'}`,
}))

const adminInputUi = computed(() => ({
  base: isDarkMode.value
    ? 'h-10 rounded-[8px] border-slate-600 bg-slate-800 px-3 text-sm font-medium text-slate-50 transition hover:border-sky-400 hover:bg-slate-750 focus:border-sky-400 focus-visible:ring-2 focus-visible:ring-sky-400/30'
    : 'h-10 rounded-[8px] border-slate-400 bg-white px-3 text-sm font-medium text-slate-950 shadow-[inset_0_1px_0_rgba(255,255,255,0.7)] transition hover:border-slate-500 focus:border-sky-600 focus-visible:ring-2 focus-visible:ring-sky-300',
}))

const adminGhostUi = computed(() => ({
  base: isDarkMode.value
    ? 'h-10 rounded-[8px] border border-slate-700 bg-slate-800/80 px-3 text-sm font-medium text-slate-100'
    : 'h-10 rounded-[8px] border border-slate-300 bg-slate-100/95 px-3 text-sm font-medium text-slate-950',
}))

const adminSelectUi = computed(() => ({
  base: isDarkMode.value
    ? 'h-10 rounded-[8px] border-slate-600 bg-slate-800 px-3 text-sm font-medium text-slate-50 transition hover:border-sky-400 hover:bg-slate-800 focus:border-sky-400 focus-visible:ring-2 focus-visible:ring-sky-400/30'
    : 'h-10 rounded-[8px] border-slate-400 bg-white px-3 text-sm font-medium text-slate-950 shadow-[inset_0_1px_0_rgba(255,255,255,0.7)] transition hover:border-slate-500 focus:border-sky-600 focus-visible:ring-2 focus-visible:ring-sky-300',
  value: isDarkMode.value ? 'truncate pointer-events-none text-slate-50' : 'truncate pointer-events-none text-slate-950',
  placeholder: isDarkMode.value ? 'truncate text-slate-400' : 'truncate text-slate-500',
  trailingIcon: isDarkMode.value ? 'shrink-0 text-slate-400' : 'shrink-0 text-slate-600',
  content: isDarkMode.value ? 'rounded-[12px] bg-slate-900 ring-slate-700' : 'rounded-[12px] bg-white ring-slate-200 shadow-[0_18px_36px_rgba(15,23,42,0.14)]',
  viewport: 'max-h-[260px]',
  item: isDarkMode.value ? 'text-sm font-medium text-slate-100' : 'text-sm font-medium text-slate-900',
}))

const adminInputNumberUi = computed(() => ({
  base: adminInputUi.value.base,
  increment: 'end-0 pe-2',
  decrement: 'start-0 ps-2',
}))

const adminInputNumberButtonClass = computed(() => isDarkMode.value
  ? 'h-8 w-8 rounded-full bg-slate-700 text-slate-100 ring-1 ring-slate-500 hover:bg-sky-500 hover:text-slate-950 focus-visible:bg-sky-400 focus-visible:text-slate-950 disabled:opacity-100 disabled:bg-slate-700 disabled:text-slate-400'
  : 'h-8 w-8 rounded-full bg-white text-slate-800 ring-1 ring-slate-300 hover:bg-slate-100 hover:text-slate-950 focus-visible:bg-slate-100 focus-visible:text-slate-950')

const navSecondaryButtonClass = computed(() => isDarkMode.value
  ? 'rounded-full border border-sky-700/70 bg-slate-800 px-4 text-sky-100 hover:bg-slate-700'
  : 'rounded-full border border-sky-300 bg-white/92 px-4 text-sky-900 shadow-[0_8px_18px_rgba(15,23,42,0.06)] hover:bg-sky-50')

const navPrimaryButtonClass = computed(() => isDarkMode.value
  ? 'rounded-full bg-sky-500 px-4 text-slate-950 hover:bg-sky-400'
  : 'rounded-full bg-sky-600 px-4 text-white shadow-[0_10px_24px_rgba(2,132,199,0.24)] hover:bg-sky-700')

const navLogoutButtonClass = computed(() => isDarkMode.value
  ? 'rounded-full border border-slate-600 bg-slate-800 px-4 text-slate-100 hover:bg-slate-700'
  : 'rounded-full border border-slate-300 bg-white/92 px-4 text-slate-700 shadow-[0_8px_18px_rgba(15,23,42,0.06)] hover:bg-slate-100')

const tableHeaderButtonClass = computed(() => isDarkMode.value
  ? 'h-auto rounded-full px-1.5 py-1 text-slate-300 hover:bg-slate-700 hover:text-white'
  : 'h-auto rounded-full px-1.5 py-1 text-slate-700 hover:bg-slate-300 hover:text-slate-950')

function getJobViewState(job: BatteryJobRecord): AdminJobViewState {
  return job.workflowStage ?? (
    job.lockedAt
      ? 'SHIPPED'
      : !job.beforeChargeCompletedAt
          ? 'NEW_JOB'
          : !job.afterChargeCompletedAt
              ? 'BEFORE_CHARGING'
              : !job.deliveryCompletedAt
                  ? 'AFTER_CHARGING'
                  : 'QC_FOR_DELIVERY'
  )
}

function getJobViewStateLabel(state: AdminJobViewState) {
  const labels: Record<AdminJobViewState, string> = {
    NEW_JOB: 'New Job',
    BEFORE_CHARGING: 'Before Charge',
    AFTER_CHARGING: 'After Charge',
    QC_FOR_DELIVERY: 'QC Delivery',
    SHIPPED: 'Shipped',
  }

  return labels[state]
}

function getStateBadgeClass(state: AdminJobViewState) {
  const classes: Record<AdminJobViewState, string> = {
    NEW_JOB: 'bg-rose-100 text-rose-900 ring-1 ring-rose-200',
    BEFORE_CHARGING: 'bg-lime-100 text-lime-900 ring-1 ring-lime-200',
    AFTER_CHARGING: 'bg-sky-100 text-sky-900 ring-1 ring-sky-200',
    QC_FOR_DELIVERY: 'bg-orange-100 text-orange-900 ring-1 ring-orange-200',
    SHIPPED: 'bg-emerald-100 text-emerald-900 ring-1 ring-emerald-200',
  }

  return classes[state]
}

const tableRows = computed<AdminJobTableRow[]>(() => jobs.value
  .slice()
  .map((job) => {
    const viewState = getJobViewState(job)
    return {
      id: job.id,
      rackId: job.rackId,
      batchId: job.batchId,
      batchRef: job.batchRef,
      createdAt: formatDateTime(job.createdAt),
      createdAtValue: job.createdAt,
      createdAtRaw: toDateInputValue(job.createdAt),
      jobRef: job.jobRef,
      salesOrderId: job.salesOrderId ?? '',
      salesOrderNumber: job.salesOrderNumber ?? '-',
      invoiceId: job.invoiceId ?? '',
      invoiceNumber: job.invoiceNumber ?? '-',
      shipToRaw: (job.shipTo ?? 'ALL') as FilterShipTo,
      shipTo: job.shipTo ?? '-',
      plannedDeliveryDateRaw: toDateInputValue(job.plannedDeliveryDate),
      plannedDeliveryDate: formatDate(job.plannedDeliveryDate),
      viewState,
      viewStateLabel: getJobViewStateLabel(viewState),
    }
  }))

const filteredRows = computed(() => tableRows.value.filter((row) => {
  if (statusFilter.value !== 'ALL' && row.viewState !== statusFilter.value) return false
  if (salesOrderFilter.value !== 'ALL' && row.salesOrderId !== salesOrderFilter.value) return false
  if (invoiceFilter.value !== 'ALL' && row.invoiceId !== invoiceFilter.value) return false
  if (groupFilter.value !== 'ALL' && row.batchRef !== groupFilter.value) return false
  if (shipToFilter.value !== 'ALL' && row.shipToRaw !== shipToFilter.value) return false
  if (createdDateFilter.value && row.createdAtRaw !== createdDateFilter.value) return false
  if (shipDateFilter.value && row.plannedDeliveryDateRaw !== shipDateFilter.value) return false
  return true
}))

function getSortValue(row: AdminJobTableRow, key: JobTableSortKey) {
  const values: Record<JobTableSortKey, string> = {
    createdAt: row.createdAtValue,
    batchRef: row.batchRef,
    jobRef: row.jobRef,
    salesOrderNumber: row.salesOrderNumber,
    invoiceNumber: row.invoiceNumber,
    shipTo: row.shipTo,
    plannedDeliveryDate: row.plannedDeliveryDateRaw,
    viewState: row.viewStateLabel,
  }

  return values[key]
}

const sortedRows = computed(() => filteredRows.value
  .slice()
  .sort((left, right) => {
    const comparison = getSortValue(left, sortKey.value).localeCompare(getSortValue(right, sortKey.value), 'th')
    return sortDirection.value === 'asc' ? comparison : -comparison
  }))

const totalPages = computed(() => Math.max(1, Math.ceil(filteredRows.value.length / pageSize.value)))
const paginatedRows = computed(() => {
  const start = (currentPage.value - 1) * pageSize.value
  return sortedRows.value
    .slice(start, start + pageSize.value)
    .map((row, index) => ({
      ...row,
      rowNumber: start + index + 1,
    }))
})

const selectedRows = computed(() => tableRows.value.filter(row => selectedIds.value.includes(row.id)))
const allVisibleSelected = computed(() => paginatedRows.value.length > 0 && paginatedRows.value.every(row => selectedIds.value.includes(row.id)))
const selectedCount = computed(() => selectedIds.value.length)

const tableColumns = [
  { accessorKey: 'rowNumber', header: '#' },
  { accessorKey: 'select', header: '' },
  { accessorKey: 'createdAt', header: 'วันที่สร้างใบงาน' },
  { accessorKey: 'batchRef', header: 'Group ID' },
  { accessorKey: 'jobRef', header: 'Job ID' },
  { accessorKey: 'salesOrderNumber', header: 'SO' },
  { accessorKey: 'invoiceNumber', header: 'Invoice' },
  { accessorKey: 'shipTo', header: 'Ship To' },
  { accessorKey: 'plannedDeliveryDate', header: 'Ship Date' },
  { accessorKey: 'viewState', header: 'State' },
  { accessorKey: 'actions', header: '' },
] as const

function toggleSort(key: JobTableSortKey) {
  if (sortKey.value === key) {
    sortDirection.value = sortDirection.value === 'asc' ? 'desc' : 'asc'
  }
  else {
    sortKey.value = key
    sortDirection.value = 'asc'
  }

  currentPage.value = 1
}

function sortIcon(key: JobTableSortKey) {
  if (sortKey.value !== key) return 'i-lucide-arrow-up-down'
  return sortDirection.value === 'asc' ? 'i-lucide-arrow-up' : 'i-lucide-arrow-down'
}

function updatePageSize(value: number | { value?: number } | null) {
  const normalized = typeof value === 'object' && value !== null
    ? Number(value.value)
    : Number(value)

  pageSize.value = [10, 20, 50, 100].includes(normalized) ? normalized : 10
}

const createCalendarValue = computed<DateValue | undefined>({
  get: () => createPlannedDeliveryDate.value ? parseDate(createPlannedDeliveryDate.value) : undefined,
  set: value => createPlannedDeliveryDate.value = value ? value.toString() : '',
})

const updateCalendarValue = computed<DateValue | undefined>({
  get: () => updatePlannedDeliveryDate.value ? parseDate(updatePlannedDeliveryDate.value) : undefined,
  set: value => updatePlannedDeliveryDate.value = value ? value.toString() : '',
})

const createdDateFilterValue = computed<DateValue | undefined>({
  get: () => createdDateFilter.value ? parseDate(createdDateFilter.value) : undefined,
  set: value => createdDateFilter.value = value ? value.toString() : '',
})

const shipDateFilterValue = computed<DateValue | undefined>({
  get: () => shipDateFilter.value ? parseDate(shipDateFilter.value) : undefined,
  set: value => shipDateFilter.value = value ? value.toString() : '',
})

function toDateInputValue(value: string | null) {
  if (!value) return ''
  return new Date(value).toISOString().slice(0, 10)
}

function formatDate(value: string | null) {
  if (!value) return '-'
  return new Intl.DateTimeFormat('th-TH', { dateStyle: 'medium' }).format(new Date(value))
}

function formatDateTime(value: string | null) {
  if (!value) return '-'
  return new Intl.DateTimeFormat('th-TH', { dateStyle: 'medium', timeStyle: 'short' }).format(new Date(value))
}

function formatInputDateLabel(value: string) {
  if (!value) return ''
  return formatDate(`${value}T00:00:00`)
}

function getTodayInputValue() {
  const now = new Date()
  const year = now.getFullYear()
  const month = String(now.getMonth() + 1).padStart(2, '0')
  const day = String(now.getDate()).padStart(2, '0')
  return `${year}-${month}-${day}`
}

function setActionFeedback(message: string, tone: ActionTone = 'info') {
  toast.add({
    title: message,
    color: tone,
    duration: tone === 'error' ? 8000 : 5000,
  })
}

function toggleVisibleSelection(checked: boolean | 'indeterminate') {
  if (checked !== true) {
    selectedIds.value = selectedIds.value.filter(id => !paginatedRows.value.some(row => row.id === id))
    return
  }

  const merged = new Set([...selectedIds.value, ...paginatedRows.value.map(row => row.id)])
  selectedIds.value = Array.from(merged)
}

function toggleRowSelection(id: string, checked: boolean | 'indeterminate') {
  if (checked === true) {
    if (!selectedIds.value.includes(id)) {
      selectedIds.value = [...selectedIds.value, id]
    }
    return
  }

  selectedIds.value = selectedIds.value.filter(item => item !== id)
}

function resetCreateForm() {
  createSalesOrderId.value = ''
  createInvoiceId.value = ''
  createShipTo.value = 'AAT'
  createCreatedDate.value = getTodayInputValue()
  createPlannedDeliveryDate.value = ''
  createRackCount.value = 1
}

function resetUpdateForm() {
  updateSalesOrderId.value = ''
  updateInvoiceId.value = ''
  updateShipTo.value = 'AAT'
  updatePlannedDeliveryDate.value = ''
}

function openCreateModal() {
  resetCreateForm()
  isCreateModalOpen.value = true
}

function openUpdateModal() {
  if (!selectedRows.value.length) {
    setActionFeedback('เลือกใบงานอย่างน้อย 1 รายการก่อนอัปเดต', 'warning')
    return
  }

  const firstRow = selectedRows.value[0]
  updateSalesOrderId.value = firstRow.salesOrderId
  updateInvoiceId.value = firstRow.invoiceId
  updateShipTo.value = firstRow.shipToRaw === 'ALL' ? 'AAT' : firstRow.shipToRaw
  updatePlannedDeliveryDate.value = firstRow.plannedDeliveryDateRaw
  isUpdateModalOpen.value = true
}

function validateCreateForm() {
  if (!createSalesOrderId.value) {
    setActionFeedback('Sale Order เป็นข้อมูลบังคับ', 'error')
    return false
  }

  if (!createInvoiceId.value) {
    setActionFeedback('Invoice เป็นข้อมูลบังคับ', 'error')
    return false
  }

  if (!createShipTo.value) {
    setActionFeedback('Ship To เป็นข้อมูลบังคับ', 'error')
    return false
  }

  const count = Number(createRackCount.value)
  if (!Number.isInteger(count) || count < 1) {
    setActionFeedback('จำนวน Rack ต้องเป็นตัวเลขตั้งแต่ 1 ขึ้นไป', 'error')
    return false
  }

  return true
}

function validateUpdateForm() {
  if (!updateSalesOrderId.value) {
    setActionFeedback('Sale Order เป็นข้อมูลบังคับ', 'error')
    return false
  }

  if (!updateInvoiceId.value) {
    setActionFeedback('Invoice เป็นข้อมูลบังคับ', 'error')
    return false
  }

  if (!updateShipTo.value) {
    setActionFeedback('Ship To เป็นข้อมูลบังคับ', 'error')
    return false
  }

  return true
}

async function loadMasters() {
  const [salesOrderResponse, invoiceResponse] = await Promise.all([
    $fetch<{ salesOrders: SalesOrderItem[] }>('/api/sales-orders'),
    $fetch<{ invoices: InvoiceItem[] }>('/api/invoices'),
  ])

  salesOrders.value = salesOrderResponse.salesOrders
  invoices.value = invoiceResponse.invoices
}

async function loadJobs() {
  const response = await $fetch<{ jobs: BatteryJobRecord[] }>('/api/battery-jobs')
  jobs.value = response.jobs
  selectedIds.value = selectedIds.value.filter(id => jobs.value.some(job => job.id === id))

  if (currentPage.value > totalPages.value) {
    currentPage.value = totalPages.value
  }
}

async function createJobs() {
  if (!validateCreateForm()) return

  isBusy.value = true
  try {
    const response = await $fetch<BatteryJobsCreateResponse>('/api/battery-jobs/admin', {
      method: 'POST',
      body: {
        salesOrderId: createSalesOrderId.value,
        invoiceId: createInvoiceId.value,
        shipTo: createShipTo.value,
        plannedDeliveryDate: createPlannedDeliveryDate.value ? new Date(`${createPlannedDeliveryDate.value}T00:00:00`).toISOString() : null,
        rackCount: Number(createRackCount.value),
      },
    })

    const createdCount = response.jobs?.length ?? 1
    setActionFeedback(`สร้าง QC job สำเร็จ ${createdCount} รายการในชุดเดียวกัน`, 'success')
    isCreateModalOpen.value = false
    await loadJobs()
  }
  finally {
    isBusy.value = false
  }
}

async function logoutAdmin() {
  await $fetch('/api/admin/logout', {
    method: 'POST',
  })

  await navigateTo('/admin-login')
}

async function runBulkAction(action: 'UPDATE' | 'DELETE' | 'SET_SHIPPED', payload: Record<string, any> = {}) {
  if (!selectedIds.value.length) {
    setActionFeedback('เลือกใบงานอย่างน้อย 1 รายการก่อน', 'warning')
    return null
  }

  isBusy.value = true

  try {
    const response = await $fetch<BatteryJobBulkResponse>('/api/battery-jobs/bulk', {
      method: 'POST',
      body: {
        action,
        ids: selectedIds.value,
        applyToGroups: applyWholeGroup.value,
        ...payload,
      },
    })

    await loadJobs()
    return response
  }
  finally {
    isBusy.value = false
  }
}

async function submitUpdateSelected() {
  if (!validateUpdateForm()) return

  const response = await runBulkAction('UPDATE', {
    salesOrderId: updateSalesOrderId.value,
    invoiceId: updateInvoiceId.value,
    shipTo: updateShipTo.value,
    plannedDeliveryDate: updatePlannedDeliveryDate.value ? new Date(`${updatePlannedDeliveryDate.value}T00:00:00`).toISOString() : null,
  })

  if (!response) return

  isUpdateModalOpen.value = false
  setActionFeedback(`อัปเดตสำเร็จ ${response.affectedCount} รายการ${applyWholeGroup.value ? ' โดยขยายทั้ง Group ID' : ''}`, 'success')
}

async function submitSetShipped() {
  if (!selectedIds.value.length) {
    setActionFeedback('เลือกใบงานอย่างน้อย 1 รายการก่อน set shipped', 'warning')
    return
  }

  if (!window.confirm(`Set shipped ให้ ${selectedIds.value.length} รายการ${applyWholeGroup.value ? ' และขยายทั้ง Group ID' : ''} ใช่หรือไม่?`)) {
    return
  }

  const response = await runBulkAction('SET_SHIPPED')
  if (!response) return
  setActionFeedback(`Set shipped สำเร็จ ${response.affectedCount} รายการ`, 'success')
}

async function submitDeleteSelected() {
  if (!selectedIds.value.length) {
    setActionFeedback('เลือกใบงานอย่างน้อย 1 รายการก่อนลบ', 'warning')
    return
  }

  pendingDelete.value = {
    ids: [...selectedIds.value],
    applyToGroups: applyWholeGroup.value,
    jobs: selectedRows.value.map(row => ({
      jobRef: row.jobRef,
      rackId: row.rackId,
      salesOrderNumber: row.salesOrderNumber,
    })),
  }
  deleteConfirmOpen.value = true
}

async function confirmDelete() {
  const request = pendingDelete.value
  if (!request) return

  isBusy.value = true
  try {
    const response = await $fetch<BatteryJobBulkResponse>('/api/battery-jobs/bulk', {
      method: 'POST',
      body: {
        action: 'DELETE',
        ids: request.ids,
        applyToGroups: request.applyToGroups,
      },
    })

    await loadJobs()
    selectedIds.value = []
    const jobList = request.jobs.map(job => `${job.jobRef} (${job.rackId})`).join(', ')
    setActionFeedback(`ลบจริงสำเร็จ ${response.affectedCount} รายการ: ${jobList}`, 'success')
  }
  finally {
    isBusy.value = false
    deleteConfirmOpen.value = false
    pendingDelete.value = null
  }
}

async function deleteSingleJob(id: string) {
  const row = tableRows.value.find(job => job.id === id)
  if (!row) {
    setActionFeedback('ไม่พบข้อมูลใบงานที่ต้องการลบ', 'error')
    return
  }

  pendingDelete.value = {
    ids: [id],
    applyToGroups: false,
    jobs: [{
      jobRef: row.jobRef,
      rackId: row.rackId,
      salesOrderNumber: row.salesOrderNumber,
    }],
  }
  deleteConfirmOpen.value = true
}

watch([statusFilter, salesOrderFilter, invoiceFilter, groupFilter, shipToFilter, createdDateFilter, shipDateFilter], () => {
  currentPage.value = 1
})

watch(currentPage, (value) => {
  if (value > totalPages.value) {
    currentPage.value = totalPages.value
  }
})

watch(pageSize, () => {
  currentPage.value = 1
})

onMounted(async () => {
  currentDateTimeTimer = setInterval(() => {
    currentDateTime.value = new Date()
  }, 1000)

  await loadMasters()
  await loadJobs()
})

onBeforeUnmount(() => {
  if (currentDateTimeTimer) {
    clearInterval(currentDateTimeTimer)
  }
})
</script>

<template>
  <main :class="[pageBgClass, `px-4 py-6 [font-family:'IBM_Plex_Sans_Thai','Inter',ui-sans-serif,sans-serif] sm:px-6 lg:px-8`]" >
    <section class="mx-auto max-w-[1600px] space-y-4">
      <UCard :ui="headerCardUi">
        <div class="space-y-3">
          <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
            <div class="flex items-center gap-3">
              <img :src="adminHeaderLogoSrc" alt="PUMA Battery" class="h-10 w-auto object-contain sm:h-12" >
              <div :class="['text-xs font-black uppercase tracking-[0.24em]', workspaceLabelClass]">QC Workspace</div>
            </div>

            <div :class="['flex flex-wrap items-center justify-end gap-2 text-xs font-black uppercase tracking-[0.18em]', workspaceLabelClass]">
              <div class="flex items-center gap-2">
                <UIcon :class="['size-4', workspaceLabelClass]" name="i-lucide-calendar-clock" />
                <span>{{ currentDateTimeLabel }}</span>
              </div>

              <UButton
                :icon="themeIcon"
                color="neutral"
                variant="outline"
                :class="themeToggleButtonClass"
                square
                @click="isDarkMode = !isDarkMode"
              />
            </div>
          </div>

          <div class="flex flex-col gap-3 xl:flex-row xl:items-center xl:justify-between">
            <div class="space-y-1">
              <h1 :class="['text-[1.8rem] font-black tracking-tight leading-none sm:text-[2.2rem]', headingClass]">Battery QC Admin</h1>
            </div>

            <div class="flex flex-wrap justify-start gap-2 xl:justify-end">
              <UButton to="/battery-qc-system" :class="navSecondaryButtonClass">
                Battery QC System
              </UButton>
              <UButton :class="navPrimaryButtonClass" @click="openCreateModal">
                Create QC Job
              </UButton>
              <UButton color="neutral" variant="outline" :class="navLogoutButtonClass" @click="logoutAdmin">
                Logout
              </UButton>
            </div>
          </div>
        </div>
      </UCard>

      <UCard :ui="sectionCardUi">
        <div class="space-y-4">
          <div class="flex flex-col gap-3 xl:flex-row xl:items-start xl:justify-between">
            <div>
              <div :class="['text-xs font-black uppercase tracking-[0.2em]', workspaceLabelClass]">QC Job List</div>
              <h2 :class="['mt-1 text-2xl font-black', headingClass]">รายการใบงาน QC ตาม Rack</h2>
            </div>

            <UBadge color="info" variant="soft" :class="jobsBadgeClass">
              {{ filteredRows.length }} Jobs
            </UBadge>
          </div>

          <div class="flex flex-col gap-3 xl:flex-row xl:items-center xl:justify-end">
            <div :class="[checkboxWrapClass, 'flex items-center gap-2 rounded-full border px-3 py-2']">
              <UCheckbox v-model="applyWholeGroup" />
              <span class="text-sm font-semibold">Apply whole Group ID</span>
            </div>

            <div class="flex flex-wrap items-center justify-end gap-3">
              <UButton :loading="isBusy" :class="outlineActionButtonClass" @click="openUpdateModal">
                Update Selected
              </UButton>
              <UButton :loading="isBusy" :class="outlineActionButtonClass" @click="submitSetShipped">
                Set Shipped
              </UButton>
              <UButton :loading="isBusy" :class="outlineActionButtonClass" @click="submitDeleteSelected">
                Delete Selected
              </UButton>
            </div>
          </div>

          <div :class="[isDarkMode ? 'border-slate-700 bg-slate-800/55' : 'border-slate-300 bg-white/55', 'rounded-[15px] border p-3']">
            <div class="mb-3 flex items-center gap-2">
              <UIcon name="i-lucide-list-filter" :class="['size-4', workspaceLabelClass]" />
              <span :class="['text-xs font-black uppercase tracking-[0.18em]', workspaceLabelClass]">Filters</span>
            </div>

            <div class="grid gap-3 xl:grid-cols-7">
              <div class="relative">
                <UInput
                  :model-value="formatInputDateLabel(createdDateFilter)"
                  readonly
                  aria-label="Created Date"
                  placeholder="Created Date"
                  color="neutral"
                  variant="outline"
                  class="w-full cursor-pointer"
                  :ui="{ ...adminInputUi, base: `${adminInputUi.base} pr-12` }"
                  @click="createdDateFilterPopoverOpen = true"
                />

                <UPopover v-model:open="createdDateFilterPopoverOpen">
                  <UButton
                    color="neutral"
                    variant="ghost"
                    icon="i-lucide-calendar-clock"
                    class="absolute top-1/2 right-2 z-10 -translate-y-1/2 rounded-full text-slate-600 hover:bg-sky-50 hover:text-sky-900"
                    @click="createdDateFilterPopoverOpen = true"
                  />

                  <template #content>
                    <UCalendar v-model="createdDateFilterValue" class="p-2" @update:model-value="createdDateFilterPopoverOpen = false" />
                  </template>
                </UPopover>
              </div>

              <USelectMenu v-model="groupFilter" :items="groupFilterOptions" value-key="value" label-key="label" aria-label="Group ID" placeholder="All Group ID" color="neutral" variant="outline" :search-input="{ placeholder: 'Search Group ID...' }" class="w-full" :ui="adminSelectUi" />

              <USelectMenu v-model="salesOrderFilter" :items="salesOrderFilterOptions" value-key="value" label-key="label" aria-label="SO" placeholder="All SO" color="neutral" variant="outline" :search-input="{ placeholder: 'Search SO...' }" class="w-full" :ui="adminSelectUi" />

              <USelectMenu v-model="invoiceFilter" :items="invoiceFilterOptions" value-key="value" label-key="label" aria-label="Invoice" placeholder="All Invoice" color="neutral" variant="outline" :search-input="{ placeholder: 'Search invoice...' }" class="w-full" :ui="adminSelectUi" />

              <USelectMenu v-model="shipToFilter" :items="shipToFilterOptions" value-key="value" label-key="label" aria-label="Ship To" placeholder="All Ship To" color="neutral" variant="outline" :search-input="false" class="w-full" :ui="adminSelectUi" />

              <div class="relative">
                <UInput
                  :model-value="formatInputDateLabel(shipDateFilter)"
                  readonly
                  aria-label="Ship Date"
                  placeholder="Ship Date"
                  color="neutral"
                  variant="outline"
                  class="w-full cursor-pointer"
                  :ui="{ ...adminInputUi, base: `${adminInputUi.base} pr-12` }"
                  @click="shipDateFilterPopoverOpen = true"
                />

                <UPopover v-model:open="shipDateFilterPopoverOpen">
                  <UButton
                    color="neutral"
                    variant="ghost"
                    icon="i-lucide-calendar-days"
                    class="absolute top-1/2 right-2 z-10 -translate-y-1/2 rounded-full text-slate-600 hover:bg-sky-50 hover:text-sky-900"
                    @click="shipDateFilterPopoverOpen = true"
                  />

                  <template #content>
                    <UCalendar v-model="shipDateFilterValue" class="p-2" @update:model-value="shipDateFilterPopoverOpen = false" />
                  </template>
                </UPopover>
              </div>

              <USelectMenu v-model="statusFilter" :items="statusFilterOptions" value-key="value" label-key="label" aria-label="State" placeholder="All Status" color="neutral" variant="outline" :search-input="false" class="w-full" :ui="adminSelectUi" />
            </div>
          </div>
        </div>

        <div :class="tableWrapClass">
          <UTable
            :data="paginatedRows"
            :columns="tableColumns"
            :ui="tableUi"
            empty="ไม่พบรายการใบงานตามเงื่อนไขที่เลือก"
          >
            <template #select-header>
              <div class="flex justify-center">
                <UCheckbox :model-value="allVisibleSelected" @update:model-value="toggleVisibleSelection" />
              </div>
            </template>

            <template #select-cell="{ row }">
              <div class="flex justify-center">
                <UCheckbox :model-value="selectedIds.includes(row.original.id)" @update:model-value="toggleRowSelection(row.original.id, $event)" />
              </div>
            </template>

            <template #createdAt-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('createdAt')">
                วันที่สร้างใบงาน
                <UIcon :name="sortIcon('createdAt')" class="size-3.5" />
              </UButton>
            </template>

            <template #batchRef-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('batchRef')">
                Group ID
                <UIcon :name="sortIcon('batchRef')" class="size-3.5" />
              </UButton>
            </template>

            <template #jobRef-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('jobRef')">
                Job ID
                <UIcon :name="sortIcon('jobRef')" class="size-3.5" />
              </UButton>
            </template>

            <template #salesOrderNumber-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('salesOrderNumber')">
                SO
                <UIcon :name="sortIcon('salesOrderNumber')" class="size-3.5" />
              </UButton>
            </template>

            <template #invoiceNumber-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('invoiceNumber')">
                Invoice
                <UIcon :name="sortIcon('invoiceNumber')" class="size-3.5" />
              </UButton>
            </template>

            <template #shipTo-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('shipTo')">
                Ship To
                <UIcon :name="sortIcon('shipTo')" class="size-3.5" />
              </UButton>
            </template>

            <template #plannedDeliveryDate-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('plannedDeliveryDate')">
                Ship Date
                <UIcon :name="sortIcon('plannedDeliveryDate')" class="size-3.5" />
              </UButton>
            </template>

            <template #viewState-header>
              <UButton color="neutral" variant="ghost" :class="tableHeaderButtonClass" @click="toggleSort('viewState')">
                State
                <UIcon :name="sortIcon('viewState')" class="size-3.5" />
              </UButton>
            </template>

            <template #batchRef-cell="{ row }">
                <div :class="['font-black', headingClass]">{{ row.original.batchRef }}</div>
              </template>

            <template #jobRef-cell="{ row }">
              <div :class="['font-black', headingClass]">{{ row.original.jobRef }}</div>
            </template>

            <template #viewState-cell="{ row }">
              <UBadge color="neutral" variant="soft" class="rounded-full px-3 py-1 text-xs font-black uppercase tracking-[0.14em]" :class="getStateBadgeClass(row.original.viewState)">
                {{ row.original.viewStateLabel }}
              </UBadge>
            </template>

            <template #actions-cell="{ row }">
              <div class="flex justify-end">
                <UButton
                  color="neutral"
                  variant="ghost"
                  icon="i-lucide-trash-2"
                  :class="isDarkMode
                    ? 'rounded-full text-rose-300 hover:bg-rose-500/15 hover:text-rose-200'
                    : 'rounded-full text-rose-600 hover:bg-rose-50 hover:text-rose-700'"
                  @click="deleteSingleJob(row.original.id)"
                />
              </div>
            </template>
          </UTable>
        </div>

        <div class="mt-4 flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
          <div class="flex flex-wrap items-center gap-3">
            <div :class="['text-sm font-medium', bodyTextClass]">
              เลือกอยู่ {{ selectedCount }} รายการ • แสดง {{ paginatedRows.length }} จาก {{ filteredRows.length }} รายการ
            </div>
            <div class="flex items-center gap-2">
              <span :class="['text-sm font-semibold', bodyTextClass]">รายการต่อหน้า</span>
              <USelectMenu
                :model-value="pageSize"
                :items="pageSizeOptions"
                value-key="value"
                label-key="label"
                color="neutral"
                variant="outline"
                :search-input="false"
                class="w-32"
                :ui="adminSelectUi"
                @update:model-value="updatePageSize"
              />
            </div>
          </div>

          <UPagination
            v-model:page="currentPage"
            :total="filteredRows.length"
            :items-per-page="pageSize"
            :show-edges="true"
            :sibling-count="1"
            color="info"
            variant="outline"
            active-color="info"
            active-variant="solid"
            size="sm"
            class="justify-end"
          />
        </div>
      </UCard>

      <UModal v-model:open="isCreateModalOpen" :ui="{ content: 'sm:max-w-4xl' }">
        <template #content>
          <UCard :ui="modalCardUi">
            <template #header>
              <div class="flex flex-col gap-3 sm:flex-row sm:items-start sm:justify-between">
                <div>
                  <div class="text-xs font-black uppercase tracking-[0.2em] text-emerald-600">Create QC Job</div>
                  <h3 :class="['mt-1 text-2xl font-black', headingClass]">สร้างชุดใบงาน QC</h3>
                </div>

                <div :class="['flex items-center gap-2 text-xs font-black uppercase tracking-[0.16em]', workspaceLabelClass]">
                  <UIcon name="i-lucide-calendar-clock" class="size-4" />
                  <span>{{ currentDateTimeLabel }}</span>
                </div>
              </div>
            </template>

            <div class="grid gap-4 md:grid-cols-2">
              <UFormField label="Create Date" :ui="adminFieldUi">
                <UInput
                  :model-value="formatInputDateLabel(createCreatedDate)"
                  readonly
                  color="neutral"
                  variant="outline"
                  class="w-full"
                  :ui="adminGhostUi"
                />
              </UFormField>

              <UFormField label="SO" required :ui="adminFieldUi">
                <USelectMenu v-model="createSalesOrderId" :items="salesOrderOptions" value-key="value" label-key="label" color="neutral" variant="outline" placeholder="เลือก SO" :search-input="{ placeholder: 'Search SO...' }" class="w-full" :ui="adminSelectUi" />
              </UFormField>

              <UFormField label="Invoice" required :ui="adminFieldUi">
                <USelectMenu v-model="createInvoiceId" :items="invoiceOptions" value-key="value" label-key="label" color="neutral" variant="outline" placeholder="เลือก Invoice" :search-input="{ placeholder: 'Search invoice...' }" class="w-full" :ui="adminSelectUi" />
              </UFormField>

              <UFormField label="Ship To" required :ui="adminFieldUi">
                <USelectMenu v-model="createShipTo" :items="shipToOptions" value-key="value" label-key="label" color="neutral" variant="outline" :search-input="false" class="w-full" :ui="adminSelectUi" />
              </UFormField>

              <UFormField label="จำนวน Rack" required :ui="adminFieldUi">
                <UInputNumber
                  v-model="createRackCount"
                  :min="1"
                  :max="200"
                  :step="1"
                  color="neutral"
                  variant="outline"
                  placeholder="1"
                  class="w-full"
                  :increment="{ variant: 'solid', color: 'neutral', class: adminInputNumberButtonClass }"
                  :decrement="{ variant: 'solid', color: 'neutral', class: adminInputNumberButtonClass }"
                  :ui="adminInputNumberUi"
                />
              </UFormField>

              <UFormField label="Ship Date" :ui="adminFieldUi">
                <div class="relative">
                  <UInput
                    :model-value="formatInputDateLabel(createPlannedDeliveryDate)"
                    readonly
                    placeholder="เลือกวันส่ง"
                    color="neutral"
                    variant="outline"
                    class="w-full cursor-pointer"
                    :ui="{ ...adminInputUi, base: `${adminInputUi.base} pr-12` }"
                    @click="createShipDatePopoverOpen = true"
                  />

                  <UPopover v-model:open="createShipDatePopoverOpen">
                    <UButton
                      color="neutral"
                      variant="ghost"
                      icon="i-lucide-calendar-days"
                      :class="isDarkMode
                        ? 'absolute top-1/2 right-2 z-10 -translate-y-1/2 rounded-full text-slate-300 hover:bg-sky-500/15 hover:text-sky-200'
                        : 'absolute top-1/2 right-2 z-10 -translate-y-1/2 rounded-full text-slate-600 hover:bg-sky-50 hover:text-sky-900'"
                      @click="createShipDatePopoverOpen = true"
                    />

                    <template #content>
                      <UCalendar v-model="createCalendarValue" class="p-2" @update:model-value="createShipDatePopoverOpen = false" />
                    </template>
                  </UPopover>
                </div>
              </UFormField>
            </div>

            <template #footer>
              <div class="flex justify-end gap-3">
                <UButton color="neutral" variant="outline" :class="outlineActionButtonClass" @click="isCreateModalOpen = false">
                  Cancel
                </UButton>
                <UButton :loading="isBusy" :class="navPrimaryButtonClass" @click="createJobs">
                  Create QC Jobs
                </UButton>
              </div>
            </template>
          </UCard>
        </template>
      </UModal>

      <UModal v-model:open="isUpdateModalOpen" :ui="{ content: 'sm:max-w-4xl' }">
        <template #content>
          <UCard :ui="modalCardUi">
            <template #header>
              <div class="flex flex-col gap-3 sm:flex-row sm:items-start sm:justify-between">
                <div>
                  <div class="text-xs font-black uppercase tracking-[0.2em] text-sky-600">Update Selected</div>
                  <h3 :class="['mt-1 text-2xl font-black', headingClass]">อัปเดตใบงานที่เลือก</h3>
                  <p :class="['mt-2 text-sm', bodyTextClass]">จะอัปเดต {{ selectedCount }} รายการ{{ applyWholeGroup ? ' และขยายทั้ง Group ID' : '' }}</p>
                </div>

                <div :class="['flex items-center gap-2 text-xs font-black uppercase tracking-[0.16em]', workspaceLabelClass]">
                  <UIcon name="i-lucide-calendar-clock" class="size-4" />
                  <span>{{ currentDateTimeLabel }}</span>
                </div>
              </div>
            </template>

            <div class="grid gap-4 lg:grid-cols-2 xl:grid-cols-4">
              <UFormField label="SO" required :ui="adminFieldUi">
                <USelectMenu v-model="updateSalesOrderId" :items="salesOrderOptions" value-key="value" label-key="label" color="neutral" variant="outline" placeholder="เลือก SO" :search-input="{ placeholder: 'Search SO...' }" class="w-full" :ui="adminSelectUi" />
              </UFormField>

              <UFormField label="Invoice" required :ui="adminFieldUi">
                <USelectMenu v-model="updateInvoiceId" :items="invoiceOptions" value-key="value" label-key="label" color="neutral" variant="outline" placeholder="เลือก Invoice" :search-input="{ placeholder: 'Search invoice...' }" class="w-full" :ui="adminSelectUi" />
              </UFormField>

              <UFormField label="Ship To" required :ui="adminFieldUi">
                <USelectMenu v-model="updateShipTo" :items="shipToOptions" value-key="value" label-key="label" color="neutral" variant="outline" :search-input="false" class="w-full" :ui="adminSelectUi" />
              </UFormField>

              <UFormField label="Ship Date" :ui="adminFieldUi">
                <div class="relative">
                  <UInput
                    :model-value="formatInputDateLabel(updatePlannedDeliveryDate)"
                    readonly
                    placeholder="เลือกวันส่ง"
                    color="neutral"
                    variant="outline"
                    class="w-full cursor-pointer"
                    :ui="{ ...adminInputUi, base: `${adminInputUi.base} pr-12` }"
                    @click="updateShipDatePopoverOpen = true"
                  />

                  <UPopover v-model:open="updateShipDatePopoverOpen">
                    <UButton
                      color="neutral"
                      variant="ghost"
                      icon="i-lucide-calendar-days"
                      :class="isDarkMode
                        ? 'absolute top-1/2 right-2 z-10 -translate-y-1/2 rounded-full text-slate-300 hover:bg-sky-500/15 hover:text-sky-200'
                        : 'absolute top-1/2 right-2 z-10 -translate-y-1/2 rounded-full text-slate-600 hover:bg-sky-50 hover:text-sky-900'"
                      @click="updateShipDatePopoverOpen = true"
                    />

                    <template #content>
                      <UCalendar v-model="updateCalendarValue" class="p-2" @update:model-value="updateShipDatePopoverOpen = false" />
                    </template>
                  </UPopover>
                </div>
              </UFormField>
            </div>

            <template #footer>
              <div class="flex justify-end gap-3">
                <UButton color="neutral" variant="outline" :class="outlineActionButtonClass" @click="isUpdateModalOpen = false">
                  Cancel
                </UButton>
                <UButton :loading="isBusy" :class="navPrimaryButtonClass" @click="submitUpdateSelected">
                  Update Selected
                </UButton>
              </div>
            </template>
          </UCard>
        </template>
      </UModal>

      <UModal v-model:open="deleteConfirmOpen" :dismissible="!isBusy" :ui="{ content: 'sm:max-w-xl' }">
        <template #content>
          <UCard :ui="modalCardUi">
            <template #header>
              <div>
                <div class="text-xs font-black uppercase tracking-[0.2em] text-rose-600">Permanent Delete</div>
                <h3 :class="['mt-1 text-2xl font-black', headingClass]">ยืนยันการลบใบงาน</h3>
                <p :class="['mt-2 text-sm font-semibold', bodyTextClass]">
                  การลบนี้เป็นการลบจริงและกู้คืนไม่ได้
                </p>
              </div>
            </template>

            <div v-if="pendingDelete" class="space-y-4">
              <div :class="isDarkMode ? 'rounded-[12px] border border-rose-900/70 bg-rose-950/30 px-4 py-3 text-sm text-rose-100' : 'rounded-[12px] border border-rose-200 bg-rose-50 px-4 py-3 text-sm text-rose-950'">
                <span class="font-black">{{ pendingDelete.applyToGroups ? 'ลบทั้ง Group ID ของรายการที่เลือก' : 'ลบเฉพาะรายการด้านล่าง' }}</span>
              </div>

              <div class="max-h-56 space-y-2 overflow-y-auto pr-1">
                <div
                  v-for="job in pendingDelete.jobs"
                  :key="`${job.jobRef}-${job.rackId}`"
                  :class="isDarkMode ? 'rounded-[10px] border border-slate-700 bg-slate-800 px-4 py-3' : 'rounded-[10px] border border-slate-200 bg-slate-50 px-4 py-3'"
                >
                  <div :class="['font-black', headingClass]">{{ job.jobRef }}</div>
                  <div :class="['mt-1 text-sm', bodyTextClass]">Rack: {{ job.rackId || '-' }} · SO: {{ job.salesOrderNumber }}</div>
                </div>
              </div>
            </div>

            <template #footer>
              <div class="flex justify-end gap-3">
                <UButton color="neutral" variant="outline" :disabled="isBusy" :class="outlineActionButtonClass" @click="deleteConfirmOpen = false">
                  Cancel
                </UButton>
                <UButton
                  color="error"
                  variant="solid"
                  :loading="isBusy"
                  class="rounded-full bg-rose-600 px-4 text-sm font-black text-white hover:bg-rose-700"
                  @click="confirmDelete"
                >
                  Delete Permanently
                </UButton>
              </div>
            </template>
          </UCard>
        </template>
      </UModal>
    </section>
  </main>
</template>
