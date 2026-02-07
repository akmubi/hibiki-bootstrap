#include "proxy.h"

#include <winternl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#define NT_COPY_SIZE 0x20
#define DLL_MAIN     "hibiki.dll"
#define DLL_FALLBACK "UE4SS.dll"

typedef NTSTATUS (NTAPI *NtProtectVirtualMemory_t)(
  HANDLE,
  PVOID *,
  PSIZE_T,
  ULONG,
  PULONG
);

static uint8_t                  g_nt_orig_bytes[NT_COPY_SIZE];
static uint8_t                 *g_nt_orig_addr  = NULL;
static NtProtectVirtualMemory_t g_nt_copy       = NULL;

static void
save_nt_protect_virtual_memory(void)
{
  HMODULE h = GetModuleHandleA("ntdll.dll");
  void *orig_addr = (void *)GetProcAddress(h, "NtProtectVirtualMemory");
  if (!orig_addr) {
    return;
  }

  memcpy(g_nt_orig_bytes, orig_addr, NT_COPY_SIZE);
  g_nt_orig_addr = orig_addr;

  void *nt_copy = VirtualAlloc(NULL, NT_COPY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!nt_copy) {
    return;
  }

  memcpy(nt_copy, g_nt_orig_addr, NT_COPY_SIZE);
  g_nt_copy = (NtProtectVirtualMemory_t)nt_copy;
}

static void
restore_nt_protect_virtual_memory(void)
{
  if (!g_nt_orig_addr || !g_nt_copy) {
    return;
  }

  if (memcmp(g_nt_orig_addr, g_nt_orig_bytes, NT_COPY_SIZE) == 0) {
    return;
  }

  PVOID  addr = g_nt_orig_addr;
  SIZE_T size = NT_COPY_SIZE;
  DWORD  old_prot;

  g_nt_copy(GetCurrentProcess(), &addr, &size, PAGE_EXECUTE_READWRITE, &old_prot);
  memcpy(g_nt_orig_addr, g_nt_orig_bytes, NT_COPY_SIZE);

  addr = g_nt_orig_addr;
  size = NT_COPY_SIZE;
  g_nt_copy(GetCurrentProcess(), &addr, &size, old_prot, &old_prot);
}

static void
setup_debug_console(void)
{
  AllocConsole();
  FILE *f_out, *f_err;
  freopen_s(&f_out, "CONOUT$", "w", stdout);
  freopen_s(&f_err, "CONOUT$", "w", stderr);
  LOG("[proxy] Debug console initialized\n");
}

static DWORD WINAPI
watcher_thread(LPVOID param)
{
  (void)param;
 
  LOG("[watcher] Polling NtProtectVirtualMemory...\n");
  while (memcmp(g_nt_orig_addr, g_nt_orig_bytes, NT_COPY_SIZE) == 0) {
    Sleep(1);
  }

  LOG("[watcher] NtProtectVirtualMemory was modified! Restoring...\n");
  restore_nt_protect_virtual_memory();

  LOG("[watcher] Loading %s...\n", DLL_MAIN);
  HMODULE h = LoadLibraryA(DLL_MAIN);
  if (!h) {
    DWORD main_err = GetLastError();
    LOG("[watcher] Failed to load %s (error %lu), trying %s...\n", DLL_MAIN, main_err, DLL_FALLBACK);
    h = LoadLibraryA(DLL_FALLBACK);
    if (!h) {
      DWORD fallback_err = GetLastError();
      LOG("[watcher] Failed to load fallback: %s: %lu, %s: %lu\n",
        DLL_MAIN, main_err, DLL_FALLBACK, fallback_err);
    } else {
      LOG("[watcher] Successfully loaded %s at %p\n", DLL_FALLBACK, (void*)h);
    }
  } else {
    LOG("[watcher] Successfully loaded %s at %p\n", DLL_MAIN, (void*)h);
  }
  return 0;
}

BOOL WINAPI
DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
  (void)hinst; (void)reserved;
  
  if (reason == DLL_PROCESS_ATTACH) {
#ifdef DEBUG
    setup_debug_console();
#endif
    
    save_nt_protect_virtual_memory();

    LOG("[proxy] NtProtectVirualMemory original bytes:");
    for (int i = 0; i < NT_COPY_SIZE; ++i) {
      LOG(" %02X", g_nt_orig_bytes[i]);
    }
    LOG("\n");

    proxy_load_original_dll();
    proxy_setup_functions();

    CreateThread(NULL, 0, watcher_thread, NULL, 0, NULL);
  } else if (reason == DLL_PROCESS_DETACH) {
    if (original_dll) {
      FreeLibrary(original_dll);
    }
  }
  
  return TRUE;
}