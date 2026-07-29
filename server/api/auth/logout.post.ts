import { clearUserSession } from '../../utils/user-auth'

export default defineEventHandler((event) => {
  clearUserSession(event)
  return { ok: true }
})
