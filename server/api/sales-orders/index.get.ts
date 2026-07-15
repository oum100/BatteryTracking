import { prisma } from '../../utils/prisma'
import { salesOrderDefaults } from '../../utils/master-data'

export default defineEventHandler(async () => {
  try {
    const salesOrders = await prisma.salesOrder.findMany({
      where: {
        active: true,
      },
      orderBy: {
        soNumber: 'asc',
      },
      take: 500,
    })

    return {
      ok: true,
      salesOrders,
      source: 'database',
    }
  }
  catch (error) {
    console.warn('[sales-orders] database unavailable, using fallback defaults', error)

    return {
      ok: true,
      salesOrders: [...salesOrderDefaults],
      source: 'fallback',
    }
  }
})
