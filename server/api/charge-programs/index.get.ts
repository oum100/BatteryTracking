import { prisma } from '../../utils/prisma'
import { chargeProgramDefaults } from '../../utils/master-data'

export default defineEventHandler(async () => {
  try {
    const count = await prisma.chargeProgram.count()

    if (count === 0) {
      await prisma.chargeProgram.createMany({
        data: chargeProgramDefaults.map(({ id: _id, ...item }) => item),
        skipDuplicates: true,
      })
    }

    const chargePrograms = await prisma.chargeProgram.findMany({
      where: { active: true },
      orderBy: [{ code: 'asc' }, { name: 'asc' }],
      take: 500,
    })

    return {
      ok: true,
      chargePrograms,
      source: 'database',
    }
  }
  catch (error) {
    console.warn('[charge-programs] database unavailable, using fallback defaults', error)

    return {
      ok: true,
      chargePrograms: [...chargeProgramDefaults],
      source: 'fallback',
    }
  }
})
