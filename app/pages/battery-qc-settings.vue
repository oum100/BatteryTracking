<script setup lang="ts">
definePageMeta({ middleware: "admin-auth" });
useHead({ title: "Battery QC Settings" });

type UserRole = "ADMIN" | "CALIBRATOR";
type SettingEntity = "user" | "channel" | "program";

interface UserAccount {
  id: string;
  username: string;
  displayName: string;
  role: UserRole;
  active: boolean;
  lastLoginAt: string | null;
}

interface MasterItem {
  id: string;
  code: string;
  name: string;
  active: boolean;
}

const users = ref<UserAccount[]>([]);
const channels = ref<MasterItem[]>([]);
const programs = ref<MasterItem[]>([]);
const loading = ref(false);
const saving = ref(false);
const message = ref("");
const errorMessage = ref("");
const editorOpen = ref(false);
const deleteConfirmOpen = ref(false);
const editorKind = ref<SettingEntity>("user");
const editingId = ref<string | null>(null);
const pendingDelete = ref<{
  kind: SettingEntity;
  id: string;
  label: string;
} | null>(null);

const editor = reactive({
  username: "",
  displayName: "",
  pin: "",
  useDefaultPin: true,
  role: "CALIBRATOR" as UserRole,
  code: "",
  name: "",
});

const roleOptions = [
  { label: "Admin", value: "ADMIN" },
  { label: "Calibrator", value: "CALIBRATOR" },
];

const userColumns = [
  { accessorKey: "username", header: "Username" },
  { accessorKey: "displayName", header: "Display Name" },
  { accessorKey: "role", header: "Role" },
  { accessorKey: "lastLoginAt", header: "Last Login" },
  { accessorKey: "active", header: "Status" },
  { accessorKey: "actions", header: "Action" },
];
const masterColumns = [
  { accessorKey: "code", header: "Code" },
  { accessorKey: "name", header: "Name" },
  { accessorKey: "actions", header: "Action" },
];
const fieldUi = {
  root: "w-full",
  label: "mb-1 text-sm font-bold text-slate-700",
} as const;
const inputUi = {
  base: "rounded-[8px] border border-slate-300 bg-white px-3 text-sm font-semibold text-slate-950 ring-1 ring-inset ring-slate-300 focus:border-sky-600 focus:ring-sky-500",
} as const;
const tableUi = {
  th: "bg-slate-200 text-xs font-black uppercase tracking-[0.14em] text-slate-700",
  td: "py-3 text-sm font-semibold text-slate-800",
} as const;

const editorTitle = computed(() => {
  const action = editingId.value ? "Edit" : "Add";
  const label =
    editorKind.value === "user"
      ? "User"
      : editorKind.value === "channel"
        ? "Charging Channel"
        : "Charging Program";
  return `${action} ${label}`;
});
const editorDescription = computed(() =>
  editorKind.value === "user"
    ? "กำหนดบัญชีผู้ใช้สำหรับ Admin หรือ Calibration Workspace"
    : "Code ต้องไม่ซ้ำและจะแสดงเป็นตัวเลือกในหน้า QC",
);

function formatDate(value: string | null) {
  return value
    ? new Date(value).toLocaleString("th-TH", {
        dateStyle: "short",
        timeStyle: "short",
      })
    : "ยังไม่เคยเข้าใช้";
}

function clearEditor() {
  editor.username = "";
  editor.displayName = "";
  editor.pin = "";
  editor.useDefaultPin = true;
  editor.role = "CALIBRATOR";
  editor.code = "";
  editor.name = "";
}

