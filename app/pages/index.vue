<script setup lang="ts">
const featuredLink = {
  label: 'Battery QC System',
  path: '/battery-qc-system',
  className: 'justify-between rounded-[24px] bg-[linear-gradient(135deg,_#b1003f_0%,_#d9044f_55%,_#f23c76_100%)] px-6 py-6 text-left text-lg font-bold text-white hover:brightness-105',
} as const

const qcWorkspaceLinks = [
  {
    label: 'Battery QC Admin',
    path: '/battery-qc-admin',
    className: 'justify-between rounded-[20px] border border-amber-200 bg-amber-50 px-5 py-4 text-left text-base font-bold text-amber-950 hover:bg-amber-100',
  },
  {
    label: 'QC Calibration',
    path: '/battery-qc-calibration',
    className: 'justify-between rounded-[20px] border border-violet-200 bg-violet-50 px-5 py-4 text-left text-base font-bold text-violet-950 hover:bg-violet-100',
  },
  {
    label: 'Battery QC Settings',
    path: '/battery-qc-settings',
    className: 'justify-between rounded-[20px] border border-sky-200 bg-sky-50 px-5 py-4 text-left text-base font-bold text-sky-950 hover:bg-sky-100',
  },
  {
    label: 'Battery QC Summary',
    path: '/qc-report',
    className: 'justify-between rounded-[20px] border border-rose-200 bg-rose-50 px-5 py-4 text-left text-base font-bold text-rose-950 hover:bg-rose-100',
  },
] as const

const operationLinks = [
  {
    label: 'Battery Movement V2',
    path: '/battery-movement',
    className: 'justify-between rounded-2xl bg-lime-600 px-5 py-5 text-left text-lg font-bold text-white hover:bg-lime-700',
  },
  {
    label: 'New Batt Job',
    path: '/new-batt-job',
    className: 'justify-between rounded-2xl bg-amber-600 px-5 py-5 text-left text-lg font-bold text-white hover:bg-amber-700',
  },
  {
    label: 'Battery Report System',
    path: '/new-batt-job',
    className: 'justify-between rounded-2xl bg-amber-500 px-5 py-5 text-left text-lg font-bold text-white hover:bg-amber-600',
  },
  {
    label: 'Batt Monitor BLE V2',
    path: '/batt-movement-ble',
    className: 'justify-between rounded-2xl bg-sky-700 px-5 py-5 text-left text-lg font-bold text-white hover:bg-sky-800',
  },
] as const

const historyLinks = [
  {
    label: 'Mobile History',
    path: '/movement-history',
    className: 'justify-between rounded-2xl bg-slate-800 px-5 py-5 text-left text-lg font-bold text-white hover:bg-slate-900',
  },
  {
    label: 'Desktop History',
    path: '/movement-desktop',
    className: 'justify-between rounded-2xl bg-slate-800 px-5 py-5 text-left text-lg font-bold text-white hover:bg-slate-900',
  },
] as const

</script>

