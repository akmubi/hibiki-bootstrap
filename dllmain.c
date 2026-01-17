#include <windows.h>

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#define DLL_NAME_MAIN     "UE4SS.dll"
#define DLL_NAME_FALLBACK "hibiki.dll"

#define CRC_RVA      0xCEB3EAC // TODO: use signature search instead
#define NT_COPY_SIZE 0x20

#define ASSERT(expr)                     \
  do {                                   \
    if (!(expr)) {                       \
      errorf("Assertion failed", #expr); \
      ExitProcess(1);                    \
    }                                    \
  } while (0)

typedef NTSTATUS(NTAPI* nt_protect_virtual_memory_t)(
  HANDLE  process_handle,
  PVOID  *base_addr,
  PSIZE_T region_size,
  ULONG   new_prot,
  PULONG  old_prot
);

static nt_protect_virtual_memory_t g_nt_protect_virtual_memory = NULL;

static uint8_t *g_target        = NULL;
static uint8_t *g_page          = NULL;
static DWORD    g_page_old_prot = 0;
static SIZE_T   g_page_size     = 0;

static void
errorf(const char *title, const char *fmt, ...)
{
  char msg[1024];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  MessageBoxA(NULL, msg, title, MB_OK);
}

static BOOL WINAPI
my_virtual_protect(LPVOID addr, SIZE_T size, DWORD new_prot, PDWORD old_prot)
{
  NTSTATUS status = 0;
  HANDLE   h_process = GetCurrentProcess();
  SIZE_T   size64 = size;

  if (g_nt_protect_virtual_memory) {
    status = g_nt_protect_virtual_memory(h_process, &addr, &size64, new_prot, old_prot);
  }
  return (status >= 0);
}

static void
on_crc(CONTEXT *ctx)
{
  static int prev_a = 0;

  DWORD a = (DWORD)ctx->Rdx;
  DWORD b = (DWORD)ctx->R11;
  DWORD result = a + b;

  ctx->R11 = (DWORD64)result;
  ctx->Rip += 3;

  if (prev_a == 0xFE0 && a == 1) {
    HMODULE h_ntdll = GetModuleHandleA("ntdll.dll");
    ASSERT(h_ntdll != NULL);

    uint8_t* nt_original = (uint8_t*)GetProcAddress(h_ntdll, "NtProtectVirtualMemory");
    if (nt_original[0] == 0xE9 /* jmp */) {
      DWORD old_protect;
      if (my_virtual_protect(nt_original, NT_COPY_SIZE, PAGE_EXECUTE_READWRITE, &old_protect)) {
        memcpy(nt_original, (void*)g_nt_protect_virtual_memory, NT_COPY_SIZE);
        if (!my_virtual_protect(nt_original, NT_COPY_SIZE, old_protect, &old_protect)) {
          errorf("VirtualProtect", "Failed to restore protection of NtProtectVirtualMemory: %d", GetLastError());
        }
      } else {
        errorf("VirtualProtect", "Failed to change protection of NtProtectVirtualMemory: %d", GetLastError());
      }
    }

    if (!LoadLibraryA(DLL_NAME_MAIN)) {
      DWORD error_code_main = GetLastError();
      if (!LoadLibraryA(DLL_NAME_FALLBACK)) {
        DWORD error_code_fallback = GetLastError();
        errorf("LoadLibraryA", "Failed to load both main ('%s' : %d) and fallback ('%s' : %d) modules",
               DLL_NAME_MAIN, error_code_main, DLL_NAME_FALLBACK, error_code_fallback);
      }
    }
  }

  prev_a = a;
}

static LONG WINAPI
veh_handler(EXCEPTION_POINTERS *exp)
{
  switch (exp->ExceptionRecord->ExceptionCode) {
    case STATUS_GUARD_PAGE_VIOLATION: {
      uint8_t *rip = (uint8_t *)exp->ContextRecord->Rip;
      if (rip == g_target) {
        on_crc(exp->ContextRecord);
        exp->ContextRecord->EFlags |= 0x100;
        return EXCEPTION_CONTINUE_EXECUTION;
      }
      exp->ContextRecord->EFlags |= 0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }

    case EXCEPTION_SINGLE_STEP: {
      if (g_page && g_page_size) {
        DWORD tmp;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(g_page, &mbi, sizeof(mbi))) {
          my_virtual_protect(g_page, g_page_size, mbi.Protect | PAGE_GUARD, &tmp);
        }
      }
      exp->ContextRecord->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

static uint8_t*
page_align(void* p)
{
  return (uint8_t*)((uintptr_t)p & ~((uintptr_t)g_page_size - 1));
}

static DWORD WINAPI
init(void)
{
  HMODULE h_ntdll = GetModuleHandleA("ntdll.dll");
  ASSERT(h_ntdll != NULL);

  void* nt_original = (void*)GetProcAddress(h_ntdll, "NtProtectVirtualMemory");
  ASSERT(nt_original != NULL);

  void* nt_copy = VirtualAlloc(0, NT_COPY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  ASSERT(nt_copy != NULL);

  memcpy(nt_copy, nt_original, NT_COPY_SIZE);
  g_nt_protect_virtual_memory = (nt_protect_virtual_memory_t)nt_copy;

  AddVectoredExceptionHandler(1, veh_handler);

  HMODULE module_base = GetModuleHandle(NULL);
  ASSERT(module_base != NULL);

  uint8_t* crc_addr = (uint8_t*)((uint64_t)module_base + (uint64_t)CRC_RVA);

  SYSTEM_INFO si;
  GetSystemInfo(&si);
  g_page_size = si.dwPageSize;
  g_target = crc_addr;
  g_page = page_align(crc_addr);

  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(g_page, &mbi, sizeof(mbi))) {
    errorf("VirtualQuery", "Failed to query page information: %d\n", GetLastError());
    return FALSE;
  }

  DWORD prot = mbi.Protect;
  ASSERT((prot & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)));

  DWORD new_prot = prot | PAGE_GUARD;
  DWORD old_prot = 0;
  if (!my_virtual_protect(g_page, g_page_size, new_prot, &old_prot)) {
    errorf("VirtualProtect", "Failed add page guard for %p (page addr: %p, page size: 0x%X): %d\n",
           crc_addr, g_page, g_page_size, GetLastError());
    return FALSE;
  }

  g_page_old_prot = old_prot;
  return TRUE;
}

BOOL APIENTRY
DllMain(HMODULE h_module, DWORD reason, LPVOID reserved)
{
  (void)reserved;

  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(h_module);
    return init();
  }
  return TRUE;
}
