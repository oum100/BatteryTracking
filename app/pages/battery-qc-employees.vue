<script setup lang="ts">
definePageMeta({ middleware: 'admin-auth' })
useHead({ title: 'Employee Management' })

interface Employee {
  id: string
  code: string
  name: string
  active: boolean
}

const employees = ref<Employee[]>([])
const loading = ref(false)
const saving = ref(false)
const editorOpen = ref(false)
const editingId = ref<string | null>(null)
const message = ref('')
const errorMessage = ref('')
const form = reactive({ code: '', name: '', active: true })

const columns = [
  { accessorKey: 'code', header: 'Employee ID' },
  { accessorKey: 'name', header: 'Employee Name' },
  { accessorKey: 'active', header: 'Status' },
  { accessorKey: 'actions', header: 'Action' },
]
const editorTitle = computed(() => editingId.value ? 'Edit Employee' : 'Add Employee')
const fieldUi = { root: 'w-full', label: 'mb-1.5 text-sm font-bold text-slate-700' } as const
const inputUi = { base: 'rounded-[8px] border border-slate-300 bg-white px-3 text-sm font-semibold text-slate-950 ring-1 ring-inset ring-slate-300 focus:border-sky-600 focus:ring-sky-500' } as const

function getErrorMessage(error: unknown, fallback: string) {
  const requestError = error as { data?: { statusMessage?: string }, statusMessage?: string }
  return requestError.data?.statusMessage || requestError.statusMessage || fallback
}

async function loadEmployees() {
  loading.value = true
  try {
    const response = await $fetch<{ employees: Employee[] }>('/api/employees/admin')
    employees.value = response.employees
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'โหลดข้อมูลพนักงานไม่สำเร็จ')
  }
  finally {
    loading.value = false
  }
}

function openAdd() {
  editingId.value = null
  Object.assign(form, { code: '', name: '', active: true })
  editorOpen.value = true
}

function openEdit(employee: Employee) {
  editingId.value = employee.id
  Object.assign(form, { code: employee.code, name: employee.name, active: employee.active })
  editorOpen.value = true
}

async function saveEmployee() {
  if (!form.code.trim() || !form.name.trim()) {
    errorMessage.value = 'กรอก Employee ID และชื่อพนักงานก่อนบันทึก'
    return
  }

  saving.value = true
  message.value = ''
  errorMessage.value = ''
  try {
    if (editingId.value) {
      await $fetch(`/api/employees/${editingId.value}`, { method: 'PATCH', body: form })
    }
    else {
      await $fetch('/api/employees', { method: 'POST', body: form })
    }
    editorOpen.value = false
    message.value = editingId.value ? 'อัปเดตพนักงานเรียบร้อยแล้ว' : 'เพิ่มพนักงานเรียบร้อยแล้ว'
    await loadEmployees()
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'บันทึกพนักงานไม่สำเร็จ')
  }
  finally {
    saving.value = false
  }
}

async function setActive(employee: Employee, active: boolean) {
  saving.value = true
  errorMessage.value = ''
  try {
    await $fetch(`/api/employees/${employee.id}`, {
      method: active ? 'PATCH' : 'DELETE',
      ...(active ? { body: { code: employee.code, name: employee.name, active: true } } : {}),
    })
    message.value = active ? `เปิดใช้งาน ${employee.code} แล้ว` : `ปิดใช้งาน ${employee.code} แล้ว`
    await loadEmployees()
  }
  catch (error) {
    errorMessage.value = getErrorMessage(error, 'เปลี่ยนสถานะพนักงานไม่สำเร็จ')
  }
  finally {
    saving.value = false
  }
}

onMounted(loadEmployees)
</script>

