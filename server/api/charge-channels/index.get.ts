import { prisma } from '../../utils/prisma'
import { chargeChannelDefaults } from '../../utils/master-data'

export default defineEventHandler(async () => {
  try {
    const count = await prisma.chargeChannel.count()

    if (count === 0) {
      await prisma.chargeChannel.createMany({
        data: chargeChannelDefaults.map(({ id: _id, ...item }) => item),
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
      source: 'database',
    }
  }
  catch (error) {
    console.warn('[charge-channels] database unavailable, using fallback defaults', error)

    return {
      ok: true,
      chargeChannels: [...chargeChannelDefaults],
      source: 'fallback',
    }
  }
})
