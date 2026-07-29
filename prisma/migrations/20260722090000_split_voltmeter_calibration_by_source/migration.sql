-- Keep the existing calibration as INA226, then allow one record per source.
CREATE TYPE "VoltMeterCalibrationSource" AS ENUM ('INA226', 'INA228');

ALTER TABLE "VoltMeterCalibration"
  ADD COLUMN "source" "VoltMeterCalibrationSource" NOT NULL DEFAULT 'INA226';

DROP INDEX "VoltMeterCalibration_deviceId_key";

CREATE UNIQUE INDEX "VoltMeterCalibration_deviceId_source_key"
  ON "VoltMeterCalibration"("deviceId", "source");
