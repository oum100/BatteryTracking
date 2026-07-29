<script setup lang="ts">
const route = useRoute()
const username = ref('')
const pin = ref('')
const loading = ref(false)
const errorMessage = ref('')
const colorMode = useColorMode()

const redirectTarget = computed(() => {
  const raw = typeof route.query.redirect === 'string' ? route.query.redirect : '/battery-qc-admin'
  return raw.startsWith('/') ? raw : '/battery-qc-admin'
})

const isDarkMode = computed({
  get: () => colorMode.value === 'dark',
  set: value => colorMode.preference = value ? 'dark' : 'light',
})

const themeIcon = computed(() => isDarkMode.value ? 'i-lucide-sun-medium' : 'i-lucide-moon-star')
const pageBgClass = computed(() => isDarkMode.value
  ? 'min-h-screen bg-[radial-gradient(circle_at_top_left,_rgba(14,165,233,0.22),_transparent_32%),linear-gradient(180deg,_#07111f_0%,_#0f172a_58%,_#111827_100%)]'
  : 'min-h-screen bg-[radial-gradient(circle_at_top_left,_rgba(59,130,246,0.12),_transparent_24%),linear-gradient(180deg,_#d6e0eb_0%,_#cfd9e4_48%,_#c7d1dc_100%)]')
const pageTextClass = computed(() => isDarkMode.value ? 'text-slate-100' : 'text-slate-950')
const mutedTextClass = computed(() => isDarkMode.value ? 'text-slate-300' : 'text-slate-600')
const eyebrowClass = computed(() => isDarkMode.value ? 'text-sky-200' : 'text-sky-700')
const themeToggleButtonClass = computed(() => isDarkMode.value
  ? 'rounded-full border border-slate-600 bg-slate-900 text-slate-100 hover:bg-slate-800'
  : 'rounded-full border border-slate-400 bg-[#eef3f8] text-sky-900 hover:bg-[#e2eaf2]')
const cardUi = computed(() => ({
  root: isDarkMode.value
    ? 'w-full overflow-hidden rounded-[15px] border-0 bg-[linear-gradient(180deg,_#1f4262_0%,_#223a59_52%,_#1b2438_100%)] shadow-[0_24px_64px_rgba(2,6,23,0.45)]'
    : 'w-full overflow-hidden rounded-[15px] border-0 bg-[linear-gradient(180deg,_#c1d2e7_0%,_#cad8e8_52%,_#d4deea_100%)] shadow-[0_24px_64px_rgba(15,23,42,0.12)]',
  body: 'p-0',
}))
const panelClass = computed(() => isDarkMode.value
  ? 'bg-transparent'
  : 'bg-transparent')
const fieldUi = computed(() => ({
  root: 'w-full',
  label: `mb-1.5 text-sm font-bold ${isDarkMode.value ? 'text-slate-200' : 'text-slate-700'}`,
}))
const inputUi = computed(() => ({
  base: isDarkMode.value
    ? 'rounded-[8px] border border-slate-600 bg-slate-900 px-3 text-sm font-semibold text-slate-50 ring-1 ring-inset ring-slate-600 transition hover:border-sky-400 hover:ring-sky-400 focus:border-sky-400 focus:ring-2 focus:ring-inset focus:ring-sky-400'
    : 'rounded-[8px] border border-slate-500 bg-[#f6f9fc] px-3 text-sm font-semibold text-slate-950 ring-1 ring-inset ring-slate-400 transition hover:border-sky-600 hover:ring-sky-500 focus:border-sky-700 focus:ring-2 focus:ring-inset focus:ring-sky-500',
}))
const loginButtonClass = computed(() => isDarkMode.value
  ? 'rounded-full bg-sky-500 py-3 text-sm font-black text-slate-950 hover:bg-sky-400 disabled:cursor-not-allowed disabled:bg-slate-700 disabled:text-slate-400'
  : 'rounded-full bg-sky-700 py-3 text-sm font-black text-white hover:bg-sky-800 disabled:cursor-not-allowed disabled:bg-slate-300 disabled:text-slate-500')
const logoSrc = computed(() => isDarkMode.value
  ? '/branding/logo-puma-battery.png'
  : '/branding/logo-puma-battery-white-bg.png')
const messageAlertUi = computed(() => ({
  root: isDarkMode.value
    ? 'rounded-[15px] border border-sky-800 bg-sky-950/60 text-sky-50'
    : 'rounded-[15px] border border-sky-300 bg-[#e0f2fe] text-sky-950',
  title: 'text-sm font-black',
  description: isDarkMode.value ? 'text-sm font-semibold text-sky-100' : 'text-sm font-semibold text-sky-900',
}))
const errorAlertUi = computed(() => ({
  root: isDarkMode.value
    ? 'rounded-[15px] border border-rose-800 bg-rose-950/60 text-rose-50'
    : 'rounded-[15px] border border-rose-300 bg-[#ffe4e6] text-rose-950',
  title: 'text-sm font-black',
  description: isDarkMode.value ? 'text-sm font-semibold text-rose-100' : 'text-sm font-semibold text-rose-900',
}))