async function loadSettings() {
  loading.value = true;
  errorMessage.value = "";
  try {
    const [userResponse, channelResponse, programResponse] = await Promise.all([
      $fetch<{ users: UserAccount[] }>("/api/users"),
      $fetch<{ chargeChannels: MasterItem[] }>("/api/charge-channels"),
      $fetch<{ chargePrograms: MasterItem[] }>("/api/charge-programs"),
    ]);
    users.value = userResponse.users;
    channels.value = channelResponse.chargeChannels;
    programs.value = programResponse.chargePrograms;
  } catch {
    errorMessage.value = "โหลด Settings ไม่สำเร็จ กรุณาลองใหม่อีกครั้ง";
  } finally {
    loading.value = false;
  }
}

function openAdd(kind: SettingEntity) {
  clearEditor();
  editorKind.value = kind;
  editingId.value = null;
  editorOpen.value = true;
}

function openEdit(kind: "user", item: UserAccount): void;
function openEdit(kind: "channel" | "program", item: MasterItem): void;
function openEdit(kind: SettingEntity, item: UserAccount | MasterItem) {
  clearEditor();
  editorKind.value = kind;
  editingId.value = item.id;
  if (kind === "user") {
    const user = item as UserAccount;
    editor.username = user.username;
    editor.displayName = user.displayName;
    editor.role = user.role;
    editor.useDefaultPin = false;
  } else {
    const masterItem = item as MasterItem;
    editor.code = masterItem.code;
    editor.name = masterItem.name;
  }
  editorOpen.value = true;
}

function requestDelete(kind: SettingEntity, item: UserAccount | MasterItem) {
  const label =
    kind === "user"
      ? `${(item as UserAccount).username} - ${(item as UserAccount).displayName}`
      : `${(item as MasterItem).code} - ${(item as MasterItem).name}`;
  pendingDelete.value = { kind, id: item.id, label };
  deleteConfirmOpen.value = true;
}

function errorText(error: unknown, fallback: string) {
  const details = error as {
    statusMessage?: string;
    data?: { statusMessage?: string };
  };
  return details.data?.statusMessage || details.statusMessage || fallback;
}

async function saveEditor() {
  saving.value = true;
  message.value = "";
  errorMessage.value = "";
  try {
    if (editorKind.value === "user") {
      const body = {
        username: editor.username,
        displayName: editor.displayName,
        role: editor.role,
        ...(editor.pin ? { pin: editor.pin } : {}),
      };
      if (editingId.value) {
        await $fetch(`/api/users/${editingId.value}`, {
          method: "PATCH",
          body,
        });
      } else {
        if (!editor.useDefaultPin && !editor.pin) {
          throw new Error("กำหนด PIN เริ่มต้น หรือเลือกใช้ Default PIN");
        }
        await $fetch("/api/users", {
          method: "POST",
          body: {
            ...body,
            useDefaultPin: editor.useDefaultPin,
            ...(editor.useDefaultPin ? {} : { pin: editor.pin }),
          },
        });
      }
    } else {
      const endpoint =
        editorKind.value === "channel"
          ? "/api/charge-channels"
          : "/api/charge-programs";
      const body = { code: editor.code, name: editor.name };
      if (editingId.value) {
        await $fetch(`${endpoint}/${editingId.value}`, {
          method: "PATCH",
          body,
        });
      } else {
        await $fetch(endpoint, { method: "POST", body });
      }
    }
    message.value = editingId.value
      ? "อัปเดตรายการเรียบร้อยแล้ว"
      : "เพิ่มรายการเรียบร้อยแล้ว";
    editorOpen.value = false;
    await loadSettings();
  } catch (error) {
    errorMessage.value = errorText(error, "บันทึกข้อมูลไม่สำเร็จ");
  } finally {
    saving.value = false;
  }
}

async function confirmDelete() {
  if (!pendingDelete.value) return;
  saving.value = true;
  errorMessage.value = "";
  try {
    const { kind, id, label } = pendingDelete.value;
    if (kind === "user") {
      await $fetch(`/api/users/${id}`, {
        method: "PATCH",
        body: { active: false },
      });
    } else {
      const endpoint =
        kind === "channel" ? "/api/charge-channels" : "/api/charge-programs";
      await $fetch(`${endpoint}/${id}`, { method: "DELETE" });
    }
    message.value = `ลบ ${label} แล้ว`;
    deleteConfirmOpen.value = false;
    pendingDelete.value = null;
    await loadSettings();
  } catch (error) {
    errorMessage.value = errorText(error, "ลบรายการไม่สำเร็จ");
  } finally {
    saving.value = false;
  }
}

