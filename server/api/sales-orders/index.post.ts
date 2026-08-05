import { prisma } from '../../utils/prisma'
import { ensureOptionalText, ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

interface SalesOrderPayload {
  soNumber?: string
  description?: string
  invoiceNo?: string
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const body = await readBody<SalesOrderPayload>(event)
  const soNumber = ensureRequiredText(body.soNumber, 'soNumber').toUpperCase()
  const description = ensureOptionalText(body.description)
  const invoiceNo = ensureOptionalText(body.invoiceNo)?.toUpperCase()
  const salesOrder = await prisma.$transaction(async (tx) => {
    const result = await tx.salesOrder.upsert({
      where: { soNumber },
      update: {
        description,
        active: true,
      },
      create: {
        soNumber,
        description,
        active: true,
      },
    })

    if (invoiceNo) {
      await tx.invoice.create({
        data: { invoiceNo, salesOrderId: result.id, active: true },
      })
    }

    return result
  })

  return {
    ok: true,
    salesOrder,
  }
})
