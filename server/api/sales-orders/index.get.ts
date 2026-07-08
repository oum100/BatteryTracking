import { prisma } from '../../utils/prisma'

export default defineEventHandler(async () => {
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
  }
})
