import { ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'
import { prisma } from '../../utils/prisma'

interface InvoicePayload {
  invoiceNo?: string
  salesOrderId?: string
  active?: boolean
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = ensureRequiredText(getRouterParam(event, 'id'), 'invoice ID')
  const body = await readBody<InvoicePayload>(event)
  const salesOrderId = ensureRequiredText(body.salesOrderId, 'salesOrderId')
  const salesOrder = await prisma.salesOrder.findFirst({ where: { id: salesOrderId, active: true } })
  if (!salesOrder) {
    throw createError({ statusCode: 400, statusMessage: 'salesOrderId must reference an active SO' })
  }
  if (body.active === false) {
    const currentSalesOrder = await prisma.salesOrder.findFirst({ where: { invoice: { id }, active: true } })
    if (currentSalesOrder) {
      throw createError({
        statusCode: 409,
        statusMessage: `Invoice นี้ถูกผูกกับ SO ${currentSalesOrder.soNumber}; ปิดใช้งาน SO ก่อน`,
      })
    }
  }
  const invoice = await prisma.invoice.update({
    where: { id },
    data: {
      invoiceNo: ensureRequiredText(body.invoiceNo, 'invoiceNo').toUpperCase(),
      salesOrderId,
      ...(typeof body.active === 'boolean' ? { active: body.active } : {}),
    },
  })

  return { ok: true, invoice }
})
