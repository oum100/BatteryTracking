export default defineEventHandler(() => {
  return {
    ok: true,
    machines: [],
    message: 'Legacy endpoint kept for compatibility. Use /api/battery-movements instead.'
  }
})
