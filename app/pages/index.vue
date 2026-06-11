<template>
  <main class="mx-auto flex min-h-screen max-w-6xl items-center px-6 py-16 sm:px-10 lg:px-12">
    <section class="grid w-full gap-10 lg:grid-cols-[1.2fr_0.8fr] lg:items-center">
      <div class="space-y-8">
        <div class="inline-flex items-center gap-3 rounded-full border border-lime-900/10 bg-white/70 px-4 py-2 text-sm font-medium text-lime-950 shadow-sm backdrop-blur">
          <span class="h-2.5 w-2.5 rounded-full bg-lime-500" />
          BatteryTracking Starter
        </div>

        <div class="space-y-4">
          <p class="text-sm font-semibold uppercase tracking-[0.2em] text-lime-800/80">
            Nuxt 4 + Prisma + Tailwind v4
          </p>
          <h1 class="max-w-3xl text-5xl font-semibold tracking-tight text-slate-950 sm:text-6xl">
            Track packs, cells, cycles, and maintenance in one calm workspace.
          </h1>
          <p class="max-w-2xl text-lg leading-8 text-slate-700">
            The project is scaffolded and ready for us to build inventory, health history, charging logs, and service flows on top of a typed full-stack base.
          </p>
        </div>

        <div class="flex flex-wrap gap-3 text-sm font-medium">
          <span class="rounded-full bg-slate-950 px-4 py-2 text-white">Nuxt 4</span>
          <span class="rounded-full bg-white px-4 py-2 text-slate-900 ring-1 ring-slate-900/10">@prisma/client 6.19.2</span>
          <span class="rounded-full bg-white px-4 py-2 text-slate-900 ring-1 ring-slate-900/10">tailwindcss ^4.2.2</span>
        </div>

        <div class="flex flex-wrap gap-3">
          <NuxtLink
            to="/camera-test"
            class="rounded-full bg-slate-950 px-5 py-3 text-sm font-medium text-white"
          >
            Camera Test
          </NuxtLink>
          <NuxtLink
            to="/battery-transfer"
            class="rounded-full bg-lime-500 px-5 py-3 text-sm font-semibold text-lime-950"
          >
            Battery Transfer Mock
          </NuxtLink>
          <NuxtLink
            to="/battery-movement"
            class="rounded-full bg-sky-500 px-5 py-3 text-sm font-semibold text-sky-950"
          >
            Battery Movement V2
          </NuxtLink>
        </div>
      </div>

      <aside class="rounded-[2rem] border border-white/70 bg-white/80 p-6 shadow-[0_24px_80px_rgba(68,87,45,0.14)] backdrop-blur">
        <div class="grid gap-4 sm:grid-cols-2">
          <div class="rounded-2xl bg-lime-50 p-5">
            <p class="text-sm text-lime-900/70">Data layer</p>
            <p class="mt-2 text-2xl font-semibold text-lime-950">Prisma ready</p>
            <p class="mt-3 text-sm leading-6 text-lime-950/75">
              Starter schema includes batteries, cycle counts, and status fields.
            </p>
          </div>

          <div class="rounded-2xl bg-slate-950 p-5 text-white">
            <p class="text-sm text-white/70">UI layer</p>
            <p class="mt-2 text-2xl font-semibold">Tailwind v4</p>
            <p class="mt-3 text-sm leading-6 text-white/75">
              PostCSS and the global stylesheet are already wired into Nuxt.
            </p>
          </div>

          <div class="rounded-2xl border border-slate-200 p-5 sm:col-span-2">
            <p class="text-sm text-slate-500">Suggested next build steps</p>
            <ul class="mt-3 space-y-3 text-sm leading-6 text-slate-700">
              <li>Define battery inventory fields and pack hierarchy.</li>
              <li>Create API routes for intake, charge logs, and maintenance history.</li>
              <li>Build dashboards for health status, stock movement, and aging packs.</li>
            </ul>
          </div>
        </div>
      </aside>
    </section>
  </main>
</template>
