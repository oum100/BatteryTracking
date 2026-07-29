import { prisma } from "../../utils/prisma";

const activeJobWhere = {
  lockedAt: null,
  status: { not: "SHIPPED" as const },
};

export default defineEventHandler(async () => {
  const [beforeCharge, afterCharge, delivery] = await Promise.all([
    prisma.batteryJob.count({
      where: { ...activeJobWhere, beforeChargeCompletedAt: null },
    }),
    prisma.batteryJob.count({
      where: {
        ...activeJobWhere,
        beforeChargeCompletedAt: { not: null },
        afterChargeCompletedAt: null,
      },
    }),
    prisma.batteryJob.count({
      where: {
        ...activeJobWhere,
        afterChargeCompletedAt: { not: null },
        deliveryCompletedAt: null,
      },
    }),
  ]);

  return {
    ok: true,
    queues: { beforeCharge, afterCharge, delivery },
  };
});
