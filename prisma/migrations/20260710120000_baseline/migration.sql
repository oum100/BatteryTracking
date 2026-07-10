-- CreateSchema
CREATE SCHEMA IF NOT EXISTS "public";

-- CreateEnum
CREATE TYPE "public"."BatteryJobPhase" AS ENUM ('BEFORE_CHARGE', 'AFTER_CHARGE', 'DELIVERY');

-- CreateEnum
CREATE TYPE "public"."BatteryJobStatus" AS ENUM ('OPEN', 'BEFORE_CHARGE_COMPLETED', 'AFTER_CHARGE_COMPLETED', 'READY_FOR_DELIVERY');

-- CreateEnum
CREATE TYPE "public"."MovementStage" AS ENUM ('STOCK_TO_CHARGE', 'CHARGE_TO_DELIVERY', 'DELIVERY_TRANSFER');

-- CreateTable
CREATE TABLE "public"."Battery" (
    "id" TEXT NOT NULL,
    "code" TEXT NOT NULL,
    "label" TEXT NOT NULL,
    "chemistry" TEXT,
    "capacityMah" INTEGER,
    "nominalVolt" DOUBLE PRECISION,
    "cycleCount" INTEGER NOT NULL DEFAULT 0,
    "healthPercent" INTEGER,
    "status" TEXT NOT NULL DEFAULT 'IN_STOCK',
    "location" TEXT,
    "notes" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "Battery_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."BatteryJob" (
    "id" TEXT NOT NULL,
    "phase" "public"."BatteryJobPhase" NOT NULL DEFAULT 'BEFORE_CHARGE',
    "status" "public"."BatteryJobStatus" NOT NULL DEFAULT 'OPEN',
    "palletId" TEXT NOT NULL,
    "workStartedAt" TIMESTAMP(3) NOT NULL,
    "operatorId" TEXT,
    "salesOrderId" TEXT,
    "beforeChargeCompletedAt" TIMESTAMP(3),
    "afterChargeCompletedAt" TIMESTAMP(3),
    "deliveryCompletedAt" TIMESTAMP(3),
    "lockedAt" TIMESTAMP(3),
    "notes" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "BatteryJob_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."BatteryJobSlot" (
    "id" TEXT NOT NULL,
    "jobId" TEXT NOT NULL,
    "slotNumber" INTEGER NOT NULL,
    "batteryId" TEXT,
    "beforeVoltage" DOUBLE PRECISION,
    "beforeVoltageMv" INTEGER,
    "beforeMeasuredAt" TIMESTAMP(3),
    "afterVoltage" DOUBLE PRECISION,
    "afterVoltageMv" INTEGER,
    "afterMeasuredAt" TIMESTAMP(3),
    "deliveryVoltage" DOUBLE PRECISION,
    "deliveryVoltageMv" INTEGER,
    "deliveryMeasuredAt" TIMESTAMP(3),
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "BatteryJobSlot_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."BatteryMovement" (
    "id" TEXT NOT NULL,
    "stage" "public"."MovementStage" NOT NULL,
    "fromRack" TEXT NOT NULL,
    "fromSlot" TEXT NOT NULL,
    "batterySn" TEXT NOT NULL,
    "voltage" DOUBLE PRECISION NOT NULL,
    "voltageMeasuredAt" TIMESTAMP(3),
    "toRack" TEXT NOT NULL,
    "toSlot" TEXT NOT NULL,
    "scanSource" TEXT,
    "voltageSource" TEXT,
    "deviceId" TEXT,
    "notes" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,
    "firmwareVersion" TEXT,

    CONSTRAINT "BatteryMovement_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."Employee" (
    "id" TEXT NOT NULL,
    "code" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "active" BOOLEAN NOT NULL DEFAULT true,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "Employee_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."SalesOrder" (
    "id" TEXT NOT NULL,
    "soNumber" TEXT NOT NULL,
    "description" TEXT,
    "active" BOOLEAN NOT NULL DEFAULT true,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "SalesOrder_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "Battery_code_key" ON "public"."Battery"("code" ASC);

-- CreateIndex
CREATE INDEX "BatteryJob_palletId_createdAt_idx" ON "public"."BatteryJob"("palletId" ASC, "createdAt" ASC);

-- CreateIndex
CREATE INDEX "BatteryJob_status_phase_createdAt_idx" ON "public"."BatteryJob"("status" ASC, "phase" ASC, "createdAt" ASC);

-- CreateIndex
CREATE INDEX "BatteryJobSlot_jobId_slotNumber_idx" ON "public"."BatteryJobSlot"("jobId" ASC, "slotNumber" ASC);

-- CreateIndex
CREATE UNIQUE INDEX "BatteryJobSlot_jobId_slotNumber_key" ON "public"."BatteryJobSlot"("jobId" ASC, "slotNumber" ASC);

-- CreateIndex
CREATE INDEX "BatteryMovement_batterySn_createdAt_idx" ON "public"."BatteryMovement"("batterySn" ASC, "createdAt" ASC);

-- CreateIndex
CREATE INDEX "BatteryMovement_stage_createdAt_idx" ON "public"."BatteryMovement"("stage" ASC, "createdAt" ASC);

-- CreateIndex
CREATE INDEX "Employee_active_name_idx" ON "public"."Employee"("active" ASC, "name" ASC);

-- CreateIndex
CREATE UNIQUE INDEX "Employee_code_key" ON "public"."Employee"("code" ASC);

-- CreateIndex
CREATE INDEX "SalesOrder_active_soNumber_idx" ON "public"."SalesOrder"("active" ASC, "soNumber" ASC);

-- CreateIndex
CREATE UNIQUE INDEX "SalesOrder_soNumber_key" ON "public"."SalesOrder"("soNumber" ASC);

-- AddForeignKey
ALTER TABLE "public"."BatteryJob" ADD CONSTRAINT "BatteryJob_operatorId_fkey" FOREIGN KEY ("operatorId") REFERENCES "public"."Employee"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."BatteryJob" ADD CONSTRAINT "BatteryJob_salesOrderId_fkey" FOREIGN KEY ("salesOrderId") REFERENCES "public"."SalesOrder"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."BatteryJobSlot" ADD CONSTRAINT "BatteryJobSlot_jobId_fkey" FOREIGN KEY ("jobId") REFERENCES "public"."BatteryJob"("id") ON DELETE CASCADE ON UPDATE CASCADE;

