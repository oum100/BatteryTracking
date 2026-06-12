import { PrismaClient } from '@prisma/client'

const globalForPrisma = globalThis as typeof globalThis & {
  prisma?: PrismaClient
}

const TRANSIENT_CONNECTION_PATTERNS = [
  'Error in PostgreSQL connection: Error { kind: Closed, cause: None }',
  'Can\'t reach database server',
  'Timed out fetching a new connection from the connection pool',
]

function isTransientPrismaConnectionError(error: unknown) {
  if (!(error instanceof Error)) {
    return false
  }

  return TRANSIENT_CONNECTION_PATTERNS.some(pattern => error.message.includes(pattern))
}

function wait(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms))
}

function createPrismaClient() {
  const client = new PrismaClient({
    log: process.env.NODE_ENV === 'development' ? ['warn', 'error'] : ['error']
  })

  return client.$extends({
    query: {
      async $allOperations({ args, query }) {
        try {
          return await query(args)
        }
        catch (error) {
          if (!isTransientPrismaConnectionError(error)) {
            throw error
          }

          if (process.env.NODE_ENV === 'development') {
            console.warn('[prisma] transient connection issue detected, retrying once')
          }

          await client.$disconnect().catch(() => {})
          await wait(250)

          return query(args)
        }
      }
    }
  })
}

export const prisma =
  globalForPrisma.prisma ??
  createPrismaClient()

if (process.env.NODE_ENV !== 'production') {
  globalForPrisma.prisma = prisma
}
