import 'dotenv/config'
import { randomBytes, scryptSync } from 'node:crypto'
import { PrismaClient } from '@prisma/client'

const username = String(process.env.BOOTSTRAP_ADMIN_USERNAME || 'ADMIN').trim().toUpperCase()
const displayName = String(process.env.BOOTSTRAP_ADMIN_NAME || 'QC Administrator').trim()
const pin = String(process.env.BOOTSTRAP_ADMIN_PIN || process.env.ADMIN_PIN || '').trim()

if (!/^\d{4,12}$/.test(pin)) {
  throw new Error('Set BOOTSTRAP_ADMIN_PIN (4-12 digits) before bootstrapping the first admin account.')
}

const salt = randomBytes(16).toString('hex')
const pinHash = `${salt}:${scryptSync(pin, salt, 64).toString('hex')}`
const prisma = new PrismaClient()

try {
  const user = await prisma.userAccount.upsert({
    where: { username },
    update: { displayName, pinHash, role: 'ADMIN', active: true },
    create: { username, displayName, pinHash, role: 'ADMIN', active: true },
  })
  console.log(`Admin account ${user.username} is ready.`)
}
finally {
  await prisma.$disconnect()
}
