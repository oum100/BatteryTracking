-- Each sales order must own exactly one invoice. Existing ambiguous data is
-- rejected so a production migration cannot silently assign the wrong pair.
ALTER TABLE "SalesOrder" ADD COLUMN "invoiceId" TEXT;

WITH "SalesOrderInvoice" AS (
  SELECT "salesOrderId", MIN("invoiceId") AS "invoiceId"
  FROM "BatteryJob"
  WHERE "salesOrderId" IS NOT NULL AND "invoiceId" IS NOT NULL
  GROUP BY "salesOrderId"
  HAVING COUNT(DISTINCT "invoiceId") = 1
)
UPDATE "SalesOrder"
SET "invoiceId" = "SalesOrderInvoice"."invoiceId"
FROM "SalesOrderInvoice"
WHERE "SalesOrder"."id" = "SalesOrderInvoice"."salesOrderId";

DO $$
BEGIN
  IF EXISTS (SELECT 1 FROM "SalesOrder" WHERE "invoiceId" IS NULL) THEN
    RAISE EXCEPTION 'Every existing SalesOrder must have exactly one Invoice before this migration';
  END IF;

  IF EXISTS (
    SELECT "invoiceId"
    FROM "SalesOrder"
    GROUP BY "invoiceId"
    HAVING COUNT(*) > 1
  ) THEN
    RAISE EXCEPTION 'An Invoice cannot be linked to more than one SalesOrder';
  END IF;
END $$;

ALTER TABLE "SalesOrder" ALTER COLUMN "invoiceId" SET NOT NULL;
CREATE UNIQUE INDEX "SalesOrder_invoiceId_key" ON "SalesOrder"("invoiceId");
ALTER TABLE "SalesOrder" ADD CONSTRAINT "SalesOrder_invoiceId_fkey"
  FOREIGN KEY ("invoiceId") REFERENCES "Invoice"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

ALTER TABLE "BatteryJob" DROP CONSTRAINT IF EXISTS "BatteryJob_invoiceId_fkey";
ALTER TABLE "BatteryJob" DROP COLUMN "invoiceId";
