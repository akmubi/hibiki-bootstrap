@echo off
setlocal
cd /d "%~dp0"

set OUT_DIR=build
set TARGET_DLL=XAPOFX1_5
set REAL_DLL=%SystemRoot%\System32\%TARGET_DLL%.dll

mkdir %OUT_DIR% 2>nul

echo [1/5] Compiling dll_proxy_gen...
cl /nologo /O2 /W4 /D_CRT_SECURE_NO_WARNINGS dll_proxy_gen.c /Fo:%OUT_DIR%\dll_proxy_gen.obj /Fe:%OUT_DIR%\dll_proxy_gen.exe ImageHlp.lib
if errorlevel 1 exit /b 1

echo [2/5] Generating proxy files...
%OUT_DIR%\dll_proxy_gen.exe "%REAL_DLL%" "%OUT_DIR%"
if errorlevel 1 exit /b 1

echo [3/5] Assembling stubs...
ml64 /nologo /c /Fo %OUT_DIR%\stubs.obj %OUT_DIR%\%TARGET_DLL%.asm
if errorlevel 1 exit /b 1

echo [4/5] Compiling main.c...
cl /nologo /O2 /MD /W3 /c /I%OUT_DIR% /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /Fo:%OUT_DIR%\main.obj main.c
if errorlevel 1 exit /b 1

echo [5/5] Linking %TARGET_DLL%.dll...
link /nologo /DLL /OUT:%OUT_DIR%\%TARGET_DLL%.dll /DEF:%OUT_DIR%\%TARGET_DLL%.def /NOIMPLIB %OUT_DIR%\main.obj %OUT_DIR%\stubs.obj Shlwapi.lib kernel32.lib user32.lib
if errorlevel 1 exit /b 1

echo.
echo Done: %OUT_DIR%\%TARGET_DLL%.dll