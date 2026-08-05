import { prisma } from '../../utils/prisma'
import { ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = ensureRequiredText(getRouterParam(event, 'id'), 'employee ID')
  const employee = await prisma.employee.update({
    where: { id },
    data: { active: false },
  })

  return { ok: true, employee }
})
