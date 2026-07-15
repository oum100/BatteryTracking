import { createHmac, timingSafeEqual } from 'node:crypto'
import type { H3Event } from 'h3'

const ADMIN_SESSION_COOKIE = 'battery_admin_session'
const ADMIN_SESSION_TTL_MS = 1000 * 60 * 60 * 12

function toBase64Url(value: string) {
  return Buffer.from(value, 'utf8')
    .toString('base64')
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/g, '')
}

function fromBase64Url(value: string) {
  const normalized = value
    .replace(/-/g, '+')
    .replace(/_/g, '/')
  const padding = normalized.length % 4 === 0 ? '' : '='.repeat(4 - (normalized.length % 4))
  return Buffer.from(`${normalized}${padding}`, 'base64').toString('utf8')
}

function getAdminSecrets() {
  const config = useRuntimeConfig()
  const adminPin = String(config.adminPin || '').trim()
  const adminSessionSecret = String(config.adminSessionSecret || adminPin).trim()

  return {
    adminPin,
    adminSessionSecret,
  }
}

function signPayload(payload: string, secret: string) {
  return createHmac('sha256', secret).update(payload).digest('hex')
}

function buildAdminSessionToken(secret: string) {
  const payload = toBase64Url(JSON.stringify({
    role: 'admin',
    exp: Date.now() + ADMIN_SESSION_TTL_MS,
  }))
  const signature = signPayload(payload, secret)
  return `${payload}.${signature}`
}

function verifyAdminSessionToken(token: string, secret: string) {
  const [payload, signature] = String(token || '').split('.')
  if (!payload || !signature) {
    return false
  }

  const expectedSignature = signPayload(payload, secret)
  const left = Buffer.from(signature, 'utf8')
  const right = Buffer.from(expectedSignature, 'utf8')
  if (left.length !== right.length || !timingSafeEqual(left, right)) {
    return false
  }

  try {
    const decoded = JSON.parse(fromBase64Url(payload)) as { role?: string, exp?: number }
    return decoded.role === 'admin' && typeof decoded.exp === 'number' && decoded.exp > Date.now()
  }
  catch {
    return false
  }
}

export function isAdminAuthConfigured() {
  const { adminPin, adminSessionSecret } = getAdminSecrets()
  return Boolean(adminPin && adminSessionSecret)
}

export function isAdminSessionAuthenticated(event: H3Event) {
  const { adminSessionSecret } = getAdminSecrets()
  if (!adminSessionSecret) {
    return false
  }

  const token = getCookie(event, ADMIN_SESSION_COOKIE)
  if (!token) {
    return false
  }

  return verifyAdminSessionToken(token, adminSessionSecret)
}

export function requireAdminSession(event: H3Event) {
  if (!isAdminSessionAuthenticated(event)) {
    throw createError({
      statusCode: 401,
      statusMessage: 'Admin authentication required',
    })
  }
}

export function verifyAdminPin(pin: unknown) {
  const { adminPin } = getAdminSecrets()
  return Boolean(adminPin) && String(pin ?? '').trim() === adminPin
}

export function setAdminSession(event: H3Event) {
  const { adminSessionSecret } = getAdminSecrets()
  if (!adminSessionSecret) {
    throw createError({
      statusCode: 500,
      statusMessage: 'Admin session secret is not configured',
    })
  }

  setCookie(event, ADMIN_SESSION_COOKIE, buildAdminSessionToken(adminSessionSecret), {
    httpOnly: true,
    sameSite: 'lax',
    secure: process.env.NODE_ENV === 'production',
    path: '/',
    maxAge: ADMIN_SESSION_TTL_MS / 1000,
  })
}

export function clearAdminSession(event: H3Event) {
  deleteCookie(event, ADMIN_SESSION_COOKIE, {
    path: '/',
  })
}
