import { prisma } from '../../utils/prisma'
import { ensureRequiredText } from '../../utils/battery-jobs'
import { requireAdminSession } from '../../utils/admin-auth'

interface ChargeChannelPayload {
  code?: string
  name?: string
}

export default defineEventHandler(async (event) => {
  requireAdminSession(event)

  const body = await readBody<ChargeChannelPayload>(event)
  const code = ensureRequiredText(body.code, 'code').toUpperCase()
  const name = ensureRequiredText(body.name, 'name')

  const chargeChannel = await prisma.chargeChannel.upsert({
    where: { code },
    update: {
      name,
      active: true,
    },
    create: {
      code,
      name,
      active: true,
    },
  })

  return {
    ok: true,
    chargeChannel,
  }
})
