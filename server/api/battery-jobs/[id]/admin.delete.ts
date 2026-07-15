import { prisma } from '../../../utils/prisma'
import { isBatteryJobLocked, jobHasRecordedSlotData } from '../../../utils/battery-jobs'
import { requireAdminSession } from '../../../utils/admin-auth'

interface DeleteBatteryJobPayload {
  scope?: 'JOB' | 'GROUP' | string | null
}

export default defineEventHandler(async (event) => {
  requireAdminSession(event)

  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  const body = await readBody<DeleteBatteryJobPayload>(event).catch(() => ({} as DeleteBatteryJobPayload))
  const scope = String(body.scope ?? 'JOB').trim().toUpperCase()

  const targetJob = await prisma.batteryJob.findUnique({
    where: { id },
    select: {
      id: true,
      batchId: true,
      status: true,
      lockedAt: true,
      slots: true,
    },
  })

  if (!targetJob) {
    throw createError({
      statusCode: 404,
      statusMessage: 'Job not found',
    })
  }

  if (scope === 'GROUP') {
    const groupWhere = targetJob.batchId
      ? { batchId: targetJob.batchId }
      : { id: targetJob.id }

    const groupJobs = await prisma.batteryJob.findMany({
      where: groupWhere,
      select: {
        id: true,
        status: true,
        lockedAt: true,
        slots: true,
      },
    })

    const hasLockedOrStartedJob = groupJobs.some(job => isBatteryJobLocked(job) || jobHasRecordedSlotData(job))
    if (hasLockedOrStartedJob) {
      throw createError({
        statusCode: 409,
        statusMessage: 'This job group already has QC progress and cannot be deleted',
      })
    }

    const deleted = await prisma.batteryJob.deleteMany({
      where: groupWhere,
    })

    return {
      ok: true,
      scope: 'GROUP',
      deletedCount: deleted.count,
    }
  }

  if (isBatteryJobLocked(targetJob) || jobHasRecordedSlotData(targetJob)) {
    throw createError({
      statusCode: 409,
      statusMessage: 'This job already has QC progress and cannot be deleted',
    })
  }

  await prisma.batteryJob.delete({
    where: { id: targetJob.id },
  })

  return {
    ok: true,
    scope: 'JOB',
    deletedCount: 1,
  }
})
