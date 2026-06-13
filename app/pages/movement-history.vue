<script setup lang="ts">
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
  syncStatus?: 'db' | 'local'
}

const STORAGE_KEY = 'battery-movement-v1-records'
const search = ref('')
const selectedOperation = ref<'ALL' | MovementOperation>('ALL')
const isRefreshing = ref(false)
const loadError = ref('')
const localFallbackRecords = ref<MovementRecord[]>([])

const operationOptions = [
  { label: 'All', value: 'ALL' },
  { label: 'Stock to Charge', value: 'STOCK_TO_CHARGE' },
  { label: 'Charge to Delivery', value: 'CHARGE_TO_DELIVERY' },
  { label: 'Delivery Transfer', value: 'DELIVERY_TRANSFER' },
] as const

const operationMeta: Record<MovementOperation, { badge: string, card: string, chip: string }> = {
  STOCK_TO_CHARGE: {
    badge: 'bg-lime-100 text-lime-950 ring-lime-300/80',
    card: 'bg-lime-50/85',
    chip: 'bg-lime-200 text-lime-950',
  },
  CHARGE_TO_DELIVERY: {
    badge: 'bg-emerald-100 text-emerald-950 ring-emerald-300/80',
    card: 'bg-emerald-50/85',
    chip: 'bg-emerald-200 text-emerald-950',
  },
  DELIVERY_TRANSFER: {
    badge: 'bg-sky-100 text-sky-950 ring-sky-300/80',
    card: 'bg-sky-50/85',
    chip: 'bg-sky-200 text-sky-950',
  },
}

