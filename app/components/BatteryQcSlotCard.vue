<script setup lang="ts">
type SlotMetric = 'before' | 'after' | 'delivery'
type SlotCardSize = 'compact' | 'full'

const props = withDefaults(defineProps<{
  slotId: string | number
  battId?: string | null
  voltB?: string | number | null
  voltA?: string | number | null
  voltD?: string | number | null
  headerIcon?: string
  titleClass?: string
  bodyClass?: string
  borderClass?: string
  statusLabel?: string
  statusClass?: string
  activeMetric?: SlotMetric | null
  activeMetricClass?: string
  size?: SlotCardSize
}>(), {
  battId: '-',
  voltB: null,
  voltA: null,
  voltD: null,
  headerIcon: 'ph:car-battery',
  titleClass: 'bg-emerald-800 text-white',
  bodyClass: 'bg-emerald-50 text-slate-950',
  borderClass: 'border-emerald-700',
  statusLabel: '',
  statusClass: 'bg-white/20 text-white',
  activeMetric: null,
  activeMetricClass: 'bg-emerald-700 text-white',
  size: 'compact',
})

const sizeUi = computed(() => {
  if (props.size === 'full') {
    return {
      root: 'rounded-[2rem] border-[3px]',
      header: 'gap-5 px-6 py-4',
      iconWrap: 'h-14 w-14',
      icon: 'h-12 w-12',
      slotText: 'text-[2.3rem]',
      status: 'px-3 py-1 text-xs',
      body: 'px-8 pb-7 pt-6',
      battId: 'text-[3.8rem]',
      metrics: 'mt-7 gap-5',
      circle: 'h-[10.5rem] w-[10.5rem]',
      metricIcon: 'h-14 w-14',
      metricValue: 'mt-3 text-[3.25rem]',
    }
  }

  return {
    root: 'rounded-[1.1rem] border-2',
    header: 'gap-1.5 px-2.5 py-1.5',
    iconWrap: 'h-7 w-7',
    icon: 'h-5 w-5',
    slotText: 'text-[1rem]',
    status: 'px-2 py-0.5 text-[8px]',
    body: 'px-3 pb-2 pt-2',
    battId: 'text-[1.05rem]',
    metrics: 'mt-1 gap-2',
    circle: 'h-[4.25rem] w-[4.25rem]',
    metricIcon: 'h-6 w-6',
    metricValue: '-mt-0.5 text-[1.15rem]',
  }
})

function formatMetric(value: string | number | null | undefined) {
  if (value === null || value === undefined || value === '') {
    return '00.00'
  }

  const normalized = typeof value === 'number' ? value : Number(value)
  if (Number.isNaN(normalized)) {
    return String(value)
  }

  return normalized.toFixed(2)
}

function metricWrapperClass(metric: SlotMetric) {
  return props.activeMetric === metric
    ? props.activeMetricClass
    : 'bg-transparent text-current'
}

function metricValueClass(metric: SlotMetric) {
  return props.activeMetric === metric ? 'text-white' : 'text-current'
}
</script>

<template>
  <div
    class="overflow-hidden shadow-[0_14px_30px_rgba(15,23,42,0.10)] transition-all duration-150"
    :class="[sizeUi.root, borderClass, bodyClass]"
  >
    <div
      class="flex items-center"
      :class="[sizeUi.header, titleClass]"
    >
      <div class="flex items-center justify-center" :class="sizeUi.iconWrap">
        <UIcon :name="headerIcon" :class="sizeUi.icon" />
      </div>

      <div class="min-w-0 flex-1 text-center font-black tracking-[-0.04em]" :class="sizeUi.slotText">
        SLOT:{{ String(slotId).padStart(2, '0') }}
      </div>

      <div class="ml-auto flex min-w-[58px] justify-end">
        <div
          v-if="statusLabel"
          class="inline-flex rounded-full font-black uppercase tracking-[0.16em]"
          :class="[sizeUi.status, statusClass]"
        >
          {{ statusLabel }}
        </div>
      </div>
    </div>

    <div :class="sizeUi.body">
      <div class="truncate text-center font-medium leading-none tracking-[-0.05em] text-black" :class="sizeUi.battId">
        {{ battId || '-' }}
      </div>

      <div class="grid grid-cols-3" :class="sizeUi.metrics">
        <div class="flex flex-col items-center justify-end">
          <div class="flex items-center justify-center rounded-full" :class="[sizeUi.circle, metricWrapperClass('before')]">
            <div class="text-center">
              <UIcon name="i-lucide-battery-low" class="mx-auto" :class="sizeUi.metricIcon" />
              <div class="font-medium leading-none tracking-[-0.05em]" :class="[sizeUi.metricValue, metricValueClass('before')]">
                {{ formatMetric(voltB) }}
              </div>
            </div>
          </div>
        </div>

        <div class="flex flex-col items-center justify-end">
          <div class="flex items-center justify-center rounded-full" :class="[sizeUi.circle, metricWrapperClass('after')]">
            <div class="text-center">
              <UIcon name="i-lucide-battery-medium" class="mx-auto" :class="sizeUi.metricIcon" />
              <div class="font-medium leading-none tracking-[-0.05em]" :class="[sizeUi.metricValue, metricValueClass('after')]">
                {{ formatMetric(voltA) }}
              </div>
            </div>
          </div>
        </div>

        <div class="flex flex-col items-center justify-end">
          <div class="flex items-center justify-center rounded-full" :class="[sizeUi.circle, metricWrapperClass('delivery')]">
            <div class="text-center">
              <UIcon name="i-lucide-truck" class="mx-auto" :class="sizeUi.metricIcon" />
              <div class="font-medium leading-none tracking-[-0.05em]" :class="[sizeUi.metricValue, metricValueClass('delivery')]">
                {{ formatMetric(voltD) }}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
