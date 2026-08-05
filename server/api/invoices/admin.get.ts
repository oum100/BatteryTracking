import { prisma } from '../../utils/prisma'
import { requireAdminSession } from '../../utils/admin-auth'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const invoices = await prisma.invoice.findMany({
    include: { salesOrder: true },
    orderBy: { invoiceNo: 'asc' },
  })

  return { ok: true, invoices }
})
