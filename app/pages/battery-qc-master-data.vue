<script setup lang="ts">
definePageMeta({ middleware: 'admin-auth' })
useHead({ title: 'Sales Order Management' })

interface SalesOrderRecord {
  id: string
  soNumber: string
  description: string | null
  active: boolean
  invoice: { id: string, invoiceNo: string } | null
}

const salesOrders = ref<SalesOrderRecord[]>([])
const loading = ref(false)
const saving = ref(false)
const salesOrderEditorOpen = ref(false)
const invoiceEditorOpen = ref(false)
const disableConfirmOpen = ref(false)
const editingSalesOrderId = ref<string | null>(null)
const selectedSalesOrder = ref<SalesOrderRecord | null>(null)
const message = ref('')
const errorMessage = ref('')

const salesOrderForm = reactive({ soNumber: '', description: '', invoiceNo: '', active: true })
const invoiceNumber = ref('')

const columns = [
  { accessorKey: 'soNumber', header: 'SO Number' },
  { accessorKey: 'invoice', header: 'Invoice' },
  { accessorKey: 'description', header: 'Description' },
  { accessorKey: 'active', header: 'Status' },
  { accessorKey: 'actions', header: 'Action' },
]

const salesOrderEditorTitle = computed(() => editingSalesOrderId.value ? 'Edit Sales Order' : 'Add Sales Order')
const invoiceEditorTitle = computed(() => selectedSalesOrder.value?.invoice ? 'Edit Invoice' : 'Add Invoice')
const fieldUi = { root: 'w-full', label: 'mb-1.5 text-sm font-bold text-slate-700' } as const
const inputUi = { base: 'rounded-[8px] border border-slate-300 bg-white px-3 text-sm font-semibold text-slate-950 ring-1 ring-inset ring-slate-300 focus:border-sky-600 focus:ring-sky-500' } as const
const tableUi = { th: 'bg-slate-200 text-xs font-black uppercase tracking-[0.14em] text-slate-700', td: 'py-3 text-sm font-semibold text-slate-800' } as const

function getErrorMessage(error: unknown, fallback: string) {
  const requestError = error as { data?: { statusMessage?: string }, statusMessage?: string }
  return requestError.data?.statusMessage || requestError.statusMessage || fallback
}

async function loadSalesOrders() {
  loading.value = true
  errorMessage.value = ''
  try {
    const response = await $fetch<{ salesOrders: SalesOrderRecord[] }>('/api/sales-orders/admin')
    salesOrders.value = response.salesOrders
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'โหลดข้อมูล SO ไม่สำเร็จ')
  }
  finally {
    loading.value = false
  }
}

function openAddSalesOrder() {
  editingSalesOrderId.value = null
  salesOrderForm.soNumber = ''
  salesOrderForm.description = ''
  salesOrderForm.invoiceNo = ''
  salesOrderForm.active = true
  salesOrderEditorOpen.value = true
}

function openEditSalesOrder(salesOrder: SalesOrderRecord) {
  editingSalesOrderId.value = salesOrder.id
  salesOrderForm.soNumber = salesOrder.soNumber
  salesOrderForm.description = salesOrder.description || ''
  salesOrderForm.invoiceNo = ''
  salesOrderForm.active = salesOrder.active
  salesOrderEditorOpen.value = true
}

async function saveSalesOrder() {
  if (!salesOrderForm.soNumber.trim()) {
    errorMessage.value = 'กรอก SO Number ก่อนบันทึก'
    return
  }

  saving.value = true
  message.value = ''
  errorMessage.value = ''
  try {
    const { invoiceNo, ...salesOrder } = salesOrderForm
    if (editingSalesOrderId.value) {
      await $fetch(`/api/sales-orders/${editingSalesOrderId.value}`, { method: 'PATCH', body: salesOrder })
    }
    else {
      await $fetch('/api/sales-orders', { method: 'POST', body: { ...salesOrder, invoiceNo } })
    }
    salesOrderEditorOpen.value = false
    message.value = editingSalesOrderId.value ? 'อัปเดต SO เรียบร้อยแล้ว' : 'เพิ่ม SO เรียบร้อยแล้ว'
    await loadSalesOrders()
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'บันทึก SO ไม่สำเร็จ')
  }
  finally {
    saving.value = false
  }
}

