import { PrismaClient } from '@prisma/client'
import {
  chargeChannelDefaults,
  chargeProgramDefaults,
  employeeDefaults,
  invoiceDefaults,
  salesOrderDefaults,
} from '../server/utils/master-data.ts'

const prisma = new PrismaClient()

async function syncEmployees() {
  const defaultCodes = employeeDefaults.map(employee => employee.code)

  await Promise.all(employeeDefaults.map(employee =>
    prisma.employee.upsert({
      where: { code: employee.code },
      update: {
        name: employee.name,
        active: employee.active,
      },
      create: {
        code: employee.code,
        name: employee.name,
        active: employee.active,
      },
    }),
  ))

  await prisma.employee.updateMany({
    where: {
      code: { notIn: defaultCodes },
      jobs: { none: {} },
      beforeChargeJobs: { none: {} },
      afterChargeJobs: { none: {} },
      beforeDeliveryJobs: { none: {} },
    },
    data: { active: false },
  })
}

async function syncSalesOrders() {
  const defaultValues = salesOrderDefaults.map(item => item.soNumber)

  await Promise.all(salesOrderDefaults.map(item =>
    prisma.salesOrder.upsert({
      where: { soNumber: item.soNumber },
      update: {
        description: item.description,
        active: item.active,
      },
      create: {
        soNumber: item.soNumber,
        description: item.description,
        active: item.active,
      },
    }),
  ))

  await prisma.salesOrder.updateMany({
    where: {
      soNumber: { notIn: defaultValues },
      jobs: { none: {} },
    },
    data: { active: false },
  })
}

async function syncInvoices() {
  const defaultValues = invoiceDefaults.map(item => item.invoiceNo)

  await Promise.all(invoiceDefaults.map(item =>
    prisma.invoice.upsert({
      where: { invoiceNo: item.invoiceNo },
      update: {
        description: item.description,
        active: item.active,
      },
      create: {
        invoiceNo: item.invoiceNo,
        description: item.description,
        active: item.active,
      },
    }),
  ))

  await prisma.invoice.updateMany({
    where: {
      invoiceNo: { notIn: defaultValues },
      jobs: { none: {} },
    },
    data: { active: false },
  })
}

async function syncChargeChannels() {
  const defaultValues = chargeChannelDefaults.map(item => item.code)

  await Promise.all(chargeChannelDefaults.map(item =>
    prisma.chargeChannel.upsert({
      where: { code: item.code },
      update: {
        name: item.name,
        active: item.active,
      },
      create: {
        code: item.code,
        name: item.name,
        active: item.active,
      },
    }),
  ))

  await prisma.chargeChannel.updateMany({
    where: {
      code: { notIn: defaultValues },
      jobs: { none: {} },
    },
    data: { active: false },
  })
}

async function syncChargePrograms() {
  const defaultValues = chargeProgramDefaults.map(item => item.code)

  await Promise.all(chargeProgramDefaults.map(item =>
    prisma.chargeProgram.upsert({
      where: { code: item.code },
      update: {
        name: item.name,
        active: item.active,
      },
      create: {
        code: item.code,
        name: item.name,
        active: item.active,
      },
    }),
  ))

  await prisma.chargeProgram.updateMany({
    where: {
      code: { notIn: defaultValues },
      jobs: { none: {} },
    },
    data: { active: false },
  })
}

async function main() {
  await syncEmployees()
  await syncSalesOrders()
  await syncInvoices()
  await syncChargeChannels()
  await syncChargePrograms()

  const counts = {
    employees: await prisma.employee.count({ where: { active: true } }),
    salesOrders: await prisma.salesOrder.count({ where: { active: true } }),
    invoices: await prisma.invoice.count({ where: { active: true } }),
    chargeChannels: await prisma.chargeChannel.count({ where: { active: true } }),
    chargePrograms: await prisma.chargeProgram.count({ where: { active: true } }),
    batteryJobs: await prisma.batteryJob.count(),
  }

  console.log('[seed] synced local master data')
  console.log(JSON.stringify(counts, null, 2))
}

main()
  .catch((error) => {
    console.error('[seed] failed', error)
    process.exitCode = 1
  })
  .finally(async () => {
    await prisma.$disconnect()
  })