function getLoginErrorMessage(error: unknown) {
  const requestError = error as {
    statusCode?: number
    response?: { status?: number }
  }
  const statusCode = requestError.statusCode ?? requestError.response?.status

  if (statusCode === 401) {
    return 'Username หรือ PIN ไม่ถูกต้อง'
  }

  if (statusCode === 400) {
    return 'กรอก Username และ PIN ให้ถูกต้องก่อนเข้าสู่ระบบ'
  }

  return 'ไม่สามารถเข้าสู่ระบบได้ กรุณาลองใหม่อีกครั้ง'
}

async function loadSessionStatus() {
  const response = await $fetch<{ authenticated: boolean, user: { role: string } | null }>('/api/auth/session')

  if (response.authenticated) {
    const requiresAdmin = redirectTarget.value === '/battery-qc-admin' || redirectTarget.value === '/battery-qc-settings'
    if (requiresAdmin && response.user?.role !== 'ADMIN') {
      errorMessage.value = 'บัญชีนี้ไม่มีสิทธิ์เข้า Admin Workspace'
      return
    }

    await navigateTo(redirectTarget.value)
  }
}

async function submitAdminLogin() {
  if (!username.value.trim()) {
    errorMessage.value = 'กรอก Username ก่อน'
    return
  }

  if (!pin.value.trim()) {
    errorMessage.value = 'กรอก PIN ก่อน'
    return
  }

  loading.value = true
  errorMessage.value = ''

  try {
    await $fetch('/api/auth/login', {
      method: 'POST',
      body: {
        username: username.value,
        pin: pin.value,
      },
    })

    await navigateTo(redirectTarget.value)
  }
  catch (error) {
    errorMessage.value = getLoginErrorMessage(error)
  }
  finally {
    loading.value = false
  }
}

onMounted(async () => {
  try {
    await loadSessionStatus()
  }
  catch (error) {
    errorMessage.value = error instanceof Error ? error.message : 'Unable to verify admin session'
  }
})
</script>

<template>
  <main :class="[pageBgClass, `px-4 py-6 [font-family:'IBM_Plex_Sans_Thai','Inter',ui-sans-serif,sans-serif] sm:px-6 lg:px-8`]">
    <section class="mx-auto flex min-h-[88vh] max-w-[720px] items-center justify-center">
      <UCard :ui="cardUi">
        <div :class="[panelClass, 'min-h-[520px] p-6 sm:p-8 lg:p-10']">
          <div class="mx-auto w-full max-w-[460px] space-y-5">
            <div class="flex items-start justify-between gap-4">
              <div class="space-y-4">
                <div :class="['text-xs font-black uppercase tracking-[0.24em]', eyebrowClass]">Protected Access</div>
                <img :src="logoSrc" alt="PUMA Battery" class="h-16 w-auto object-contain sm:h-20" />
                <h1 :class="['text-4xl font-black tracking-tight sm:text-5xl', pageTextClass]">Battery QC Access</h1>
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

            <div class="space-y-2 pt-2">
              <div :class="['text-xs font-black uppercase tracking-[0.22em]', eyebrowClass]">Sign In</div>
              <h2 :class="['text-2xl font-black tracking-tight', pageTextClass]">เข้าสู่ระบบ</h2>
              <p :class="['text-sm font-semibold leading-6', mutedTextClass]">
                ใช้ Username และ PIN สำหรับ Admin หรือ Calibration Workspace
              </p>
            </div>

            <UAlert
              v-if="errorMessage"
              color="error"
              variant="soft"
              title="Login Error"
              :description="errorMessage"
              :ui="errorAlertUi"
            />

            <UFormField label="Username" name="username" required :ui="fieldUi">
              <UInput
                v-model="username"
                placeholder="เช่น ADMIN"
                color="neutral"
                variant="outline"
                size="md"
                class="w-full"
                :disabled="loading"
                :ui="inputUi"
                autocomplete="username"
                @keyup.enter="submitAdminLogin"
              />
            </UFormField>

            <UFormField label="PIN" name="admin-pin" required :ui="fieldUi">
              <UInput
                v-model="pin"
                type="password"
                placeholder="กรอก PIN"
                color="neutral"
                variant="outline"
                size="md"
                class="w-full"
                :disabled="loading"
                :ui="inputUi"
                @keyup.enter="submitAdminLogin"
              />
            </UFormField>

            <UButton
              color="neutral"
              variant="solid"
              block
              :loading="loading"
              :disabled="loading"
              :class="loginButtonClass"
              @click="submitAdminLogin"
            >
              Sign In
            </UButton>
          </div>
        </div>
      </UCard>
    </section>
  </main>
</template>
