import { ensureRequiredText } from "../../utils/battery-jobs";
import { requireAdminSession } from "../../utils/admin-auth";
import { prisma } from "../../utils/prisma";

interface ChargeProgramPayload {
  code?: string;
  name?: string;
}

export default defineEventHandler(async (event) => {
  await requireAdminSession(event);

  const id = ensureRequiredText(getRouterParam(event, "id"), "program ID");
  const body = await readBody<ChargeProgramPayload>(event);
  const chargeProgram = await prisma.chargeProgram.update({
    where: { id },
    data: {
      code: ensureRequiredText(body.code, "code").toUpperCase(),
      name: ensureRequiredText(body.name, "name"),
    },
  });

  return { ok: true, chargeProgram };
});