<template>
  <main class="min-h-screen bg-[radial-gradient(circle_at_top_left,_rgba(14,165,233,0.16),_transparent_30%),linear-gradient(135deg,_#dbe7f4_0%,_#edf3f8_48%,_#d9e6ef_100%)] px-4 py-5 text-slate-950 sm:px-6">
    <section class="mx-auto max-w-6xl space-y-4">
      <header class="flex flex-wrap items-center justify-between gap-3 rounded-[15px] border border-slate-200 bg-white/90 px-5 py-4 shadow-sm"><div><p class="text-xs font-black uppercase tracking-[0.2em] text-sky-700">Master Data</p><h1 class="mt-1 text-3xl font-black tracking-tight text-slate-950">Employee Management</h1><p class="mt-1 text-sm font-semibold text-slate-600">เพิ่ม แก้ไข หรือปิดใช้งาน Employee ID สำหรับการสแกน QC</p></div><UButton to="/battery-qc-admin" color="neutral" variant="outline" class="rounded-full">Back to Admin</UButton></header>
      <UAlert v-if="message" color="success" variant="soft" :title="message" :ui="{ root: 'rounded-[15px] border border-emerald-300 bg-emerald-50 text-emerald-950', title: 'font-black' }" />
      <UAlert v-if="errorMessage" color="error" variant="soft" title="Employee Management Error" :description="errorMessage" :ui="{ root: 'rounded-[15px]' }" />
      <UCard :ui="{ root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm', body: 'p-0' }"><template #header><div class="flex items-center justify-between"><div><p class="text-xs font-black uppercase tracking-[0.18em] text-sky-700">Employee Setup</p><h2 class="mt-1 text-xl font-black">Employees</h2></div><UButton color="primary" icon="i-lucide-user-plus" class="rounded-full font-black" @click="openAdd">Add Employee</UButton></div></template><div class="overflow-x-auto"><UTable :data="employees" :columns="columns" :loading="loading" empty="ยังไม่มีพนักงาน"><template #active-cell="{ row }"><UBadge :color="row.original.active ? 'success' : 'neutral'" variant="soft">{{ row.original.active ? 'ACTIVE' : 'INACTIVE' }}</UBadge></template><template #actions-cell="{ row }"><div class="flex justify-end gap-1"><UButton color="neutral" variant="ghost" icon="i-lucide-pencil" title="แก้ไข" :disabled="saving" @click="openEdit(row.original)" /><UButton v-if="row.original.active" color="error" variant="ghost" icon="i-lucide-user-x" title="ปิดใช้งาน" :disabled="saving" @click="setActive(row.original, false)" /><UButton v-else color="success" variant="ghost" icon="i-lucide-user-check" title="เปิดใช้งาน" :disabled="saving" @click="setActive(row.original, true)" /></div></template></UTable></div></UCard>
      <UModal v-model:open="editorOpen" :dismissible="!saving" :ui="{ content: 'sm:max-w-xl rounded-[15px]' }"><template #content><UCard :ui="{ root: 'rounded-[15px] border-0', body: 'p-5', footer: 'p-5 border-t border-slate-200' }"><template #header><div><p class="text-xs font-black uppercase tracking-[0.18em] text-sky-700">Employee</p><h3 class="mt-1 text-2xl font-black text-slate-950">{{ editorTitle }}</h3></div></template><div class="grid gap-4 sm:grid-cols-2"><UFormField label="Employee ID" required :ui="fieldUi"><UInput v-model="form.code" placeholder="เช่น EMP-001" :ui="inputUi" /></UFormField><UFormField label="Employee Name" required :ui="fieldUi"><UInput v-model="form.name" placeholder="ชื่อพนักงาน" :ui="inputUi" /></UFormField><div v-if="editingId" class="sm:col-span-2 rounded-[8px] border border-slate-200 bg-slate-50 px-3 py-3"><UCheckbox v-model="form.active" label="Active - ใช้สแกนใน QC ได้" :ui="{ label: 'font-bold text-slate-900' }" /></div></div><template #footer><div class="flex justify-end gap-3"><UButton color="neutral" variant="outline" :disabled="saving" class="rounded-full" @click="editorOpen = false">Cancel</UButton><UButton color="primary" :loading="saving" class="rounded-full font-black" @click="saveEmployee">Save</UButton></div></template></UCard></template></UModal>
    </section>
  </main>
</template>
