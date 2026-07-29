import { prisma } from '../../utils/prisma'
import { normalizeUsername, setUserSession, validatePin, verifyPin } from '../../utils/user-auth'

interface AdminLoginPayload {
  username?: string | null
  pin?: string | null
}

export default defineEventHandler(async (event) => {
  const body = await readBody<AdminLoginPayload>(event)
  const username = normalizeUsername(body.username)
  const pin = validatePin(body.pin)
  const account = username
    ? await prisma.userAccount.findUnique({ where: { username } })
    : null

  if (!account || !account.active || account.role !== 'ADMIN' || !verifyPin(pin, account.pinHash)) {
    throw createError({ statusCode: 401, statusMessage: 'Invalid admin username or PIN' })
  }

  await prisma.userAccount.update({ where: { id: account.id }, data: { lastLoginAt: new Date() } })
  setUserSession(event, account)

  return { ok: true }
})
