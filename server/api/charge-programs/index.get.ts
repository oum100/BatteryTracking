import { prisma } from '../../utils/prisma'

const defaults = [
  { code: 'PRG-1', name: 'Program 1' },
  { code: 'PRG-2', name: 'Program 2' },
  { code: 'PRG-3', name: 'Program 3' },
]

export default defineEventHandler(async () => {
  const count = await prisma.chargeProgram.count()

  if (count === 0) {
    await prisma.chargeProgram.createMany({
      data: defaults.map(item => ({ ...item, active: true })),
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
  }
})
