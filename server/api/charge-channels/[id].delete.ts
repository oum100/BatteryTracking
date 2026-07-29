import { ensureRequiredText } from "../../utils/battery-jobs";
import { requireAdminSession } from "../../utils/admin-auth";
import { prisma } from "../../utils/prisma";

export default defineEventHandler(async (event) => {
  await requireAdminSession(event);

  const id = ensureRequiredText(getRouterParam(event, "id"), "channel ID");
  const jobCount = await prisma.batteryJob.count({
    where: { chargeChannelId: id },
  });
  if (jobCount > 0) {
    throw createError({
      statusCode: 409,
      statusMessage: `Charging Channel นี้ถูกใช้งานใน ${jobCount} ใบงาน จึงไม่สามารถลบได้`,
    });
  }
  await prisma.chargeChannel.update({ where: { id }, data: { active: false } });
  return { ok: true, deleted: true };
});
