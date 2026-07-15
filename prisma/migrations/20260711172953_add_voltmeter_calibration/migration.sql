-- CreateTable
CREATE TABLE "VoltMeterCalibration" (
    "id" TEXT NOT NULL,
    "deviceId" TEXT NOT NULL,
    "deviceName" TEXT,
    "inaGain" DOUBLE PRECISION NOT NULL,
    "inaOffset" DOUBLE PRECISION NOT NULL,
    "pointOneMeter" DOUBLE PRECISION NOT NULL,
    "pointOneEsp" DOUBLE PRECISION NOT NULL,
    "pointTwoMeter" DOUBLE PRECISION NOT NULL,
    "pointTwoEsp" DOUBLE PRECISION NOT NULL,
    "syncedAt" TIMESTAMP(3),
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,

    CONSTRAINT "VoltMeterCalibration_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "VoltMeterCalibration_deviceId_key" ON "VoltMeterCalibration"("deviceId");

-- CreateIndex
CREATE INDEX "VoltMeterCalibration_deviceName_idx" ON "VoltMeterCalibration"("deviceName");

-- CreateIndex
CREATE INDEX "VoltMeterCalibration_updatedAt_idx" ON "VoltMeterCalibration"("updatedAt");
