import { ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'
import { prisma } from '../../utils/prisma'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = ensureRequiredText(getRouterParam(event, 'id'), 'sales order ID')
  await prisma.salesOrder.update({ where: { id }, data: { active: false } })

  return { ok: true, deleted: true }
})
