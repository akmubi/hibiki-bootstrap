# Hibiki Bootstrap DLL

DLL proxy loader for Hi-Fi RUSH that bypasses packed executable protections to enable modding.

## Disclaimer
This project is provided for educational, research, and authorized modding purposes only.
You are responsible for ensuring that your use of this software complies with all applicable laws, licenses, and terms of service of any third-party software involved.

The authors do not endorse or support unauthorized modification, redistribution, or circumvention of software protections.

## Installation

### Steam/Epic Games
1. Copy `XAPOFX1_5.dll` to `<game>/Hibiki/Binaries/Win64/`
2. Place `hibiki.dll` or `UE4SS.dll` in the same folder
3. **Remove any other proxy DLLs** (e.g. `dwmapi.dll`, `dinput8.dll`)

### Xbox
1. Copy `dsound.dll` to `<game>/Hibiki/Binaries/Win64/`
2. Place `hibiki.dll` or `UE4SS.dll` in the same folder

## Building

### Requirements
- MSVC (Visual Studio 2019/2022)
- Windows SDK

### Build Commands

Run from **x64 Native Tools Command Prompt**:
```batch
# For Steam/Epic Games
build.bat steam
# or
build.bat epic

# For Xbox
build.bat xbox
```

**Outputs:**
- `out/steam/XAPOFX1_5.dll` - Steam version
- `out/epic/XAPOFX1_5.dll` - Epic Games version
- `out/xbox/dsound.dll` - Xbox version
