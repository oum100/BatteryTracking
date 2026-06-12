import { prisma } from '../../utils/prisma'

export default defineEventHandler(async (event) => {
  const id = getRouterParam(event, 'id')

  if (!id) {
    throw createError({
      statusCode: 400,
      statusMessage: 'id is required',
    })
  }

  await prisma.batteryMovement.delete({
    where: { id },
  })

  return {
    ok: true,
    id,
  }
})
