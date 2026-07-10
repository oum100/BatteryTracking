import { prisma } from '../../utils/prisma'

const defaults = [
  { invoiceNo: '6907017', description: 'Invoice 6907017' },
  { invoiceNo: '6907018', description: 'Invoice 6907018' },
  { invoiceNo: '6907019', description: 'Invoice 6907019' },
  { invoiceNo: '6907020', description: 'Invoice 6907020' },
  { invoiceNo: '6907021', description: 'Invoice 6907021' },
]

export default defineEventHandler(async () => {
  const count = await prisma.invoice.count()

  if (count === 0) {
    await prisma.invoice.createMany({
      data: defaults.map(item => ({
        invoiceNo: item.invoiceNo,
        description: item.description,
        active: true,
      })),
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
  }
})
