import { ensureRequiredText } from "../../utils/battery-jobs";
import { requireAdminSession } from "../../utils/admin-auth";
import { prisma } from "../../utils/prisma";

interface ChargeChannelPayload {
  code?: string;
  name?: string;
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event);

  const id = ensureRequiredText(getRouterParam(event, "id"), "channel ID");
  const body = await readBody<ChargeChannelPayload>(event);
  const chargeChannel = await prisma.chargeChannel.update({
    where: { id },
    data: {
      code: ensureRequiredText(body.code, "code").toUpperCase(),
      name: ensureRequiredText(body.name, "name"),
    },
  });

  return { ok: true, chargeChannel };
});
