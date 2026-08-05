import { prisma } from '../../utils/prisma'
import { requireAdminSession } from '../../utils/admin-auth'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const employees = await prisma.employee.findMany({
    orderBy: [{ active: 'desc' }, { code: 'asc' }],
  })

  return { ok: true, employees }
})
