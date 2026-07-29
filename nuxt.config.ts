// https://nuxt.com/docs/api/configuration/nuxt-config
export default defineNuxtConfig({
  compatibilityDate: "2025-07-15",
  modules: ["@nuxt/ui"],
  css: ["~/assets/css/main.css"],
  runtimeConfig: {
    authSessionSecret:
      process.env.AUTH_SESSION_SECRET || process.env.ADMIN_SESSION_SECRET || "",
    defaultUserPin: process.env.DEFAULT_USER_PIN || process.env.ADMIN_PIN || "",
  },
  devtools: { enabled: true },
  vite: {
    server: {
      allowedHosts: true,
    },
  },
});
