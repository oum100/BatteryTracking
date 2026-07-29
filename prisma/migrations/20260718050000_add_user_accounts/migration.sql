-- Accounts are intentionally limited to protected Admin and Calibration workspaces.
CREATE TYPE "UserRole" AS ENUM ('ADMIN', 'CALIBRATOR');

CREATE TABLE "UserAccount" (
  "id" TEXT NOT NULL,
  "username" TEXT NOT NULL,
  "displayName" TEXT NOT NULL,
  "pinHash" TEXT NOT NULL,
  "role" "UserRole" NOT NULL,
  "active" BOOLEAN NOT NULL DEFAULT true,
  "lastLoginAt" TIMESTAMP(3),
  "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
  "updatedAt" TIMESTAMP(3) NOT NULL,

  CONSTRAINT "UserAccount_pkey" PRIMARY KEY ("id")
);

CREATE UNIQUE INDEX "UserAccount_username_key" ON "UserAccount"("username");
CREATE INDEX "UserAccount_active_role_idx" ON "UserAccount"("active", "role");
