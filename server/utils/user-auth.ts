import { createHmac, randomBytes, scryptSync, timingSafeEqual } from 'node:crypto'
import type { H3Event } from 'h3'
import { prisma } from './prisma'

export const USER_ROLES = ['ADMIN', 'CALIBRATOR'] as const
export type UserRole = typeof USER_ROLES[number]

const SESSION_COOKIE = 'battery_user_session'
const SESSION_TTL_SECONDS = 60 * 60 * 12

type SessionPayload = {
  userId: string
  role: UserRole
  exp: number
}

function toBase64Url(value: string) {
  return Buffer.from(value, 'utf8').toString('base64url')
}

function fromBase64Url(value: string) {
  return Buffer.from(value, 'base64url').toString('utf8')
}

function getSessionSecret() {
  return String(useRuntimeConfig().authSessionSecret || '').trim()
}

function signPayload(payload: string, secret: string) {
  return createHmac('sha256', secret).update(payload).digest('hex')
}

function isUserRole(value: unknown): value is UserRole {
  return typeof value === 'string' && USER_ROLES.includes(value as UserRole)
}

function verifySessionToken(token: string, secret: string): SessionPayload | null {
  const [payload, signature] = String(token || '').split('.')
  if (!payload || !signature) {
    return null
  }

  const expectedSignature = signPayload(payload, secret)
  const left = Buffer.from(signature, 'utf8')
  const right = Buffer.from(expectedSignature, 'utf8')
  if (left.length !== right.length || !timingSafeEqual(left, right)) {
    return null
  }

  try {
    const decoded = JSON.parse(fromBase64Url(payload)) as Partial<SessionPayload>
    if (!decoded.userId || !isUserRole(decoded.role) || typeof decoded.exp !== 'number' || decoded.exp <= Date.now()) {
      return null
    }

    return decoded as SessionPayload
  }
  catch {
    return null
  }
}

export function normalizeUsername(value: unknown) {
  return String(value ?? '').trim().toUpperCase().replace(/\s+/g, '')
}

export function validatePin(value: unknown) {
  const pin = String(value ?? '').trim()
  if (!/^\d{4,12}$/.test(pin)) {
    throw createError({ statusCode: 400, statusMessage: 'PIN must contain 4 to 12 digits' })
  }

  return pin
}

export function hashPin(pin: string) {
  const salt = randomBytes(16).toString('hex')
  const hash = scryptSync(pin, salt, 64).toString('hex')
  return `${salt}:${hash}`
}

export function verifyPin(pin: string, storedHash: string) {
  const [salt, expectedHash] = String(storedHash || '').split(':')
  if (!salt || !expectedHash) {
    return false
  }

  const derivedHash = scryptSync(pin, salt, 64).toString('hex')
  const left = Buffer.from(derivedHash, 'hex')
  const right = Buffer.from(expectedHash, 'hex')
  return left.length === right.length && timingSafeEqual(left, right)
}

export async function getCurrentUser(event: H3Event) {
  const secret = getSessionSecret()
  const token = getCookie(event, SESSION_COOKIE)
  const payload = secret && token ? verifySessionToken(token, secret) : null
  if (!payload) {
    return null
  }

  const account = await prisma.userAccount.findUnique({
    where: { id: payload.userId },
    select: { id: true, username: true, displayName: true, role: true, active: true },
  })

  if (!account || !account.active || account.role !== payload.role) {
    return null
  }

  return account
}

export async function requireUserRole(event: H3Event, roles: UserRole[]) {
  const user = await getCurrentUser(event)
  if (!user) {
    throw createError({ statusCode: 401, statusMessage: 'Authentication required' })
  }

  if (!roles.includes(user.role)) {
    throw createError({ statusCode: 403, statusMessage: 'You do not have permission for this workspace' })
  }

  return user
}

export function setUserSession(event: H3Event, user: { id: string, role: UserRole }) {
  const secret = getSessionSecret()
  if (!secret) {
    throw createError({ statusCode: 500, statusMessage: 'AUTH_SESSION_SECRET is not configured' })
  }

  const payload = toBase64Url(JSON.stringify({
    userId: user.id,
    role: user.role,
    exp: Date.now() + (SESSION_TTL_SECONDS * 1000),
  } satisfies SessionPayload))
  const token = `${payload}.${signPayload(payload, secret)}`

  setCookie(event, SESSION_COOKIE, token, {
    httpOnly: true,
    sameSite: 'lax',
    secure: process.env.NODE_ENV === 'production',
    path: '/',
    maxAge: SESSION_TTL_SECONDS,
  })
}

export function clearUserSession(event: H3Event) {
  deleteCookie(event, SESSION_COOKIE, { path: '/' })
}
