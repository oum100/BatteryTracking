import { prisma } from '../../utils/prisma'

export default defineEventHandler(async () => {
  const [fromRacks, toRacks] = await Promise.all([
    prisma.batteryMovement.findMany({
      select: {
        fromRack: true,
      },
      distinct: ['fromRack'],
      orderBy: {
        fromRack: 'asc',
      },
    }),
    prisma.batteryMovement.findMany({
      select: {
        toRack: true,
      },
      distinct: ['toRack'],
      orderBy: {
        toRack: 'asc',
      },
    }),
  ])

  const racks = [...new Set([
    ...fromRacks.map(item => item.fromRack.trim()).filter(Boolean),
    ...toRacks.map(item => item.toRack.trim()).filter(Boolean),
  ])].sort((left, right) => left.localeCompare(right))

  return {
    ok: true,
    racks,
  }
})
