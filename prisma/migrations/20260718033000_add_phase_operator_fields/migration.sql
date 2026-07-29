-- Complete the per-phase QC assignment fields introduced in the application schema.
CREATE TYPE "ShipToFactory" AS ENUM ('AAT', 'FTM');

ALTER TABLE "BatteryJob"
ADD COLUMN "afterChargeOperatorId" TEXT,
ADD COLUMN "beforeChargeOperatorId" TEXT,
ADD COLUMN "beforeDeliveryOperatorId" TEXT,
ADD COLUMN "plannedDeliveryDate" TIMESTAMP(3),
ADD COLUMN "shipTo" "ShipToFactory";

CREATE INDEX "BatteryJob_plannedDeliveryDate_idx" ON "BatteryJob"("plannedDeliveryDate");
CREATE INDEX "BatteryJob_shipTo_idx" ON "BatteryJob"("shipTo");

ALTER TABLE "BatteryJob"
ADD CONSTRAINT "BatteryJob_beforeChargeOperatorId_fkey"
FOREIGN KEY ("beforeChargeOperatorId") REFERENCES "Employee"("id") ON DELETE SET NULL ON UPDATE CASCADE;

ALTER TABLE "BatteryJob"
ADD CONSTRAINT "BatteryJob_afterChargeOperatorId_fkey"
FOREIGN KEY ("afterChargeOperatorId") REFERENCES "Employee"("id") ON DELETE SET NULL ON UPDATE CASCADE;

ALTER TABLE "BatteryJob"
ADD CONSTRAINT "BatteryJob_beforeDeliveryOperatorId_fkey"
FOREIGN KEY ("beforeDeliveryOperatorId") REFERENCES "Employee"("id") ON DELETE SET NULL ON UPDATE CASCADE;
