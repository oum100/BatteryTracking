import { prisma } from '../../utils/prisma'

export default defineEventHandler(async () => {
  const employees = await prisma.employee.findMany({
    where: {
      active: true,
    },
    orderBy: [
      { name: 'asc' },
      { code: 'asc' },
    ],
    take: 500,
  })

  return {
    ok: true,
    employees,
  }
})
