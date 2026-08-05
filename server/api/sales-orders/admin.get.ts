import { prisma } from '../../utils/prisma'
import { requireAdminSession } from '../../utils/admin-auth'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const salesOrders = await prisma.salesOrder.findMany({
    include: { invoice: true },
    orderBy: { soNumber: 'asc' },
  })

  return { ok: true, salesOrders }
})
