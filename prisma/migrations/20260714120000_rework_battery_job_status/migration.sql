ALTER TABLE "BatteryJob"
ALTER COLUMN "status" DROP DEFAULT;

ALTER TABLE "BatteryJob"
ALTER COLUMN "status" TYPE TEXT
USING ("status"::TEXT);

UPDATE "BatteryJob" AS job
SET "status" = CASE
  WHEN job."lockedAt" IS NOT NULL OR job."status" = 'READY_FOR_DELIVERY' THEN 'SHIPPED'
  WHEN job."afterChargeCompletedAt" IS NOT NULL THEN 'QC_FOR_DELIVERY'
  WHEN job."beforeChargeCompletedAt" IS NOT NULL THEN 'AFTER_CHARGING'
  WHEN EXISTS (
    SELECT 1
    FROM "BatteryJobSlot" AS slot
    WHERE slot."jobId" = job."id"
      AND (
        COALESCE(slot."batteryId", '') <> ''
        OR slot."beforeVoltage" IS NOT NULL
        OR slot."beforeMeasuredAt" IS NOT NULL
      )
  ) THEN 'BEFORE_CHARGING'
  ELSE 'NEW_JOB'
END;

ALTER TYPE "public"."BatteryJobStatus" RENAME TO "BatteryJobStatus_old";

CREATE TYPE "public"."BatteryJobStatus" AS ENUM (
  'NEW_JOB',
  'BEFORE_CHARGING',
  'AFTER_CHARGING',
  'QC_FOR_DELIVERY',
  'SHIPPED'
);

ALTER TABLE "BatteryJob"
ALTER COLUMN "status" TYPE "public"."BatteryJobStatus"
USING ("status"::"public"."BatteryJobStatus");

ALTER TABLE "BatteryJob"
ALTER COLUMN "status" SET DEFAULT 'NEW_JOB';

DROP TYPE "public"."BatteryJobStatus_old";
