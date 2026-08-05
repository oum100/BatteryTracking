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
  const salesOrders = await prisma.salesOrder.findMany({
    where: { soNumber: { in: salesOrderDefaults.map(item => item.soNumber) } },
    select: { id: true, soNumber: true },
  })
  const salesOrderIdByNumber = new Map(salesOrders.map(salesOrder => [salesOrder.soNumber, salesOrder.id]))

  await Promise.all(invoiceDefaults.map((item, index) => {
    const salesOrderId = salesOrderIdByNumber.get(salesOrderDefaults[index]?.soNumber)
    if (!salesOrderId) {
      throw new Error(`Missing default SO for ${item.invoiceNo}`)
    }

    return (
    prisma.invoice.upsert({
      where: { invoiceNo: item.invoiceNo },
      update: {
        salesOrderId,
        active: item.active,
      },
      create: {
        invoiceNo: item.invoiceNo,
        salesOrderId,
        active: item.active,
      },
    }),
    )
  }))

  await prisma.invoice.updateMany({
    where: {
      invoiceNo: { notIn: defaultValues },
      salesOrder: null,
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

function createMockSlots(jobCode, includeAfterCharge) {
  const measuredAt = new Date()

  return Array.from({ length: 21 }, (_, index) => {
    const slotNumber = index + 1
    const beforeVoltage = Number((12.42 + (slotNumber % 5) * 0.01).toFixed(3))
    const afterVoltage = Number((13.18 + (slotNumber % 5) * 0.01).toFixed(3))

    return {
      slotNumber,
      batteryId: `${jobCode}-B${String(slotNumber).padStart(2, '0')}`,
      beforeVoltage,
      beforeVoltageMv: Math.round(beforeVoltage * 1000),
      beforeMeasuredAt: measuredAt,
      ...(includeAfterCharge
        ? {
            afterVoltage,
            afterVoltageMv: Math.round(afterVoltage * 1000),
            afterMeasuredAt: measuredAt,
          }
        : {}),
    }
  })
}

async function seedWorkflowMocks() {
  const [operator, salesOrder, invoice, chargeChannel, chargeProgram] = await Promise.all([
    prisma.employee.findFirst({ where: { active: true }, orderBy: { code: 'asc' } }),
    prisma.salesOrder.findFirst({ where: { active: true }, orderBy: { soNumber: 'asc' } }),
    prisma.invoice.findFirst({ where: { active: true }, orderBy: { invoiceNo: 'asc' } }),
    prisma.chargeChannel.findFirst({ where: { active: true }, orderBy: { code: 'asc' } }),
    prisma.chargeProgram.findFirst({ where: { active: true }, orderBy: { code: 'asc' } }),
  ])

  if (!operator || !salesOrder || !invoice || !chargeChannel || !chargeProgram) {
    throw new Error('Cannot create workflow mocks without master data')
  }

  await prisma.batteryJob.deleteMany({
    where: {
      OR: [
        { rackId: { startsWith: 'MOCK-P2-' } },
        { rackId: { startsWith: 'MOCK-P3-' } },
      ],
    },
  })

  const openedAt = new Date()
  const plannedDeliveryDate = new Date(openedAt)
  plannedDeliveryDate.setDate(plannedDeliveryDate.getDate() + 7)

  const createJob = (rackId, phase, status, completeAfterCharge) => prisma.batteryJob.create({
    data: {
      batchId: `MOCK-${phase}`,
      rackId,
      phase,
      status,
      openedAt,
      operatorId: operator.id,
      beforeChargeOperatorId: operator.id,
      ...(completeAfterCharge ? { afterChargeOperatorId: operator.id } : {}),
      salesOrderId: salesOrder.id,
      chargeChannelId: chargeChannel.id,
      chargeProgramId: chargeProgram.id,
      plannedDeliveryDate,
      shipTo: 'FTM',
      beforeChargeCompletedAt: openedAt,
      ...(completeAfterCharge ? { afterChargeCompletedAt: openedAt } : {}),
      notes: completeAfterCharge
        ? 'Mock job: QC After Charge completed, ready for QC Before Delivery.'
        : 'Mock job: QC Before Charge completed, ready for QC After Charge.',
      slots: {
        create: createMockSlots(rackId, completeAfterCharge),
      },
    },
  })

  await prisma.$transaction([
    ...Array.from({ length: 3 }, (_, index) => createJob(
      `MOCK-P2-${String(index + 1).padStart(2, '0')}`,
      'AFTER_CHARGE',
      'AFTER_CHARGING',
      false,
    )),
    ...Array.from({ length: 3 }, (_, index) => createJob(
      `MOCK-P3-${String(index + 1).padStart(2, '0')}`,
      'DELIVERY',
      'QC_FOR_DELIVERY',
      true,
    )),
  ])

  return {
    phase2Racks: ['MOCK-P2-01', 'MOCK-P2-02', 'MOCK-P2-03'],
    phase3Racks: ['MOCK-P3-01', 'MOCK-P3-02', 'MOCK-P3-03'],
  }
}

async function main() {
  await syncEmployees()
  await syncSalesOrders()
  await syncInvoices()
  await syncChargeChannels()
  await syncChargePrograms()
  const workflowMocks = await seedWorkflowMocks()

  const counts = {
    employees: await prisma.employee.count({ where: { active: true } }),
    salesOrders: await prisma.salesOrder.count({ where: { active: true } }),
    invoices: await prisma.invoice.count({ where: { active: true } }),
    chargeChannels: await prisma.chargeChannel.count({ where: { active: true } }),
    chargePrograms: await prisma.chargeProgram.count({ where: { active: true } }),
    batteryJobs: await prisma.batteryJob.count(),
  }

  console.log('[seed] synced local master data')
  console.log('[seed] workflow mocks created')
  console.log(JSON.stringify(workflowMocks, null, 2))
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
