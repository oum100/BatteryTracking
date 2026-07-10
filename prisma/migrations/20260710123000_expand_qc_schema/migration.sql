-- Rename palletId to rackId while preserving existing values
DROP INDEX IF EXISTS "BatteryJob_palletId_createdAt_idx";

ALTER TABLE "BatteryJob"
RENAME COLUMN "palletId" TO "rackId";

-- Extend BatteryJob master references
ALTER TABLE "BatteryJob"
ADD COLUMN "chargeChannelId" TEXT,
ADD COLUMN "chargeProgramId" TEXT,
ADD COLUMN "invoiceId" TEXT;

-- New master tables for QC workflow
CREATE TABLE "Invoice" (
    "id" TEXT NOT NULL,
    "invoiceNo" TEXT NOT NULL,
    "description" TEXT,
    "active" BOOLEAN NOT NULL DEFAULT true,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "Invoice_pkey" PRIMARY KEY ("id")
);

CREATE TABLE "ChargeChannel" (
    "id" TEXT NOT NULL,
    "code" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "active" BOOLEAN NOT NULL DEFAULT true,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "ChargeChannel_pkey" PRIMARY KEY ("id")
);

CREATE TABLE "ChargeProgram" (
    "id" TEXT NOT NULL,
    "code" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "active" BOOLEAN NOT NULL DEFAULT true,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "ChargeProgram_pkey" PRIMARY KEY ("id")
);

CREATE UNIQUE INDEX "Invoice_invoiceNo_key" ON "Invoice"("invoiceNo");
CREATE INDEX "Invoice_active_invoiceNo_idx" ON "Invoice"("active", "invoiceNo");

CREATE UNIQUE INDEX "ChargeChannel_code_key" ON "ChargeChannel"("code");
CREATE INDEX "ChargeChannel_active_code_idx" ON "ChargeChannel"("active", "code");
CREATE INDEX "ChargeChannel_active_name_idx" ON "ChargeChannel"("active", "name");

CREATE UNIQUE INDEX "ChargeProgram_code_key" ON "ChargeProgram"("code");
CREATE INDEX "ChargeProgram_active_code_idx" ON "ChargeProgram"("active", "code");
CREATE INDEX "ChargeProgram_active_name_idx" ON "ChargeProgram"("active", "name");

CREATE INDEX "BatteryJob_rackId_createdAt_idx" ON "BatteryJob"("rackId", "createdAt");
CREATE INDEX "BatteryJob_rackId_status_createdAt_idx" ON "BatteryJob"("rackId", "status", "createdAt");

ALTER TABLE "BatteryJob"
ADD CONSTRAINT "BatteryJob_invoiceId_fkey"
FOREIGN KEY ("invoiceId") REFERENCES "Invoice"("id") ON DELETE SET NULL ON UPDATE CASCADE;

ALTER TABLE "BatteryJob"
ADD CONSTRAINT "BatteryJob_chargeChannelId_fkey"
FOREIGN KEY ("chargeChannelId") REFERENCES "ChargeChannel"("id") ON DELETE SET NULL ON UPDATE CASCADE;

ALTER TABLE "BatteryJob"
ADD CONSTRAINT "BatteryJob_chargeProgramId_fkey"
FOREIGN KEY ("chargeProgramId") REFERENCES "ChargeProgram"("id") ON DELETE SET NULL ON UPDATE CASCADE;
