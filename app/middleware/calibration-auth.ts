export default defineNuxtRouteMiddleware(async (to) => {
  try {
    const headers = process.server ? useRequestHeaders(['cookie']) : undefined
    const response = await $fetch<{ authenticated: boolean, user: { role: string } | null }>('/api/auth/session', { headers })
    if (response.authenticated && ['ADMIN', 'CALIBRATOR'].includes(response.user?.role || '')) {
      return
    }
  }
  catch {
    // Redirect below.
  }

  const redirect = encodeURIComponent(to.fullPath || '/battery-qc-calibration')
  return navigateTo(`/admin-login?redirect=${redirect}`)
})