onMounted(loadSettings);
</script>

<template>
  <main
    class="min-h-screen bg-[linear-gradient(135deg,_#dbe7f4_0%,_#edf3f8_48%,_#d9e6ef_100%)] px-4 py-5 text-slate-950 sm:px-6"
  >
    <section class="mx-auto max-w-7xl space-y-4">
      <header
        class="flex flex-wrap items-center justify-between gap-3 rounded-[15px] border border-slate-200 bg-white/90 px-5 py-4 shadow-sm"
      >
        <div>
          <p class="text-xs font-black uppercase tracking-[0.2em] text-sky-700">
            Settings Workspace
          </p>
          <h1 class="mt-1 text-3xl font-black tracking-tight text-slate-950">
            Battery QC Settings
          </h1>
        </div>
        <UButton
          to="/battery-qc-admin"
          color="neutral"
          variant="outline"
          class="rounded-full"
          >Back to Admin</UButton
        >
      </header>

      <UAlert
        v-if="message"
        color="success"
        variant="soft"
        :title="message"
        :ui="{
          root: 'rounded-[15px] border border-emerald-300 bg-emerald-50 text-emerald-950',
          title: 'font-black',
        }"
      />
      <UAlert
        v-if="errorMessage"
        color="error"
        variant="soft"
        title="Settings Error"
        :description="errorMessage"
        :ui="{ root: 'rounded-[15px]', title: 'font-black' }"
      />

      <UCard
        :ui="{
          root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm',
          body: 'p-0',
        }"
      >
        <template #header
          ><div class="flex flex-wrap items-center justify-between gap-3">
            <div>
              <p
                class="text-xs font-black uppercase tracking-[0.18em] text-sky-700"
              >
                Protected Users
              </p>
              <h2 class="mt-1 text-xl font-black text-slate-950">
                Admin & Calibration Accounts
              </h2>
            </div>
            <UButton
              color="primary"
              icon="i-lucide-plus"
              class="rounded-full font-black"
              @click="openAdd('user')"
              >Add User</UButton
            >
          </div></template
        >
        <div class="overflow-x-auto">
          <UTable
            :data="users"
            :columns="userColumns"
            :loading="loading"
            :ui="tableUi"
            empty="ยังไม่มี User"
            ><template #role-cell="{ row }"
              ><UBadge color="info" variant="soft">{{
                row.original.role
              }}</UBadge></template
            ><template #lastLoginAt-cell="{ row }">{{
              formatDate(row.original.lastLoginAt)
            }}</template
            ><template #active-cell="{ row }"
              ><UBadge
                :color="row.original.active ? 'success' : 'neutral'"
                variant="soft"
                >{{ row.original.active ? "ACTIVE" : "INACTIVE" }}</UBadge
              ></template
            ><template #actions-cell="{ row }"
              ><div class="flex justify-end gap-1">
                <UButton
                  color="neutral"
                  variant="ghost"
                  icon="i-lucide-pencil"
                  aria-label="Edit user"
                  :disabled="saving"
                  @click="openEdit('user', row.original)"
                /><UButton
                  color="error"
                  variant="ghost"
                  icon="i-lucide-trash-2"
                  aria-label="Delete user"
                  :disabled="saving || !row.original.active"
                  @click="requestDelete('user', row.original)"
                /></div></template
          ></UTable>
        </div>
      </UCard>

      <div class="grid gap-4 xl:grid-cols-2">
        <UCard
          :ui="{
            root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm',
            body: 'p-0',
          }"
        >
          <template #header
            ><div class="flex items-center justify-between gap-3">
              <div>
                <p
                  class="text-xs font-black uppercase tracking-[0.18em] text-sky-700"
                >
                  Charging Setup
                </p>
                <h2 class="mt-1 text-xl font-black text-slate-950">
                  Charging Channels
                </h2>
              </div>
              <UButton
                color="primary"
                icon="i-lucide-plus"
                class="rounded-full font-black"
                @click="openAdd('channel')"
                >Add Channel</UButton
              >
            </div></template
          >
          <div class="overflow-x-auto">
            <UTable
              :data="channels"
              :columns="masterColumns"
              :loading="loading"
              :ui="tableUi"
              empty="ยังไม่มี Charging Channel"
              ><template #actions-cell="{ row }"
                ><div class="flex justify-end gap-1">
                  <UButton
                    color="neutral"
                    variant="ghost"
                    icon="i-lucide-pencil"
                    aria-label="Edit channel"
                    :disabled="saving"
                    @click="openEdit('channel', row.original)"
                  /><UButton
                    color="error"
                    variant="ghost"
                    icon="i-lucide-trash-2"
                    aria-label="Delete channel"
                    :disabled="saving"
                    @click="requestDelete('channel', row.original)"
                  /></div></template
            ></UTable>
          </div>
        </UCard>

        <UCard
          :ui="{
            root: 'rounded-[15px] border border-slate-200 bg-white shadow-sm',
            body: 'p-0',
          }"
        >
          <template #header
            ><div class="flex items-center justify-between gap-3">
              <div>
                <p
                  class="text-xs font-black uppercase tracking-[0.18em] text-sky-700"
                >
                  Charging Setup
                </p>
                <h2 class="mt-1 text-xl font-black text-slate-950">
                  Charging Programs
                </h2>
              </div>
              <UButton
                color="primary"
                icon="i-lucide-plus"
                class="rounded-full font-black"
                @click="openAdd('program')"
                >Add Program</UButton
              >
            </div></template
          >
          <div class="overflow-x-auto">
            <UTable
              :data="programs"
              :columns="masterColumns"
              :loading="loading"
              :ui="tableUi"
              empty="ยังไม่มี Charging Program"
              ><template #actions-cell="{ row }"
                ><div class="flex justify-end gap-1">
                  <UButton
                    color="neutral"
                    variant="ghost"
                    icon="i-lucide-pencil"
                    aria-label="Edit program"
                    :disabled="saving"
                    @click="openEdit('program', row.original)"
                  /><UButton
                    color="error"
                    variant="ghost"
                    icon="i-lucide-trash-2"
                    aria-label="Delete program"
                    :disabled="saving"
                    @click="requestDelete('program', row.original)"
                  /></div></template
            ></UTable>
          </div>
        </UCard>
      </div>

      <UModal
        v-model:open="editorOpen"
        :dismissible="!saving"
        :ui="{ content: 'sm:max-w-xl rounded-[15px]' }"
        ><template #content
          ><UCard
            :ui="{
              root: 'rounded-[15px] border-0',
              body: 'p-5',
              footer: 'p-5 border-t border-slate-200',
            }"
            ><template #header
              ><div>
                <p
                  class="text-xs font-black uppercase tracking-[0.18em] text-sky-700"
                >
                  Settings
                </p>
                <h3 class="mt-1 text-2xl font-black text-slate-950">
                  {{ editorTitle }}
                </h3>
                <p class="mt-2 text-sm font-semibold text-slate-600">
                  {{ editorDescription }}
                </p>
              </div></template
            >
            <div v-if="editorKind === 'user'" class="grid gap-4 sm:grid-cols-2">
              <UFormField label="Username" required :ui="fieldUi"
                ><UInput
                  v-model="editor.username"
                  placeholder="เช่น CAL001"
                  :ui="inputUi" /></UFormField
              ><UFormField label="Display name" required :ui="fieldUi"
                ><UInput
                  v-model="editor.displayName"
                  placeholder="ชื่อผู้ใช้งาน"
                  :ui="inputUi" /></UFormField
              ><UFormField label="Role" required :ui="fieldUi"
                ><USelectMenu
                  v-model="editor.role"
                  :items="roleOptions"
                  value-key="value"
                  label-key="label"
                  :search-input="false"
                  :ui="inputUi"
              /></UFormField>
              <div
                v-if="!editingId"
                class="sm:col-span-2 rounded-[8px] border border-sky-200 bg-sky-50 px-3 py-3"
              >
                <UCheckbox
                  v-model="editor.useDefaultPin"
                  label="Use Default PIN"
                  :ui="{ label: 'font-black text-slate-900' }"
                />
                <p class="mt-1 text-xs font-semibold text-slate-600">
                  ใช้ค่า DEFAULT_USER_PIN ที่กำหนดใน environment
                </p>
              </div>
              <UFormField
                v-if="editingId || !editor.useDefaultPin"
                :label="editingId ? 'New PIN (optional)' : 'Initial PIN'"
                :required="!editingId"
                :ui="fieldUi"
                ><UInput
                  v-model="editor.pin"
                  type="password"
                  placeholder="4-12 digits"
                  :ui="inputUi"
              /></UFormField>
            </div>
            <div v-else class="grid gap-4 sm:grid-cols-2">
              <UFormField label="Code" required :ui="fieldUi"
                ><UInput
                  v-model="editor.code"
                  placeholder="เช่น CH-A"
                  :ui="inputUi" /></UFormField
              ><UFormField label="Name" required :ui="fieldUi"
                ><UInput
                  v-model="editor.name"
                  placeholder="ชื่อรายการ"
                  :ui="inputUi"
              /></UFormField>
            </div>
            <template #footer
              ><div class="flex justify-end gap-3">
                <UButton
                  color="neutral"
                  variant="outline"
                  :disabled="saving"
                  class="rounded-full"
                  @click="editorOpen = false"
                  >Cancel</UButton
                ><UButton
                  color="primary"
                  :loading="saving"
                  class="rounded-full font-black"
                  @click="saveEditor"
                  >Save</UButton
                >
              </div></template
            ></UCard
          ></template
        ></UModal
      >

      <UModal
        v-model:open="deleteConfirmOpen"
        :dismissible="!saving"
        :ui="{ content: 'sm:max-w-lg rounded-[15px]' }"
        ><template #content
          ><UCard
            :ui="{
              root: 'rounded-[15px] border-0',
              body: 'p-5',
              footer: 'p-5 border-t border-slate-200',
            }"
            ><template #header
              ><div>
                <p
                  class="text-xs font-black uppercase tracking-[0.18em] text-rose-600"
                >
                  Soft Delete
                </p>
                <h3 class="mt-1 text-2xl font-black text-slate-950">
                  ยืนยันการลบรายการ
                </h3>
              </div></template
            >
            <div class="space-y-3">
              <p class="text-sm font-semibold leading-6 text-slate-700">
                รายการจะถูกปิดใช้งานและไม่แสดงให้เลือกในงาน QC ใหม่
                ข้อมูลเดิมในใบงานจะไม่ถูกลบ
              </p>
              <div
                class="rounded-[8px] border border-rose-200 bg-rose-50 px-4 py-3 font-black text-rose-950"
              >
                {{ pendingDelete?.label }}
              </div>
            </div>
            <template #footer
              ><div class="flex justify-end gap-3">
                <UButton
                  color="neutral"
                  variant="outline"
                  :disabled="saving"
                  class="rounded-full"
                  @click="deleteConfirmOpen = false"
                  >Cancel</UButton
                ><UButton
                  color="error"
                  :loading="saving"
                  class="rounded-full font-black"
                  @click="confirmDelete"
                  >Delete</UButton
                >
              </div></template
            ></UCard
          ></template
        ></UModal
      >
    </section>
  </main>
</template>
