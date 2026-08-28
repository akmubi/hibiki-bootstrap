# Hibiki Bootstrap DLL

DLL proxy loader for Hi-Fi RUSH that bypasses packed executable protections to enable modding.

## Disclaimer
This project is provided for educational, research, and authorized modding purposes only.
You are responsible for ensuring that your use of this software complies with all applicable laws, licenses, and terms of service of any third-party software involved.

The authors do not endorse or support unauthorized modification, redistribution, or circumvention of software protections.

## Installation

Use a bootstrap build configured for the DLL you want to load.

For example, a bootstrap built for `foo.dll` will only try to load `foo.dll`.

### Steam/Epic Games
1. Copy `XAPOFX1_5.dll` to `<game>/Hibiki/Binaries/Win64/`.
2. Place target DLL, such as `overdub.dll` or `UE4SS.dll` in the same directory.
3. **Remove any other proxy DLLs** (e.g. `dwmapi.dll`, `dinput8.dll`).

Example:
```
<game>/Hibiki/Binaries/Win64/
|-- XAPOFX1_5.dll
`-- overdub.dll
```

### Xbox
1. Copy `dsound.dll` to `<game>/Hibiki/Binaries/WinGDK/`.
2. Place target DLL in the same directory.
Example:
```
<game>/Hibiki/Binaries/WinGDK/
|-- dsound.dll
`-- overdub.dll
```

## Building

### Requirements
- Visual Studio 2019/2022
- MSVC x64 build tools
- Windows SDK

### Build Commands

Run the commands from an **x64 Native Tools Command Prompt for Visual Studio**.

Steam:
```batch
build.bat steam <target-dll-name>
```

Epic Games:
```batch
build.bat epic <target-dll-name>
```

Xbox:
```batch
build.bat xbox <target-dll-name>
```

Replace `<target-dll-name>` with the exact DLL filename that the bootstrap should load.

The DLL argument is optional. When omitted, it defaults to `hibiki.dll`:
```batch
build.bat steam
```

**Outputs:**
- `build/steam/XAPOFX1_5.dll`
- `build/epic/XAPOFX1_5.dll`
- `build/xbox/dsound.dll`

