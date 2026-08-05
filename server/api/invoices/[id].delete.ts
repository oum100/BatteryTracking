import { ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'
import { prisma } from '../../utils/prisma'

export default defineEventHandler(async (event) => {
  await requireAdminSession(event)

  const id = ensureRequiredText(getRouterParam(event, 'id'), 'invoice ID')
  const salesOrder = await prisma.salesOrder.findFirst({ where: { invoice: { id }, active: true } })
  if (salesOrder) {
    throw createError({
      statusCode: 409,
      statusMessage: `Invoice นี้ถูกผูกกับ SO ${salesOrder.soNumber}; ปิดใช้งาน SO ก่อน`,
    })
  }
  await prisma.invoice.update({ where: { id }, data: { active: false } })

  return { ok: true, deleted: true }
})
