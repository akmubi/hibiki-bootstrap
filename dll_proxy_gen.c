#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

// clang-format off
#define NOMINMAX
#include <windows.h>
#include <imagehlp.h>
#include <tchar.h>
// clang-format on

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "ImageHlp.lib")

#define MAX_NAMELEN 512

typedef struct {
  WORD ordinal;
  BOOL is_named;
  char name[MAX_NAMELEN];
} export_t;

typedef struct {
  export_t *entries;
  size_t    count;
  size_t    cap;
} export_list_t;

static int
export_list_new(export_list_t *list, size_t cap)
{
  export_t *entries = malloc(cap * sizeof(*entries));
  if (!entries) {
    return -1;
  }

  *list = (export_list_t){
    .entries = entries,
    .cap     = cap,
    .count   = 0,
  };
  return 0;
}

static void
export_list_delete(export_list_t *list)
{
  free(list->entries);
  *list = (export_list_t){0};
}

static int
cmp_ord(const void *a, const void *b)
{
  const export_t *ea = (const export_t *)a;
  const export_t *eb = (const export_t *)b;

  if (ea->ordinal < eb->ordinal) {
    return -1;
  }

  if (ea->ordinal > eb->ordinal) {
    return 1;
  }

  return 0;
}

static BOOL
dump_exports(const TCHAR *dll_path, export_list_t *out_list)
{
  BOOL result = FALSE;

  export_list_t exports = {0};

  HANDLE h = CreateFile(dll_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    _ftprintf(stderr, _T("CreateFile: %lu\n"), GetLastError());
    return result;
  }

  HANDLE m = CreateFileMapping(h, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!m) {
    _ftprintf(stderr, _T("CreateFileMapping: %lu\n"), GetLastError());
    goto close_file;
  }

  BYTE *base = (BYTE *)MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
  if (!base) {
    _ftprintf(stderr, _T("MapViewOfFile: %lu\n"), GetLastError());
    goto close_mapping;
  }

  ULONG                   dir_size = 0;
  IMAGE_EXPORT_DIRECTORY *exp      = ImageDirectoryEntryToData(base, FALSE, IMAGE_DIRECTORY_ENTRY_EXPORT, &dir_size);
  if (!exp) {
    _ftprintf(stderr, _T("No export directory (err=%lu)\n"), GetLastError());
    goto unmap_view;
  }

  IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)(base + 0);
  IMAGE_NT_HEADERS *nt  = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);

  DWORD *name_rvas = ImageRvaToVa(nt, base, exp->AddressOfNames, NULL);
  DWORD *func_rvas = ImageRvaToVa(nt, base, exp->AddressOfFunctions, NULL);
  WORD  *ord_table = ImageRvaToVa(nt, base, exp->AddressOfNameOrdinals, NULL);

  if (export_list_new(&exports, exp->NumberOfFunctions) < 0) {
    _ftprintf(stderr, _T("Out of memory\n"));
    goto unmap_view;
  }

  // track which ordinals already have names
  BYTE *has_name = calloc(exp->NumberOfFunctions, 1);
  if (!has_name) {
    _ftprintf(stderr, _T("Out of memory\n"));
    export_list_delete(&exports);
    goto unmap_view;
  }

  // named exports
  for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
    const char *nm  = ImageRvaToVa(nt, base, name_rvas[i], NULL);
    WORD        ord = (WORD)(exp->Base + ord_table[i]);
    export_t   *e   = &exports.entries[exports.count];

    e->ordinal  = ord;
    e->is_named = TRUE;
    e->name[0]  = 0;

    if (nm) {
      strncpy_s(e->name, sizeof(e->name), nm, sizeof(e->name) - 1);
      e->name[sizeof(e->name) - 1] = 0;
    }

    // mark EAT index as already emitted
    DWORD eat_idx = ord - exp->Base;
    if (eat_idx < exp->NumberOfFunctions) {
      has_name[eat_idx] = 1;
    }

    exports.count += 1;
  }

  // ordinal-only exports
  for (DWORD i = 0; i < exp->NumberOfFunctions; ++i) {
    WORD      ord = (WORD)(exp->Base + i);
    DWORD     rva = func_rvas[i];
    export_t *e   = &exports.entries[exports.count];

    // unimplemented slot
    if (rva == 0) {
      continue;
    }

    // already emitted as named
    if (has_name[i]) {
      continue;
    }

    e->ordinal  = ord;
    e->is_named = FALSE;

    // store synthetic name
    snprintf(e->name, MAX_NAMELEN, "ordinal%u", (unsigned)ord);

    exports.count += 1;
  }

  free(has_name);

  // sort list by ordinal
  qsort(exports.entries, exports.count, sizeof(*exports.entries), cmp_ord);

  if (out_list) {
    *out_list = exports;
  }

  result = TRUE;

