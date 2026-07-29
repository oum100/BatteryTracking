import { prisma } from '../../utils/prisma'
import { requireAdminSession } from '../../utils/admin-auth'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)
  const users = await prisma.userAccount.findMany({
    select: {
      id: true,
      username: true,
      displayName: true,
      role: true,
      active: true,
      lastLoginAt: true,
      createdAt: true,
      updatedAt: true,
    },
    orderBy: [{ role: 'asc' }, { username: 'asc' }],
  })

  return { ok: true, users }
})
