-- An SO is created before its Invoice. Move the one-to-one relation to Invoice
-- so every Invoice can reference at most one SalesOrder while an SO can wait
-- for its Invoice to be issued.
ALTER TABLE "Invoice" ADD COLUMN "salesOrderId" TEXT;

UPDATE "Invoice"
SET "salesOrderId" = "SalesOrder"."id"
FROM "SalesOrder"
WHERE "SalesOrder"."invoiceId" = "Invoice"."id";

ALTER TABLE "SalesOrder" DROP CONSTRAINT IF EXISTS "SalesOrder_invoiceId_fkey";
DROP INDEX IF EXISTS "SalesOrder_invoiceId_key";
ALTER TABLE "SalesOrder" DROP COLUMN "invoiceId";

CREATE UNIQUE INDEX "Invoice_salesOrderId_key" ON "Invoice"("salesOrderId");
CREATE INDEX "Invoice_salesOrderId_idx" ON "Invoice"("salesOrderId");
ALTER TABLE "Invoice" ADD CONSTRAINT "Invoice_salesOrderId_fkey"
  FOREIGN KEY ("salesOrderId") REFERENCES "SalesOrder"("id") ON DELETE RESTRICT ON UPDATE CASCADE;
