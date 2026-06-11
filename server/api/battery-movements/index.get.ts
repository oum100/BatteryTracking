import { prisma } from '../../utils/prisma'

export default defineEventHandler(async () => {
  const records = await prisma.batteryMovement.findMany({
    orderBy: {
      createdAt: 'desc',
    },
    take: 100,
  })

  return {
    ok: true,
    records,
  }
})
