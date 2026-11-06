import { clearUserSession, getCurrentUser } from '../../utils/user-auth'

export default defineEventHandler(async (event) => {
  let user = null

  try {
    user = await getCurrentUser(event)
  }
  catch {
    clearUserSession(event)
  }

  return {
    ok: true,
    authenticated: Boolean(user),
    user: user
      ? { username: user.username, displayName: user.displayName, role: user.role }
      : null,
  }
})
