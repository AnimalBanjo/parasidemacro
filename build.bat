@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
pushd "%ROOT%"

set "SRC=src\main.cpp"
set "IMGUI=imgui"
set "RC=res\resource.rc"
set "OBJDIR=build"
set "OUT=Paradise.exe"

set "CONFIG=Release"
if /I "%~1"=="debug" set "CONFIG=Debug"

where cl.exe >nul 2>nul
if not errorlevel 1 goto :have_cl

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [!] vswhere.exe not found. Install Visual Studio 2022 with "Desktop development with C++".
    goto :fail
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * ^
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
    -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo [!] No Visual Studio with C++ tools found.
    goto :fail
)
echo [*] Using Visual Studio at: %VSPATH%
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 goto :fail

:have_cl

if not exist "%SRC%"         ( echo [!] Missing %SRC% & goto :fail )
if not exist "%RC%"          ( echo [!] Missing %RC%  & goto :fail )
if not exist "%IMGUI%\imgui.cpp" (
    echo [!] Missing ImGui sources at "%IMGUI%"
    goto :fail
)

if not exist "%OBJDIR%" mkdir "%OBJDIR%" >nul

set "CFLAGS=/nologo /std:c++17 /EHsc /DWIN32_LEAN_AND_MEAN /I"%IMGUI%" /I"src""
set "CFLAGS=%CFLAGS% /W3 /wd4244 /wd4267 /wd4996"

if /I "%CONFIG%"=="Debug" (
    set "CFLAGS=%CFLAGS% /Od /Zi /MTd /RTC1"
    set "LDFLAGS=/DEBUG"
) else (
    set "CFLAGS=%CFLAGS% /O2 /Oi /GL /DNDEBUG /MT"
    set "LDFLAGS=/LTCG /OPT:REF /OPT:ICF"
)

echo [*] Compiling resources: %RC%
rc /nologo /fo "%OBJDIR%\resource.res" "%RC%"
if errorlevel 1 goto :fail

echo [*] Compiling main.cpp + ImGui (%CONFIG%, x64) ...

cl %CFLAGS% ^
   /Fo"%OBJDIR%\\" /Fd"%OBJDIR%\Paradise.pdb" ^
   "%SRC%" ^
   "%IMGUI%\imgui.cpp" ^
   "%IMGUI%\imgui_draw.cpp" ^
   "%IMGUI%\imgui_tables.cpp" ^
   "%IMGUI%\imgui_widgets.cpp" ^
   "%IMGUI%\imgui_impl_win32.cpp" ^
   "%IMGUI%\imgui_impl_dx9.cpp" ^
   "%OBJDIR%\resource.res" ^
   /link /OUT:"%OUT%" /SUBSYSTEM:WINDOWS %LDFLAGS% ^
   winmm.lib d3d9.lib user32.lib gdi32.lib dwmapi.lib ^
   windowscodecs.lib ole32.lib advapi32.lib shell32.lib

if errorlevel 1 goto :fail

echo.
echo [OK] Built %OUT%  (%CONFIG%, x64)
popd
endlocal
exit /b 0

:fail
echo.
echo [FAILED] Build aborted.
popd
endlocal
exit /b 1