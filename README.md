# Hibiki Bootstrap DLL

## Disclaimer
This project is provided for educational, research, and authorized modding purposes only.
You are responsible for ensuring that your use of this software complies with all applicable laws, licenses, and terms of service of any third-party software involved.

The authors do not endorse or support unauthorized modification, redistribution, or circumvention of software protections.

## Usage
Copy the generated DLL files into the game's main folder. Example:
```
C:\Program Files (x86)\Steam\steamapps\common\Hi-Fi RUSH\Hibiki\Binaries\Win64`
```

## Target DLL naming and load order
At runtime, the bootstrap DLL attempts to load the target DLL in the following order:
- `UE4SS.dll`
- `hibiki.dll` (fallback)

### ⚠ Important
- Only **one** DLL proxy can be used at a time.
- If another proxy DLL is already present in the game folder, remove it first, or the game may fail to start or crash.
- For **UE4SS** specifically, make sure to remove `dwmapi.dll`.

## TL;DR
- Copy the DLLs into the game's `Win64` folder.
- If it's not `UE4SS.dll` - rename the DLL you want to load to `hibiki.dll`.
- Only one proxy DLL is allowed - remove any others (for UE4SS, delete `dwmapi.dll`)

## Example Folder Layout
If you're using **UE4SS**, your folder should look like this:
```
Hi-Fi RUSH
└─ Hibiki
   └─ Binaries
      └─ Win64
         ├─ hibiki_bootstrap.dll
         ├─ UE4SS.dll
         ├─ XAPOFX1_5.dll
         ├─ Hi-Fi-RUSH.exe
         └─ ...
```

If you're using something else, your folder should look like this:
```
Hi-Fi RUSH
└─ Hibiki
   └─ Binaries
      └─ Win64
         ├─ hibiki_bootstrap.dll
         ├─ hibiki.dll             <----- HERE
         ├─ XAPOFX1_5.dll
         ├─ Hi-Fi-RUSH.exe
         └─ ...
```

## Requirements
### Supported platforms
- Windows (x86_64): main platform
- Cross-build (non-Windows hosts): supported *only if* you provide the target DLL under `external/` (see below)

### Tooling
- CMake: 3.20+
- Compiler / build tools
  - MSVC (Visual Studio 2019/2022 recommended) for native Windows builds
  - Alternatively MinGW-w64 for GCC-based builds on cross environments

### Assembly (MSVC builds)
When building with MSVC:
- MASM is enabled (enable_language(ASM_MASM)), so you need a Visual Studio installation that includes MSVC + MASM.

### Input DLL for generation
The build generates proxy sources by inspecting a "real" DLL:
- Native Windows build: uses the host OS copy automatically (`%SystemRoot%\System32\XAPOFX1_5.dll`)
- Cross-build: you must provide `external/XAPOFX1_5.dll` (under the source tree)

## Build (Windows / MSVC)
### Configure
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

### Build (Release)
```
cmake --build build --config Release
```

### Build (Debug)
```
cmake --build build --config Debug
```

## Build (Cross-build)
Cross-building is supported, but you must supply the "real" DLL under the repository:
1. Place the input DLL here:
```
external/XAPOFX1_5.dll
```

2. Configure & build using cross toolchain. Example for MinGW-w64:
```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
