<script setup lang="ts">
const props = defineProps<{
  label: string;
  detail: string;
  icon: string;
  theme: "before" | "after" | "delivery";
  queueCount: number | null;
}>();

const emit = defineEmits<{
  select: [];
}>();

const themeClass = computed(() => {
  if (props.theme === "before") {
    return "border-lime-500 bg-[linear-gradient(180deg,_#f4ffe7_0%,_#e5f7cf_100%)] text-lime-950";
  }

  if (props.theme === "after") {
    return "border-sky-500 bg-[linear-gradient(180deg,_#eef7ff_0%,_#dbeeff_100%)] text-sky-950";
  }

  return "border-amber-500 bg-[linear-gradient(180deg,_#fff5e7_0%,_#ffe6bf_100%)] text-amber-950";
});

const queueBadgeClass = computed(() => {
  if (props.theme === "before") {
    return "bg-lime-700 text-white ring-lime-800/20";
  }

  if (props.theme === "after") {
    return "bg-sky-700 text-white ring-sky-800/20";
  }

  return "bg-amber-700 text-white ring-amber-800/20";
});
</script>

<template>
  <button
    type="button"
    class="min-h-[180px] rounded-xl border-2 px-5 py-5 text-left shadow-[0_24px_60px_rgba(15,23,42,0.12)] transition-all duration-200 hover:-translate-y-1 hover:shadow-[0_30px_70px_rgba(15,23,42,0.16)] active:translate-y-0 sm:min-h-[210px] sm:px-7 sm:py-6"
    :class="themeClass"
    @click="emit('select')"
  >
    <div class="flex items-start justify-between gap-4 sm:gap-6">
      <div>
        <div
          class="text-[11px] font-black uppercase tracking-[0.18em] opacity-70 sm:text-xs sm:tracking-[0.22em]"
        >
          Select Mode
        </div>
        <div
          class="mt-2 inline-flex rounded-full px-2.5 py-1 text-[11px] font-black tracking-[0.08em] shadow-sm ring-1 sm:text-xs"
          :class="queueBadgeClass"
        >
          WAITING {{ queueCount ?? "-" }}
        </div>
      </div>
      <div
        class="flex h-14 w-14 shrink-0 items-center justify-center rounded-xl border border-current/15 bg-white/55 shadow-[0_12px_24px_rgba(15,23,42,0.08)] sm:h-[72px] sm:w-[72px]"
      >
        <UIcon :name="icon" class="size-8 sm:size-10" />
      </div>
    </div>
    <div
      class="mt-4 text-[1.2rem] font-black leading-[1.05] tracking-[-0.03em] sm:mt-5 sm:text-[1.85rem] lg:text-[1.95rem]"
    >
      {{ label }}
    </div>
    <div
      class="mt-2 text-[0.92rem] font-semibold leading-snug opacity-90 sm:mt-3 sm:text-[0.98rem] lg:text-[1rem]"
    >
      {{ detail }}
    </div>
  </button>
</template>
