import { prisma } from '../../utils/prisma'

const defaults = [
  { code: 'CH-A', name: 'Channel A' },
  { code: 'CH-B', name: 'Channel B' },
  { code: 'CH-C', name: 'Channel C' },
  { code: 'CH-D', name: 'Channel D' },
]

export default defineEventHandler(async () => {
  const count = await prisma.chargeChannel.count()

  if (count === 0) {
    await prisma.chargeChannel.createMany({
      data: defaults.map(item => ({ ...item, active: true })),
      skipDuplicates: true,
    })
  }

  const chargeChannels = await prisma.chargeChannel.findMany({
    where: { active: true },
    orderBy: [{ code: 'asc' }, { name: 'asc' }],
    take: 500,
  })

  return {
    ok: true,
    chargeChannels,
  }
})
