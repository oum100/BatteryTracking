-- Rename workStartedAt to openedAt while preserving existing values
ALTER TABLE "BatteryJob"
RENAME COLUMN "workStartedAt" TO "openedAt";
