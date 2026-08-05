import { prisma } from '../../utils/prisma'
import { ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

interface InvoicePayload {
  invoiceNo?: string
  salesOrderId?: string
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const body = await readBody<InvoicePayload>(event)
  const invoiceNo = ensureRequiredText(body.invoiceNo, 'invoiceNo').toUpperCase()
  const salesOrderId = ensureRequiredText(body.salesOrderId, 'salesOrderId')
  const salesOrder = await prisma.salesOrder.findFirst({ where: { id: salesOrderId, active: true } })
  if (!salesOrder) {
    throw createError({ statusCode: 400, statusMessage: 'salesOrderId must reference an active SO' })
  }

  const invoice = await prisma.invoice.upsert({
    where: { invoiceNo },
    update: {
      salesOrderId,
      active: true,
    },
    create: {
      invoiceNo,
      salesOrderId,
      active: true,
    },
  })

  return {
    ok: true,
    invoice,
  }
})