function openInvoiceEditor(salesOrder: SalesOrderRecord) {
  selectedSalesOrder.value = salesOrder
  invoiceNumber.value = salesOrder.invoice?.invoiceNo || ''
  invoiceEditorOpen.value = true
}

async function saveInvoice() {
  if (!selectedSalesOrder.value || !invoiceNumber.value.trim()) {
    errorMessage.value = 'กรอก Invoice Number ก่อนบันทึก'
    return
  }

  saving.value = true
  message.value = ''
  errorMessage.value = ''
  try {
    const body = { invoiceNo: invoiceNumber.value, salesOrderId: selectedSalesOrder.value.id }
    if (selectedSalesOrder.value.invoice) {
      await $fetch(`/api/invoices/${selectedSalesOrder.value.invoice.id}`, { method: 'PATCH', body })
    }
    else {
      await $fetch('/api/invoices', { method: 'POST', body })
    }
    invoiceEditorOpen.value = false
    message.value = selectedSalesOrder.value.invoice ? 'อัปเดต Invoice เรียบร้อยแล้ว' : 'เพิ่ม Invoice เรียบร้อยแล้ว'
    await loadSalesOrders()
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'บันทึก Invoice ไม่สำเร็จ')
  }
  finally {
    saving.value = false
  }
}

function requestDisable(salesOrder: SalesOrderRecord) {
  selectedSalesOrder.value = salesOrder
  disableConfirmOpen.value = true
}

async function confirmDisable() {
  if (!selectedSalesOrder.value) return
  saving.value = true
  errorMessage.value = ''
  try {
    await $fetch(`/api/sales-orders/${selectedSalesOrder.value.id}`, { method: 'DELETE' })
    message.value = `ปิดใช้งาน ${selectedSalesOrder.value.soNumber} แล้ว`
    disableConfirmOpen.value = false
    await loadSalesOrders()
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'ปิดใช้งาน SO ไม่สำเร็จ')
  }
  finally {
    saving.value = false
  }
}

async function restoreSalesOrder(salesOrder: SalesOrderRecord) {
  saving.value = true
  errorMessage.value = ''
  try {
    await $fetch(`/api/sales-orders/${salesOrder.id}`, {
      method: 'PATCH',
      body: { soNumber: salesOrder.soNumber, description: salesOrder.description, active: true },
    })
    message.value = `เปิดใช้งาน ${salesOrder.soNumber} แล้ว`
    await loadSalesOrders()
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'เปิดใช้งาน SO ไม่สำเร็จ')
  }
  finally {
    saving.value = false
  }
}

onMounted(loadSalesOrders)
</script>