<template>
  <main class="min-h-screen bg-[radial-gradient(circle_at_top,_rgba(217,249,157,0.5),_transparent_42%),linear-gradient(180deg,_#f7faef_0%,_#eef4e1_100%)] px-6 py-8 sm:px-8">
    <div class="mx-auto flex min-h-[calc(100vh-4rem)] w-full max-w-3xl items-center">
      <UCard
        class="w-full rounded-[28px] border-0 bg-white/92 shadow-[0_24px_70px_rgba(70,95,32,0.14)] ring-1 ring-lime-950/8 backdrop-blur"
        :ui="{
          body: 'p-6 sm:p-8',
          root: 'overflow-hidden'
        }"
      >
        <div class="space-y-6">
          <div class="space-y-4">
            <div class="flex items-center justify-between gap-4">
              <UBadge class="rounded-full bg-slate-800 px-4 py-2 text-sm font-bold tracking-[0.22em] text-white">
                BatteryTracking
              </UBadge>
              <p class="text-xs font-black uppercase tracking-[0.18em] text-slate-500">Version 1.0.5</p>
            </div>

            <div class="space-y-2">
              <h1 class="text-4xl font-black tracking-tight text-slate-950 sm:text-5xl">
                Choose a workspace
              </h1>
              <p class="max-w-2xl text-base leading-7 text-slate-600 sm:text-lg">
                เปิดหน้างานที่ต้องใช้ได้ทันทีจากส่วนหลักของระบบ
              </p>
            </div>
          </div>

          <div class="space-y-5">
            <div class="rounded-[26px] border border-rose-200 bg-white p-3 shadow-[0_18px_40px_rgba(190,24,93,0.08)]">
              <div class="mb-3 px-2">
                <div class="text-xs font-black uppercase tracking-[0.24em] text-rose-500">QC Workspace</div>
              </div>

              <UButton
                :to="featuredLink.path"
                block
                size="xl"
                color="neutral"
                variant="solid"
                :class="featuredLink.className"
                :ui="{
                  base: 'w-full justify-between',
                  leadingIcon: 'text-white',
                  trailingIcon: 'text-white',
                  label: 'text-white font-bold text-lg'
                }"
              >
                <span>{{ featuredLink.label }}</span>
                <UIcon name="i-lucide-arrow-right" class="size-6 shrink-0 text-white" />
              </UButton>

              <div class="mt-3 grid gap-3">
                <UButton
                  v-for="link in qcWorkspaceLinks"
                  :key="link.path"
                  :to="link.path"
                  block
                  size="xl"
                  color="neutral"
                  variant="solid"
                  :class="link.className"
                  :ui="{
                    base: 'w-full justify-between',
                    trailingIcon: 'text-current',
                    label: 'font-bold'
                  }"
                >
                  <span>{{ link.label }}</span>
                  <UIcon name="i-lucide-arrow-right" class="size-5 shrink-0" />
                </UButton>
              </div>
            </div>

            <div class="grid gap-5 lg:grid-cols-2">
              <UCard
                :ui="{
                  root: 'rounded-[24px] border-0 bg-slate-50 shadow-[0_18px_40px_rgba(15,23,42,0.06)] ring-1 ring-slate-200',
                  body: 'p-4'
                }"
              >
                <div class="space-y-4">
                  <div>
                    <div class="text-xs font-black uppercase tracking-[0.24em] text-slate-500">Operations</div>
                    <div class="mt-1 text-xl font-black text-slate-950">Daily Tools</div>
                  </div>

                  <div class="grid gap-3">
                    <UButton
                      v-for="link in operationLinks"
                      :key="link.path"
                      :to="link.path"
                      block
                      size="xl"
                      color="neutral"
                      variant="solid"
                      :class="link.className"
                      :ui="{
                        base: 'w-full justify-between',
                        leadingIcon: 'text-white',
                        trailingIcon: 'text-white',
                        label: 'text-white font-bold text-lg'
                      }"
                    >
                      <span>{{ link.label }}</span>
                      <UIcon name="i-lucide-arrow-right" class="size-6 shrink-0 text-white" />
                    </UButton>
                  </div>
                </div>
              </UCard>

              <UCard
                :ui="{
                  root: 'rounded-[24px] border-0 bg-slate-50 shadow-[0_18px_40px_rgba(15,23,42,0.06)] ring-1 ring-slate-200',
                  body: 'p-4'
                }"
              >
                <div class="space-y-4">
                  <div>
                    <div class="text-xs font-black uppercase tracking-[0.24em] text-slate-500">History</div>
                    <div class="mt-1 text-xl font-black text-slate-950">Review & Tracking</div>
                  </div>

                  <div class="grid gap-3">
                    <UButton
                      v-for="link in historyLinks"
                      :key="link.path"
                      :to="link.path"
                      block
                      size="xl"
                      color="neutral"
                      variant="solid"
                      :class="link.className"
                      :ui="{
                        base: 'w-full justify-between',
                        leadingIcon: 'text-white',
                        trailingIcon: 'text-white',
                        label: 'text-white font-bold text-lg'
                      }"
                    >
                      <span>{{ link.label }}</span>
                      <UIcon name="i-lucide-arrow-right" class="size-6 shrink-0 text-white" />
                    </UButton>
                  </div>
                </div>
              </UCard>
            </div>
          </div>
        </div>
      </UCard>
    </div>
  </main>
</template>
