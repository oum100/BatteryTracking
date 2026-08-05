import { prisma } from '../../utils/prisma'
import { ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

interface EmployeePayload {
  code?: string
  name?: string
  active?: boolean
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = ensureRequiredText(getRouterParam(event, 'id'), 'employee ID')
  const body = await readBody<EmployeePayload>(event)
  const employee = await prisma.employee.update({
    where: { id },
    data: {
      code: ensureRequiredText(body.code, 'code').toUpperCase(),
      name: ensureRequiredText(body.name, 'name'),
      ...(typeof body.active === 'boolean' ? { active: body.active } : {}),
    },
  })

  return { ok: true, employee }
})
