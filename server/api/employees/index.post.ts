import { prisma } from '../../utils/prisma'
import { ensureRequiredText } from '../../utils/battery-jobs'

interface EmployeePayload {
  code?: string
  name?: string
}

export default defineEventHandler(async (event) => {
  const body = await readBody<EmployeePayload>(event)
  const code = ensureRequiredText(body.code, 'code').toUpperCase()
  const name = ensureRequiredText(body.name, 'name')

  const employee = await prisma.employee.upsert({
    where: { code },
    update: {
      name,
      active: true,
    },
    create: {
      code,
      name,
      active: true,
    },
  })

  return {
    ok: true,
    employee,
  }
})