unmap_view:
  UnmapViewOfFile(base);
close_mapping:
  CloseHandle(m);
close_file:
  CloseHandle(h);
  return result;
}

static void
path_filename_noext(const TCHAR *in, TCHAR *out, size_t out_cap)
{
  const TCHAR *p    = in;
  const TCHAR *last = in;

  for (; *p; ++p) {
    if (*p == _T('\\') || *p == _T('/')) {
      last = p + 1;
    }
  }

  _tcsncpy_s(out, out_cap, last, _TRUNCATE);

  TCHAR *dot = _tcsrchr(out, _T('.'));
  if (dot) {
    *dot = 0;
  }
}

int
_tmain(int argc, TCHAR *argv[])
{
  if (argc < 4) {
    _ftprintf(stderr, _T("Usage: %s <input_system_dll> <output_dir> <mod_loader_path>\n"), argv[0]);
    _ftprintf(stderr,
              _T("Example: %s C:\\Windows\\System32\\dwmapi.dll out ")
              _T("mods\\loader.dll\n"),
              argv[0]);
    return 1;
  }

  const TCHAR *input_dll  = argv[1];
  const TCHAR *out_dir    = argv[2];
  const TCHAR *loader_dll = argv[3];

  export_list_t exports = {0};
  if (!dump_exports(input_dll, &exports)) {
    return 2;
  }

  TCHAR base_noext[MAX_PATH] = {0};
  path_filename_noext(input_dll, base_noext, sizeof(base_noext));

  // build output file paths
  TCHAR def_path[MAX_PATH]  = {0};
  TCHAR masm_path[MAX_PATH] = {0};
  TCHAR gas_path[MAX_PATH]  = {0};
  TCHAR c_path[MAX_PATH]    = {0};

  _sntprintf_s(def_path, sizeof(def_path), _TRUNCATE, _T("%s\\%s.def"), out_dir, base_noext);
  _sntprintf_s(masm_path, sizeof(masm_path), _TRUNCATE, _T("%s\\%s.asm"), out_dir, base_noext);
  _sntprintf_s(gas_path, sizeof(gas_path), _TRUNCATE, _T("%s\\%s_gas.S"), out_dir, base_noext);
  _sntprintf_s(c_path, sizeof(c_path), _TRUNCATE, _T("%s\\dllmain.c"), out_dir);

  // DEF
  FILE *fdef = _tfopen(def_path, _T("wb"));
  if (!fdef) {
    _ftprintf(stderr, _T("Cannot write %s\n"), def_path);
    export_list_delete(&exports);
    return 3;
  }

#ifdef UNICODE
  fwprintf(fdef, L"LIBRARY %s\nEXPORTS\n", base_noext);
#else
  fprintf(fdef, "LIBRARY %s\nEXPORTS\n", base_noext);
#endif

  for (size_t i = 0; i < exports.count; ++i) {
    export_t *e = &exports.entries[i];

    if (e->is_named) {
      fprintf(fdef, "  %s=f%zu @%u\n", e->name, i, e->ordinal);
    } else {
      // ordinal-only export: export by ordinal NONAME and attach stub
      fprintf(fdef, "  f%zu @%u NONAME\n", i, e->ordinal);
    }
  }
  fclose(fdef);

  // ASM stubs (x64 MASM): jmp qword ptr [m_procs + 8*i]
  FILE *fmasm = _tfopen(masm_path, _T("wb"));
  if (!fmasm) {
    _ftprintf(stderr, _T("Cannot write %s\n"), masm_path);
    export_list_delete(&exports);
    return 4;
  }

  fprintf(fmasm, "option casemap:none\n");
  fprintf(fmasm, "EXTERN m_procs:QWORD\n");
  fprintf(fmasm, ".code\n");

  for (size_t i = 0; i < exports.count; i++) {
    fprintf(fmasm, "f%zu PROC\n", i);
    fprintf(fmasm, "    jmp QWORD PTR [m_procs + %zu*8]\n", i);
    fprintf(fmasm, "f%zu ENDP\n", i);
  }

  fprintf(fmasm, "END\n");
  fclose(fmasm);

  // GAS stubs (x64 GNU as): load m_procs[i] and jmp through it
  FILE *fgas = _tfopen(gas_path, _T("wb"));
  if (!fgas) {
    _ftprintf(stderr, _T("Cannot write %s\n"), gas_path);
    export_list_delete(&exports);
    return 4;
  }

  fprintf(fgas, ".text\n");
  fprintf(fgas, ".extern m_procs\n\n");

  for (size_t i = 0; i < exports.count; ++i) {
    unsigned long long offset = (unsigned long long)i * 8ULL;

    fprintf(fgas, ".globl f%zu\n", i);
    fprintf(fgas, "f%zu:\n", i);
    fprintf(fgas, "    mov m_procs+%llu(%%rip), %%rax\n", offset);
    fprintf(fgas, "    jmp *%%rax\n\n");
  }

  fclose(fgas);

  // dllmain.c
  FILE *fc = _tfopen(c_path, _T("wb"));
  if (!fc) {
    _ftprintf(stderr, _T("Cannot write %s\n"), c_path);
    export_list_delete(&exports);
    return 5;
  }

  #if 0
  #endif

  fprintf(fc,
          "#ifndef WIN32_LEAN_AND_MEAN\n"
          "#define WIN32_LEAN_AND_MEAN\n"
          "#endif\n"
          "#include <windows.h>\n"
          "#include <stdio.h>\n"
          "#include <shlwapi.h>\n"
          "#pragma comment(lib, \"Shlwapi.lib\")\n\n"
          "HMODULE original_dll = NULL;\n"
          "void   *m_procs[%zu] = {0};\n"
          "\n"
          "static void\n"
          "load_original_dll(void)\n"
          "{\n"
          "  if (original_dll) {\n"
          "    return;\n"
          "  }\n\n"
          "  wchar_t sysdir[MAX_PATH];\n"
          "  if (!GetSystemDirectoryW(sysdir, MAX_PATH)) {\n"
          "    return;\n"
          "  }\n\n"
          "  wchar_t path[MAX_PATH];\n"
          "  lstrcpynW(path, sysdir, MAX_PATH);\n"
          "  if (!PathAppendW(path, L\"\\\\%s.dll\")) {\n"
          "    return;\n"
          "  }\n\n"
          "  original_dll = LoadLibraryW(path);\n"
          "}\n"
          "\n"
          "static void setup_functions(void) {\n"
          "  if (!original_dll) {\n"
          "    return;\n"
          "  }\n",
          (unsigned long long)exports.count,
          base_noext);

  for (size_t i = 0; i < exports.count; i++) {
    export_t *e = &exports.entries[i];
    if (e->is_named) {
      fprintf(fc, "  m_procs[%zu] = (void*)GetProcAddress(original_dll, \"%s\");\n", i, e->name);
    } else {
      fprintf(fc, "  m_procs[%zu] = (void*)GetProcAddress(original_dll, (LPCSTR)%u);\n", i, e->ordinal);
    }
  }

#ifdef UNICODE
  _ftprintf(fc,
            "}\n\n"
            "BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)\n"
            "{\n"
            "  if (reason == DLL_PROCESS_ATTACH) {\n"
            "    DisableThreadLibraryCalls(hinst);\n"
            "    load_original_dll();\n"
            "    setup_functions();\n"
            "    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, "
            "(LPVOID)L\"%ls\", 0, NULL);\n"
            "  } else if (reason == DLL_PROCESS_DETACH) {\n"
            "    if (original_dll) {\n"
            "      FreeLibrary(original_dll);\n"
            "    }\n"
            "  }\n"
            "  return TRUE;\n"
            "}\n",
            loader_dll);
#else
  fprintf(fc,
          "}\n\n"
          "BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)\n"
          "{\n"
          "  if (reason == DLL_PROCESS_ATTACH) {\n"
          "    DisableThreadLibraryCalls(hinst);\n"
          "    load_original_dll();\n"
          "    setup_functions();\n"
          "    HMODULE h_mod = LoadLibraryA(\"%s\");\n"
          "    if (!h_mod) {\n"
          "      char buf[256];\n"
          "      snprintf(buf, sizeof(buf), \"LoadLibrarayA failed (error: %%lu)\", GetLastError());\n"
          "      MessageBox(NULL, buf, \"LoadLibraryA\", MB_OK);\n"
          "    }\n"
          "  } else if (reason == DLL_PROCESS_DETACH) {\n"
          "    if (original_dll) {\n"
          "      FreeLibrary(original_dll);\n"
          "    }\n"
          "  }\n"
          "  return TRUE;\n"
          "}\n",
          loader_dll);
#endif
  fclose(fc);

  _tprintf(_T("Wrote:\n  %s\n  %s\n  %s\n  %s\n"), def_path, masm_path, gas_path, c_path);
  export_list_delete(&exports);
  return 0;
}
