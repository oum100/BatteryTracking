import { prisma } from '../../utils/prisma'

export default defineEventHandler(async () => {
  try {
    const invoices = await prisma.invoice.findMany({
      where: { active: true },
      orderBy: { invoiceNo: 'asc' },
      take: 500,
    })

    return {
      ok: true,
      invoices,
      source: 'database',
    }
  }
  catch (error) {
    console.warn('[invoices] database unavailable', error)

    return {
      ok: true,
      invoices: [],
      source: 'fallback',
    }
  }
})
