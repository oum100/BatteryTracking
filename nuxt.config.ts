// https://nuxt.com/docs/api/configuration/nuxt-config
export default defineNuxtConfig({
  compatibilityDate: '2025-07-15',
  modules: ['@nuxt/ui'],
  css: ['~/assets/css/main.css'],
  runtimeConfig: {
    adminPin: process.env.ADMIN_PIN || '',
    adminSessionSecret: process.env.ADMIN_SESSION_SECRET || '',
  },
  devtools: { enabled: true },
  vite: {
    server: {
      allowedHosts: true
    }
  }
})
