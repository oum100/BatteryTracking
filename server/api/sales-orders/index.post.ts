import { prisma } from '../../utils/prisma'
import { ensureOptionalText, ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

interface SalesOrderPayload {
  soNumber?: string
  description?: string
}

export default defineEventHandler(async (event) => {
  requireAdminSession(event)

  const body = await readBody<SalesOrderPayload>(event)
  const soNumber = ensureRequiredText(body.soNumber, 'soNumber').toUpperCase()
  const description = ensureOptionalText(body.description)

  const salesOrder = await prisma.salesOrder.upsert({
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

  return {
    ok: true,
    salesOrder,
  }
})