const { data, pending, refresh } = await useFetch<{ ok: boolean, records: Array<MovementRecord & { stage?: string }> }>('/api/battery-movements', {
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

const records = computed<MovementRecord[]>(() => {
  const dbRecords = (data.value?.records ?? []).map(record => ({
    ...record,
    operation: normalizeOperation(record.operation ?? record.stage),
    syncStatus: 'db' as const,
  }))

  const merged = [...localFallbackRecords.value, ...dbRecords]
  const seen = new Set<string>()

  return merged
    .filter((record) => {
      if (seen.has(record.id)) {
        return false
      }

      seen.add(record.id)
      return true
    })
    .sort((left, right) => new Date(right.createdAt).getTime() - new Date(left.createdAt).getTime())
})

const filteredRecords = computed(() => {
  const query = search.value.trim().toLowerCase()

  return records.value.filter((record) => {
    const matchesOperation = selectedOperation.value === 'ALL' || record.operation === selectedOperation.value

    if (!matchesOperation) {
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
    ].join(' ').toLowerCase()

    return haystack.includes(query)
  })
})

const totalCount = computed(() => records.value.length)

const operationCounts = computed(() => {
  return {
    STOCK_TO_CHARGE: records.value.filter(record => record.operation === 'STOCK_TO_CHARGE').length,
    CHARGE_TO_DELIVERY: records.value.filter(record => record.operation === 'CHARGE_TO_DELIVERY').length,
    DELIVERY_TRANSFER: records.value.filter(record => record.operation === 'DELIVERY_TRANSFER').length,
  }
})

async function reloadRecords() {
  isRefreshing.value = true
  loadError.value = ''

  try {
    loadLocalFallbackRecords()
    await refresh()
  }
  catch (error) {
    loadError.value = error instanceof Error ? error.message : 'Unable to load movement history'
  }
  finally {
    isRefreshing.value = false
  }
}

function loadLocalFallbackRecords() {
  if (typeof localStorage === 'undefined') {
    localFallbackRecords.value = []
    return
  }

  try {
    const raw = localStorage.getItem(STORAGE_KEY)
    const parsed = raw ? JSON.parse(raw) : []

    localFallbackRecords.value = Array.isArray(parsed)
          ? parsed.map((record: any) => ({
          ...record,
          operation: normalizeOperation(record.operation ?? record.stage),
          syncStatus: 'local' as const,
        }))
      : []
  }
  catch {
    localFallbackRecords.value = []
  }
}

function formatDateTime(value: string | null) {
  if (!value) {
    return '-'
  }

  return new Intl.DateTimeFormat('th-TH', {
    dateStyle: 'short',
    timeStyle: 'short',
  }).format(new Date(value))
}

function getOperationLabel(operation: MovementOperation) {
  const labels: Record<MovementOperation, string> = {
    STOCK_TO_CHARGE: 'Stock to Charge',
    CHARGE_TO_DELIVERY: 'Charge to Delivery',
    DELIVERY_TRANSFER: 'Delivery Transfer',
  }

  return labels[operation]
}

onMounted(() => {
  loadLocalFallbackRecords()
})
</script>

<template>
  <main class="min-h-[100dvh] bg-[radial-gradient(circle_at_top,rgba(190,242,100,0.45),transparent_28%),linear-gradient(180deg,#f7faef_0%,#eef6df_100%)] px-3 py-3 text-slate-950">
    <section class="mx-auto flex max-w-md flex-col gap-3">
      <UCard
        :ui="{
          root: 'rounded-md ring-0 bg-white/90 shadow-[0_18px_40px_rgba(15,23,42,0.08)]',
          body: 'px-3 py-3',
        }"
      >
        <div class="flex items-start justify-between gap-3">
          <div class="min-w-0">
            <UBadge class="rounded-md px-3 py-1 text-sm font-bold uppercase tracking-[0.18em] ring-1 bg-slate-100 text-slate-950 ring-slate-300/80">
              History
            </UBadge>
            <h1 class="mt-2 text-[1.35rem] leading-none font-black tracking-tight">
              Movement Records
            </h1>
            <p class="mt-1 text-sm text-slate-600">
              ดูรายการที่บันทึกเข้าฐานข้อมูลล่าสุด
            </p>
          </div>

          <div class="rounded-md border border-slate-200 bg-slate-50 px-3 py-2 text-right shadow-sm">
            <div class="text-[11px] font-bold uppercase tracking-[0.18em] text-slate-500">
              Total
            </div>
            <div class="text-xl leading-none font-black text-slate-950">
              {{ totalCount }}
            </div>
          </div>
        </div>
      </UCard>

      <div class="grid grid-cols-3 gap-2">
        <UCard :ui="{ root: 'rounded-md ring-0 bg-white/85 shadow-sm', body: 'px-2 py-2 text-center' }">
          <div class="text-[10px] font-bold uppercase tracking-[0.14em] text-slate-500">Stock</div>
          <div class="mt-1 text-lg font-black text-slate-950">{{ operationCounts.STOCK_TO_CHARGE }}</div>
        </UCard>
        <UCard :ui="{ root: 'rounded-md ring-0 bg-white/85 shadow-sm', body: 'px-2 py-2 text-center' }">
          <div class="text-[10px] font-bold uppercase tracking-[0.14em] text-slate-500">Charge</div>
          <div class="mt-1 text-lg font-black text-slate-950">{{ operationCounts.CHARGE_TO_DELIVERY }}</div>
        </UCard>
        <UCard :ui="{ root: 'rounded-md ring-0 bg-white/85 shadow-sm', body: 'px-2 py-2 text-center' }">
          <div class="text-[10px] font-bold uppercase tracking-[0.14em] text-slate-500">Transfer</div>
          <div class="mt-1 text-lg font-black text-slate-950">{{ operationCounts.DELIVERY_TRANSFER }}</div>
        </UCard>
      </div>

      <UCard
        :ui="{
          root: 'rounded-md ring-0 bg-white/88 shadow-[0_12px_28px_rgba(15,23,42,0.06)]',
          body: 'p-3',
        }"
      >
        <div class="grid gap-2">
          <UInput
            v-model="search"
            icon="i-lucide-search"
            placeholder="Search battery, rack, slot"
            :ui="{ base: '!min-h-10 !bg-white !px-3 !py-2 !text-sm !text-slate-950 !placeholder:text-slate-400' }"
          />

          <div class="grid grid-cols-4 gap-1">
            <UButton
              v-for="option in operationOptions"
              :key="option.value"
              size="sm"
              block
              color="neutral"
              variant="soft"
              :class="selectedOperation === option.value ? 'bg-slate-900 text-white hover:bg-slate-800' : 'border border-slate-300 bg-white text-slate-700 hover:bg-slate-50'"
              @click="selectedOperation = option.value"
            >
              {{ option.label }}
            </UButton>
          </div>

          <div class="grid grid-cols-2 gap-2">
            <UButton
              block
              color="neutral"
              variant="soft"
              icon="i-lucide-refresh-cw"
              :loading="isRefreshing"
              class="border border-slate-300 bg-white text-slate-700 hover:bg-slate-50"
              @click="reloadRecords"
            >
              Refresh
            </UButton>
            <NuxtLink to="/battery-movement" class="contents">
              <UButton
                block
                color="neutral"
                variant="soft"
                icon="i-lucide-scan-line"
                class="bg-lime-100 text-lime-950 hover:bg-lime-200"
              >
                Back To Scan
              </UButton>
            </NuxtLink>
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

      <UCard
        v-if="pending"
        :ui="{ root: 'rounded-md ring-0 bg-white/85 shadow-sm', body: 'px-3 py-4' }"
      >
        <div class="text-sm text-slate-600">Loading movement history...</div>
      </UCard>

      <UCard
        v-else-if="filteredRecords.length === 0"
        :ui="{ root: 'rounded-md ring-0 bg-white/85 shadow-sm', body: 'px-3 py-4' }"
      >
        <div class="text-sm font-semibold text-slate-950">No records found</div>
        <div class="mt-1 text-sm text-slate-600">
          ยังไม่มีข้อมูล หรือ filter/search นี้ยังไม่เจอรายการ
        </div>
      </UCard>

      <div v-else class="flex flex-col gap-2">
        <UCard
          v-for="record in filteredRecords"
          :key="record.id"
          :ui="{
            root: `rounded-md ring-0 shadow-[0_12px_28px_rgba(15,23,42,0.07)] ${operationMeta[record.operation].card}`,
            body: 'p-3',
          }"
        >
          <div class="flex items-start justify-between gap-3">
            <div class="min-w-0">
              <div class="flex flex-wrap items-center gap-2">
                <UBadge class="rounded-md px-2.5 py-1 text-[11px] font-bold uppercase tracking-[0.16em] ring-1" :class="operationMeta[record.operation].badge">
                  {{ getOperationLabel(record.operation) }}
                </UBadge>
                <UBadge
                  :class="record.syncStatus === 'local'
                    ? 'rounded-md bg-amber-100 px-2.5 py-1 text-[11px] font-bold uppercase tracking-[0.16em] text-amber-950 ring-1 ring-amber-300/80'
                    : 'rounded-md bg-slate-100 px-2.5 py-1 text-[11px] font-bold uppercase tracking-[0.16em] text-slate-950 ring-1 ring-slate-300/80'"
                >
                  {{ record.syncStatus === 'local' ? 'LOCAL' : 'DB' }}
                </UBadge>
              </div>
              <div class="mt-2 truncate text-base font-black text-slate-950">
                {{ record.batterySn }}
              </div>
              <div class="mt-1 text-[11px] text-slate-600">
                Saved {{ formatDateTime(record.createdAt) }}
              </div>
            </div>

            <div class="rounded-md px-2.5 py-2 text-center shadow-sm" :class="operationMeta[record.operation].chip">
              <div class="text-[10px] font-bold uppercase tracking-[0.14em]">Volt</div>
              <div class="mt-1 text-lg leading-none font-black">{{ record.voltage.toFixed(2) }}</div>
            </div>
          </div>

          <div class="mt-3 grid grid-cols-2 gap-2 text-[11px]">
            <div class="rounded-md bg-white/90 px-2.5 py-2 shadow-sm">
              <div class="text-slate-500">From Rack</div>
              <div class="mt-0.5 text-sm font-bold text-slate-950">{{ record.fromRack }}</div>
            </div>
            <div class="rounded-md bg-white/90 px-2.5 py-2 shadow-sm">
              <div class="text-slate-500">From Slot</div>
              <div class="mt-0.5 text-sm font-bold text-slate-950">{{ record.fromSlot }}</div>
            </div>
            <div class="rounded-md bg-white/90 px-2.5 py-2 shadow-sm">
              <div class="text-slate-500">To Rack</div>
              <div class="mt-0.5 text-sm font-bold text-slate-950">{{ record.toRack }}</div>
            </div>
            <div class="rounded-md bg-white/90 px-2.5 py-2 shadow-sm">
              <div class="text-slate-500">To Slot</div>
              <div class="mt-0.5 text-sm font-bold text-slate-950">{{ record.toSlot }}</div>
            </div>
          </div>

          <div class="mt-3 flex flex-wrap gap-2 text-[11px]">
            <span class="rounded-md bg-white/90 px-2 py-1 text-slate-700 shadow-sm">
              Scan: <strong class="text-slate-950">{{ record.scanSource || '-' }}</strong>
            </span>
            <span class="rounded-md bg-white/90 px-2 py-1 text-slate-700 shadow-sm">
              Voltage: <strong class="text-slate-950">{{ record.voltageSource || '-' }}</strong>
            </span>
            <span class="rounded-md bg-white/90 px-2 py-1 text-slate-700 shadow-sm">
              Device: <strong class="text-slate-950">{{ record.deviceId || '-' }}</strong>
            </span>
          </div>
        </UCard>
      </div>
    </section>
  </main>
</template>
