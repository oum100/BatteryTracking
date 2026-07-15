import { prisma } from '../../utils/prisma'
import { employeeDefaults } from '../../utils/master-data'

export default defineEventHandler(async () => {
  try {
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
      source: 'database',
    }
  }
  catch (error) {
    console.warn('[employees] database unavailable, using fallback defaults', error)

    return {
      ok: true,
      employees: [...employeeDefaults],
      source: 'fallback',
    }
  }
})
