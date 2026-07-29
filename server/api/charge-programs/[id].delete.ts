import { ensureRequiredText } from "../../utils/battery-jobs";
import { requireAdminSession } from "../../utils/admin-auth";
import { prisma } from "../../utils/prisma";

export default defineEventHandler(async (event) => {
  await requireAdminSession(event);

  const id = ensureRequiredText(getRouterParam(event, "id"), "program ID");
  const jobCount = await prisma.batteryJob.count({
    where: { chargeProgramId: id },
  });
  if (jobCount > 0) {
    throw createError({
      statusCode: 409,
      statusMessage: `Charging Program นี้ถูกใช้งานใน ${jobCount} ใบงาน จึงไม่สามารถลบได้`,
    });
  }
  await prisma.chargeProgram.update({ where: { id }, data: { active: false } });
  return { ok: true, deleted: true };
});
