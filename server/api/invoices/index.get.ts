import { prisma } from '../../utils/prisma'
import { invoiceDefaults } from '../../utils/master-data'

export default defineEventHandler(async () => {
  try {
    const count = await prisma.invoice.count()

    if (count === 0) {
      await prisma.invoice.createMany({
        data: invoiceDefaults.map(({ id: _id, ...item }) => item),
        skipDuplicates: true,
      })
    }

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
    console.warn('[invoices] database unavailable, using fallback defaults', error)

    return {
      ok: true,
      invoices: [...invoiceDefaults],
      source: 'fallback',
    }
  }
})
