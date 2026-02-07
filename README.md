# Hibiki Bootstrap DLL

DLL proxy loader for Hi-Fi RUSH that bypasses packed executable protections to enable modding.

## Disclaimer
This project is provided for educational, research, and authorized modding purposes only.
You are responsible for ensuring that your use of this software complies with all applicable laws, licenses, and terms of service of any third-party software involved.

The authors do not endorse or support unauthorized modification, redistribution, or circumvention of software protections.

## Installation

1. Copy `XAPOFX1_5.dll` to `<game>/Hibiki/Binaries/Win64/`
2. Place `hibiki.dll` or `UE4SS.dll` in the same folder
3. **Remove any other proxy DLLs** (e.g. `dwmapi.dll`, `dinput8.dll` etc.)

## Building

Requirements:
- MSVC (Visual Studio 2019/2022)
- Windows SDK

Run `build.bat` from x64 Native Tools Command Prompt

## Configuration

Edit `main.c`:
```c
#define DLL_MAIN     "hibiki.dll"      // Change target DLL here
#define DLL_FALLBACK "UE4SS.dll"
```

Edit `build.bat`:
```batch
REM Change proxy DLL here
set TARGET_DLL=XAPOFX1_5
```

## How It Works

1. Proxies `XAPOFX1_5.dll` to load early
2. Saves original `NtProtectVirtualMemory` bytes before unpacker modifies it
3. Polls every 1ms to detect when unpacker finishes
4. Restores NtPVM and loads `hibiki.dll` or `UE4SS.dll`

## Important

- ⚠️ Only one proxy DLL allowed - remove others or game crashes
- Built for Hi-Fi RUSH (patch 10) - may need adjustments for other versions
