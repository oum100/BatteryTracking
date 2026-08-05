import { ensureOptionalText, ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'
import { prisma } from '../../utils/prisma'

interface SalesOrderPayload {
  soNumber?: string
  description?: string | null
  active?: boolean
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = ensureRequiredText(getRouterParam(event, 'id'), 'sales order ID')
  const body = await readBody<SalesOrderPayload>(event)
  const salesOrder = await prisma.salesOrder.update({
    where: { id },
    data: {
      soNumber: ensureRequiredText(body.soNumber, 'soNumber').toUpperCase(),
      description: ensureOptionalText(body.description),
      ...(typeof body.active === 'boolean' ? { active: body.active } : {}),
    },
  })

  return { ok: true, salesOrder }
})
