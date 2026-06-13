<script setup lang="ts">
const workspaceLinks = [
  {
    label: 'Battery Movement V2',
    path: '/battery-movement',
    className: 'justify-between rounded-2xl bg-lime-600 px-5 py-5 text-left text-lg font-bold text-white hover:bg-lime-700',
  },
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

function getWorkspaceHref(path: string) {
  if (typeof window === 'undefined') {
    return path
  }

  const url = new URL(path, window.location.origin)
  const isAndroid = /Android/i.test(window.navigator.userAgent)

  if (!isAndroid) {
    return url.toString()
  }

  return `intent://${url.host}${url.pathname}${url.search}${url.hash}#Intent;scheme=${url.protocol.replace(':', '')};package=com.android.chrome;end`
}
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
        <div class="space-y-8">
          <div class="space-y-4">
            <UBadge class="rounded-full bg-slate-800 px-4 py-2 text-sm font-bold tracking-[0.22em] text-white">
              BatteryTracking
            </UBadge>

            <div class="space-y-2">
              <h1 class="text-4xl font-black tracking-tight text-slate-950 sm:text-5xl">
                Choose a workspace
              </h1>
              <p class="max-w-2xl text-base leading-7 text-slate-600 sm:text-lg">
                เปิดหน้างานที่ต้องใช้ได้ทันทีจาก 3 ส่วนหลักของระบบ
              </p>
            </div>
          </div>

          <div class="grid gap-4">
            <UButton
              v-for="link in workspaceLinks"
              :key="link.path"
              :href="getWorkspaceHref(link.path)"
              external
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
  </main>
</template>
