export default defineNuxtRouteMiddleware(async (to) => {
  if (to.path === '/admin-login') {
    return
  }

  const requiredRoles = to.path === '/battery-qc-system'
    ? ['ADMIN', 'STAFF']
    : to.path === '/qc-operation'
      ? ['ADMIN', 'OFFICER']
      : ['/qc-report', '/qc-group-report'].includes(to.path)
        ? ['ADMIN', 'OFFICER']
      : ['ADMIN']

  try {
    const headers = process.server ? useRequestHeaders(['cookie']) : undefined
    const response = await $fetch<{ authenticated: boolean, user: { role: string } | null }>('/api/auth/session', { headers })
    if (response.authenticated && requiredRoles.includes(response.user?.role || '')) {
      return
    }
  }
  catch {
    // Redirect below.
  }

  const redirect = encodeURIComponent(to.fullPath || '/battery-qc-system')
  return navigateTo(`/admin-login?redirect=${redirect}`)
})