<template>
  <main class="min-h-screen bg-[radial-gradient(circle_at_top_left,_rgba(14,165,233,0.16),_transparent_30%),linear-gradient(135deg,_#dbe7f4_0%,_#edf3f8_48%,_#d9e6ef_100%)] px-4 py-5 text-slate-950 sm:px-6">
    <section class="mx-auto max-w-7xl space-y-4">
      <header class="flex flex-wrap items-center justify-between gap-3 rounded-[15px] border border-slate-200 bg-white/90 px-5 py-4 shadow-sm">
        <div>
          <p class="text-xs font-black uppercase tracking-[0.2em] text-sky-700">Master Data</p>
          <h1 class="mt-1 text-3xl font-black tracking-tight text-slate-950">Sales Order Management</h1>
          <p class="mt-1 text-sm font-semibold text-slate-600">สร้าง SO ก่อน แล้วเพิ่ม Invoice จากรายการ SO ที่เกี่ยวข้อง</p>
        </div>
        <UButton to="/battery-qc-admin" color="neutral" variant="outline" class="rounded-full">Back to Admin</UButton>
      </header>

      <UAlert v-if="message" color="success" variant="soft" :title="message" :ui="{ root: 'rounded-[15px] border border-emerald-300 bg-emerald-50 text-emerald-950', title: 'font-black' }" />
      <UAlert v-if="errorMessage" color="error" variant="soft" title="SO Management Error" :description="errorMessage" :ui="{ root: 'rounded-[15px]', title: 'font-black' }" />

      <UCard :ui="{ root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm', body: 'p-0' }">
        <template #header>
          <div class="flex items-center justify-between gap-3">
            <div><p class="text-xs font-black uppercase tracking-[0.18em] text-sky-700">Order Setup</p><h2 class="mt-1 text-xl font-black text-slate-950">Sales Orders</h2></div>
            <UButton color="primary" icon="i-lucide-plus" class="rounded-full font-black" @click="openAddSalesOrder">Add SO</UButton>
          </div>
        </template>
        <div class="overflow-x-auto">
          <UTable :data="salesOrders" :columns="columns" :loading="loading" :ui="tableUi" empty="ยังไม่มี SO">
            <template #invoice-cell="{ row }"><span :class="row.original.invoice ? 'font-black text-slate-900' : 'text-slate-400'">{{ row.original.invoice?.invoiceNo || 'ยังไม่มี Invoice' }}</span></template>
            <template #description-cell="{ row }">{{ row.original.description || '-' }}</template>
            <template #active-cell="{ row }"><UBadge :color="row.original.active ? 'success' : 'neutral'" variant="soft">{{ row.original.active ? 'ACTIVE' : 'INACTIVE' }}</UBadge></template>
            <template #actions-cell="{ row }"><div class="flex justify-end gap-1"><UButton color="neutral" variant="ghost" icon="i-lucide-pencil" aria-label="Edit sales order" :disabled="saving" @click="openEditSalesOrder(row.original)" /><UButton color="primary" variant="ghost" :icon="row.original.invoice ? 'i-lucide-receipt-text' : 'i-lucide-file-plus-2'" :aria-label="row.original.invoice ? 'Edit invoice' : 'Add invoice'" :disabled="saving || !row.original.active" @click="openInvoiceEditor(row.original)">{{ row.original.invoice ? 'Edit Invoice' : 'Add Invoice' }}</UButton><UButton v-if="row.original.active" color="error" variant="ghost" icon="i-lucide-trash-2" aria-label="Disable sales order" :disabled="saving" @click="requestDisable(row.original)" /><UButton v-else color="success" variant="ghost" icon="i-lucide-rotate-ccw" aria-label="Restore sales order" :disabled="saving" @click="restoreSalesOrder(row.original)" /></div></template>
          </UTable>
        </div>
      </UCard>

      <UModal v-model:open="salesOrderEditorOpen" :dismissible="!saving" :ui="{ content: 'sm:max-w-3xl rounded-[15px]' }">
        <template #content>
          <UCard :ui="{ root: 'rounded-[15px] border-0', body: 'p-5', footer: 'p-5 border-t border-slate-200' }">
            <template #header><div><p class="text-xs font-black uppercase tracking-[0.18em] text-sky-700">Sales Order</p><h3 class="mt-1 text-2xl font-black text-slate-950">{{ salesOrderEditorTitle }}</h3></div></template>
            <div class="grid gap-4 sm:grid-cols-3">
              <UFormField label="SO Number" required :ui="fieldUi"><UInput v-model="salesOrderForm.soNumber" placeholder="เช่น SO-240701" :ui="inputUi" /></UFormField>
              <UFormField v-if="!editingSalesOrderId" label="Invoice Number (Optional)" :ui="fieldUi"><UInput v-model="salesOrderForm.invoiceNo" placeholder="เช่น 6907017" :ui="inputUi" /></UFormField>
              <UFormField label="Description" :ui="fieldUi"><UInput v-model="salesOrderForm.description" placeholder="รายละเอียด SO" :ui="inputUi" /></UFormField>
              <div v-if="!editingSalesOrderId" class="sm:col-span-3 rounded-[8px] border border-sky-200 bg-sky-50 px-3 py-3 text-sm font-semibold text-sky-950">เว้น Invoice ไว้ได้ แล้วค่อยเพิ่มจากปุ่มในรายการ SO ภายหลัง</div>
              <div v-else class="sm:col-span-3 rounded-[8px] border border-slate-200 bg-slate-50 px-3 py-3"><UCheckbox v-model="salesOrderForm.active" label="Active - แสดงเป็นตัวเลือกในใบงานใหม่" :ui="{ label: 'font-bold text-slate-900' }" /></div>
            </div>
            <template #footer><div class="flex justify-end gap-3"><UButton color="neutral" variant="outline" :disabled="saving" class="rounded-full" @click="salesOrderEditorOpen = false">Cancel</UButton><UButton color="primary" :loading="saving" class="rounded-full font-black" @click="saveSalesOrder">Save</UButton></div></template>
          </UCard>
        </template>
      </UModal>

      <UModal v-model:open="invoiceEditorOpen" :dismissible="!saving" :ui="{ content: 'sm:max-w-lg rounded-[15px]' }"><template #content><UCard :ui="{ root: 'rounded-[15px] border-0', body: 'p-5', footer: 'p-5 border-t border-slate-200' }"><template #header><div><p class="text-xs font-black uppercase tracking-[0.18em] text-sky-700">{{ selectedSalesOrder?.soNumber }}</p><h3 class="mt-1 text-2xl font-black text-slate-950">{{ invoiceEditorTitle }}</h3><p class="mt-2 text-sm font-semibold text-slate-600">Invoice นี้จะผูกกับ SO นี้โดยอัตโนมัติ</p></div></template><UFormField label="Invoice Number" required :ui="fieldUi"><UInput v-model="invoiceNumber" placeholder="เช่น 6907017" :ui="inputUi" /></UFormField><template #footer><div class="flex justify-end gap-3"><UButton color="neutral" variant="outline" :disabled="saving" class="rounded-full" @click="invoiceEditorOpen = false">Cancel</UButton><UButton color="primary" :loading="saving" class="rounded-full font-black" @click="saveInvoice">Save Invoice</UButton></div></template></UCard></template></UModal>

      <UModal v-model:open="disableConfirmOpen" :dismissible="!saving" :ui="{ content: 'sm:max-w-lg rounded-[15px]' }"><template #content><UCard :ui="{ root: 'rounded-[15px] border-0', body: 'p-5', footer: 'p-5 border-t border-slate-200' }"><template #header><div><p class="text-xs font-black uppercase tracking-[0.18em] text-rose-600">Soft Delete</p><h3 class="mt-1 text-2xl font-black text-slate-950">ปิดใช้งาน SO</h3></div></template><p class="text-sm font-semibold leading-6 text-slate-700">SO จะไม่แสดงให้เลือกในใบงานใหม่ แต่ใบงานเดิมและ Invoice ที่ผูกไว้จะไม่ถูกลบ</p><div class="mt-3 rounded-[8px] border border-rose-200 bg-rose-50 px-4 py-3 font-black text-rose-950">{{ selectedSalesOrder?.soNumber }}</div><template #footer><div class="flex justify-end gap-3"><UButton color="neutral" variant="outline" :disabled="saving" class="rounded-full" @click="disableConfirmOpen = false">Cancel</UButton><UButton color="error" :loading="saving" class="rounded-full font-black" @click="confirmDisable">Disable</UButton></div></template></UCard></template></UModal>
    </section>
  </main>
</template>
