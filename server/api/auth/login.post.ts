import { prisma } from '../../utils/prisma'
import { normalizeUsername, setUserSession, validatePin, verifyPin } from '../../utils/user-auth'

interface LoginPayload {
  username?: string
  pin?: string
}

export default defineEventHandler(async (event) => {
  const body = await readBody<LoginPayload>(event)
  const username = normalizeUsername(body.username)
  const pin = validatePin(body.pin)

  if (!username) {
    throw createError({ statusCode: 400, statusMessage: 'Username is required' })
  }

  const account = await prisma.userAccount.findUnique({ where: { username } })
  if (!account || !account.active || !verifyPin(pin, account.pinHash)) {
    throw createError({ statusCode: 401, statusMessage: 'Invalid username or PIN' })
  }

  await prisma.userAccount.update({
    where: { id: account.id },
    data: { lastLoginAt: new Date() },
  })
  setUserSession(event, account)

  return {
    ok: true,
    user: {
      username: account.username,
      displayName: account.displayName,
      role: account.role,
    },
  }
})
