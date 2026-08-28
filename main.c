#include "proxy.h"

#include <winternl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#ifndef BOOTSTRAP_DLL
#define BOOTSTRAP_DLL "hibiki.dll"
#endif

#if defined STEAM || defined EPIC
#define NT_COPY_SIZE 0x20

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

static bool
save_nt_protect_virtual_memory(void)
{
  HMODULE h = GetModuleHandleA("ntdll.dll");
  void *orig_addr = (void *)GetProcAddress(h, "NtProtectVirtualMemory");
  if (!orig_addr) {
    return false;
  }

  memcpy(g_nt_orig_bytes, orig_addr, NT_COPY_SIZE);
  g_nt_orig_addr = orig_addr;

  void *nt_copy = VirtualAlloc(NULL, NT_COPY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!nt_copy) {
    g_nt_orig_addr = NULL;
    return false;
  }

  memcpy(nt_copy, g_nt_orig_addr, NT_COPY_SIZE);
  g_nt_copy = (NtProtectVirtualMemory_t)nt_copy;
  return true;
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

  LOG("[watcher] Loading %s...\n", BOOTSTRAP_DLL);
  HMODULE h = LoadLibraryA(BOOTSTRAP_DLL);
  if (!h) {
    LOG("[watcher] Failed to load %s (error %lu)\n", BOOTSTRAP_DLL, GetLastError());
  } else {
    LOG("[watcher] Successfully loaded %s at %p\n", BOOTSTRAP_DLL, (void *)h);
  }
  return 0;
}
#endif

static void
setup_debug_console(void)
{
  AllocConsole();
  FILE *f_out, *f_err;
  freopen_s(&f_out, "CONOUT$", "w", stdout);
  freopen_s(&f_err, "CONOUT$", "w", stderr);
  LOG("[proxy] Debug console initialized\n");
}

BOOL WINAPI
DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
  (void)hinst; (void)reserved;
  
  if (reason == DLL_PROCESS_ATTACH) {
#ifdef DEBUG
    setup_debug_console();
#endif

#if defined STEAM || defined EPIC
    bool watcher_ready = save_nt_protect_virtual_memory();
    if (watcher_ready) {
      LOG("[proxy] NtProtectVirtualMemory original bytes:");
      for (int i = 0; i < NT_COPY_SIZE; ++i) {
        LOG(" %02X", g_nt_orig_bytes[i]);
      }
      LOG("\n");
    }
#endif

    proxy_load_original_dll();
    proxy_setup_functions();

#if defined STEAM || defined EPIC
    if (watcher_ready) {
      HANDLE thread = CreateThread(NULL, 0, watcher_thread, NULL, 0, NULL);
      if (thread) {
        CloseHandle(thread);
      } else {
        LOG("[proxy] Failed to create watcher thread (error %lu)\n", GetLastError());
      }
    } else {
      LOG("[proxy] Failed to initialize NtProtectVirtualMemory watcher\n");
    }
#else
    HMODULE h = LoadLibraryA(BOOTSTRAP_DLL);
    if (!h) {
      LOG("[proxy] Failed to load %s (error %lu)\n", BOOTSTRAP_DLL, GetLastError());
    } else {
      LOG("[proxy] Successfully loaded %s at %p\n", BOOTSTRAP_DLL, (void *)h);
    }
#endif
  } else if (reason == DLL_PROCESS_DETACH) {
    if (original_dll) {
      FreeLibrary(original_dll);
    }
  }
  
  return TRUE;
}
