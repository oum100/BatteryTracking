import { prisma } from '../../utils/prisma'
import { ensureRequiredText } from '../../utils/battery-jobs'

interface ChargeProgramPayload {
  code?: string
  name?: string
}

export default defineEventHandler(async (event) => {
  const body = await readBody<ChargeProgramPayload>(event)
  const code = ensureRequiredText(body.code, 'code').toUpperCase()
  const name = ensureRequiredText(body.name, 'name')

  const chargeProgram = await prisma.chargeProgram.upsert({
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
    chargeProgram,
  }
})
