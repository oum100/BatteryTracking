import { isAdminSessionAuthenticated } from '../../utils/admin-auth'

export default defineEventHandler(async (event) => ({
  ok: true,
  authenticated: await isAdminSessionAuthenticated(event),
}))
