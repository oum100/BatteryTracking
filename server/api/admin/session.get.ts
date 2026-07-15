import { isAdminAuthConfigured, isAdminSessionAuthenticated } from '../../utils/admin-auth'

export default defineEventHandler((event) => ({
  ok: true,
  configured: isAdminAuthConfigured(),
  authenticated: isAdminSessionAuthenticated(event),
}))
