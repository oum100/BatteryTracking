import { prisma } from "../../utils/prisma";
import {
  hashPin,
  normalizeUsername,
  requireUserRole,
  USER_ROLES,
  validatePin,
} from "../../utils/user-auth";

interface CreateUserPayload {
  username?: string;
  displayName?: string;
  pin?: string;
  useDefaultPin?: boolean;
  role?: string;
}

export default defineEventHandler(async (event) => {
  await requireUserRole(event, ["ADMIN"]);
  const body = await readBody<CreateUserPayload>(event);
  const username = normalizeUsername(body.username);
  const displayName = String(body.displayName ?? "").trim();
  const defaultPin = String(useRuntimeConfig().defaultUserPin || "").trim();
  const pin = body.useDefaultPin
    ? validatePin(defaultPin)
    : validatePin(body.pin);
  const role = String(body.role ?? "").trim();

  if (!username || !displayName) {
    throw createError({
      statusCode: 400,
      statusMessage: "Username and display name are required",
    });
  }
  if (!USER_ROLES.includes(role as (typeof USER_ROLES)[number])) {
    throw createError({ statusCode: 400, statusMessage: "Invalid user role" });
  }

  const user = await prisma.userAccount.create({
    data: {
      username,
      displayName,
      pinHash: hashPin(pin),
      role: role as (typeof USER_ROLES)[number],
    },
    select: {
      id: true,
      username: true,
      displayName: true,
      role: true,
      active: true,
      lastLoginAt: true,
      createdAt: true,
      updatedAt: true,
    },
  });

  return { ok: true, user };
});
