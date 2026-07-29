import { prisma } from '../../utils/prisma'
import { hashPin, normalizeUsername, requireUserRole, USER_ROLES, validatePin } from '../../utils/user-auth'

interface UpdateUserPayload {
  username?: string
  displayName?: string
  pin?: string
  role?: string
  active?: boolean
}

export default defineEventHandler(async (event) => {
  const currentUser = await requireUserRole(event, ['ADMIN'])
  const id = String(getRouterParam(event, 'id') ?? '').trim()
  const body = await readBody<UpdateUserPayload>(event)
  if (!id) {
    throw createError({ statusCode: 400, statusMessage: 'User ID is required' })
  }

  const data: Record<string, unknown> = {}
  if (body.username !== undefined) data.username = normalizeUsername(body.username)
  if (body.displayName !== undefined) data.displayName = String(body.displayName ?? '').trim()
  if (body.role !== undefined) {
    const role = String(body.role).trim()
    if (!USER_ROLES.includes(role as typeof USER_ROLES[number])) {
      throw createError({ statusCode: 400, statusMessage: 'Invalid user role' })
    }
    data.role = role
  }
  if (body.active !== undefined) data.active = Boolean(body.active)
  if (body.pin !== undefined && String(body.pin).trim()) data.pinHash = hashPin(validatePin(body.pin))

  if (!Object.keys(data).length) {
    throw createError({ statusCode: 400, statusMessage: 'No changes provided' })
  }
  if (id === currentUser.id && data.active === false) {
    throw createError({ statusCode: 400, statusMessage: 'You cannot deactivate your own account' })
  }

  const user = await prisma.userAccount.update({
    where: { id },
    data,
    select: { id: true, username: true, displayName: true, role: true, active: true, lastLoginAt: true, createdAt: true, updatedAt: true },
  })
  return { ok: true, user }
})
