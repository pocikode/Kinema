@echo off
REM ──────────────────────────────────────────────────────────────
REM  package.bat
REM  Build Kinema from source and create a platform installer.
REM
REM  Usage:
REM    Windows: .\package.bat
REM ──────────────────────────────────────────────────────────────

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "PACKAGE_DIR=%SCRIPT_DIR%package"

REM Determine vcpkg toolchain (adjust if your vcpkg is elsewhere)
if exist "%SCRIPT_DIR%vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_CHAIN=%SCRIPT_DIR%vcpkg\scripts\buildsystems\vcpkg.cmake"
) else (
    echo [INFO] vcpkg not found in project root.
    echo [INFO] Pass -DCMAKE_TOOLCHAIN_FILE manually if needed.
    set "VCPKG_CHAIN="
)

echo === Configuring Kinema (Release) ===
if defined VCPKG_CHAIN (
    cmake -B "%BUILD_DIR%" -S "%SCRIPT_DIR%" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_TOOLCHAIN_FILE="%VCPKG_CHAIN%"
) else (
    cmake -B "%BUILD_DIR%" -S "%SCRIPT_DIR%" -DCMAKE_BUILD_TYPE=Release
)
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Building Kinema ===
cmake --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Creating Installer Package ===
if not exist "%PACKAGE_DIR%" mkdir "%PACKAGE_DIR%"
cpack --config "%BUILD_DIR%\CPackConfig.cmake" -B "%PACKAGE_DIR%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Package created! ===
dir "%PACKAGE_DIR%"
