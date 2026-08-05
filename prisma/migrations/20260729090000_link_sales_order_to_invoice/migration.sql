-- Preserve a legacy Invoice link only when it is unambiguous. An SO may exist
-- before its Invoice is issued, so an unmatched SO must remain valid.
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

CREATE UNIQUE INDEX "SalesOrder_invoiceId_key" ON "SalesOrder"("invoiceId");
ALTER TABLE "SalesOrder" ADD CONSTRAINT "SalesOrder_invoiceId_fkey"
  FOREIGN KEY ("invoiceId") REFERENCES "Invoice"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

ALTER TABLE "BatteryJob" DROP CONSTRAINT IF EXISTS "BatteryJob_invoiceId_fkey";
ALTER TABLE "BatteryJob" DROP COLUMN "invoiceId";
