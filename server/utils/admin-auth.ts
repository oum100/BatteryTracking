import type { H3Event } from 'h3'
import { clearUserSession, getCurrentUser, requireUserRole } from './user-auth'

export async function isAdminSessionAuthenticated(event: H3Event) {
  const user = await getCurrentUser(event)
  return user?.role === 'ADMIN'
}

export async function requireAdminSession(event: H3Event) {
  return await requireUserRole(event, ['ADMIN'])
}

export function clearAdminSession(event: H3Event) {
  clearUserSession(event)
}
