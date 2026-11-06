export default defineNuxtRouteMiddleware(async (to) => {
  if (to.path === '/admin-login') {
    return
  }

  try {
    const headers = process.server ? useRequestHeaders(['cookie']) : undefined
    const response = await $fetch<{ authenticated: boolean }>('/api/admin/session', {
      headers,
    })

    if (response.authenticated) {
      return
    }
  }
  catch {
    // Redirect below.
  }

  const redirect = encodeURIComponent(to.fullPath || '/qc-operation')
  return navigateTo(`/admin-login?redirect=${redirect}`)
})
