import { isAdminAuthConfigured, setAdminSession, verifyAdminPin } from '../../utils/admin-auth'

interface AdminLoginPayload {
  pin?: string | null
}

export default defineEventHandler(async (event) => {
  if (!isAdminAuthConfigured()) {
    throw createError({
      statusCode: 503,
      statusMessage: 'Admin PIN is not configured',
    })
  }

  const body = await readBody<AdminLoginPayload>(event)

  if (!verifyAdminPin(body.pin)) {
    throw createError({
      statusCode: 401,
      statusMessage: 'Invalid admin PIN',
    })
  }

  setAdminSession(event)

  return {
    ok: true,
  }
})
