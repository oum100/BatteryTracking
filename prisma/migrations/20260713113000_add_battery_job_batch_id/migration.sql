ALTER TABLE "BatteryJob"
ADD COLUMN "batchId" TEXT;

CREATE INDEX "BatteryJob_batchId_createdAt_idx" ON "BatteryJob"("batchId", "createdAt");
