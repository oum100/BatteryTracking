import { prisma } from '../../utils/prisma'

export default defineEventHandler(async () => {
  const result = await prisma.batteryMovement.deleteMany({})

  return {
    ok: true,
    deletedCount: result.count,
  }
})
