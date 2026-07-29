import { getCurrentUser } from '../../utils/user-auth'

export default defineEventHandler(async (event) => {
  const user = await getCurrentUser(event)
  return {
    ok: true,
    authenticated: Boolean(user),
    user: user
      ? { username: user.username, displayName: user.displayName, role: user.role }
      : null,
  }
})
