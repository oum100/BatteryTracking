import { prisma } from '../../utils/prisma'
import { ensureOptionalText, ensureRequiredText } from '../../utils/battery-jobs'

interface InvoicePayload {
  invoiceNo?: string
  description?: string | null
}

export default defineEventHandler(async (event) => {
  const body = await readBody<InvoicePayload>(event)
  const invoiceNo = ensureRequiredText(body.invoiceNo, 'invoiceNo').toUpperCase()
  const description = ensureOptionalText(body.description)

  const invoice = await prisma.invoice.upsert({
    where: { invoiceNo },
    update: {
      description,
      active: true,
    },
    create: {
      invoiceNo,
      description,
      active: true,
    },
  })

  return {
    ok: true,
    invoice,
  }
})
