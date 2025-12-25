@echo off
setlocal EnableDelayedExpansion

echo ================================
echo    Amalgam Development Setup
echo ================================
echo.

:: Check for Windows 10/11
for /f "tokens=4-5 delims=. " %%i in ('ver') do set VERSION=%%i.%%j
echo Detected Windows version: %VERSION%

:: Check for Visual Studio 2022
echo.
echo [1/5] Checking Visual Studio 2022...
set VS2022_PATH=""
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set VS2022_PATH="%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    echo Found: Visual Studio 2022 Enterprise
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set VS2022_PATH="%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    echo Found: Visual Studio 2022 Professional
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set VS2022_PATH="%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    echo Found: Visual Studio 2022 Community
) else (
    echo ERROR: Visual Studio 2022 not found!
    echo Please install Visual Studio 2022 with C++ development tools.
    echo Download from: https://visualstudio.microsoft.com/downloads/
    pause
    exit /b 1
)

:: Check for Git
echo.
echo [2/5] Checking Git...
git --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Git not found!
    echo Please install Git from: https://git-scm.com/download/win
    pause
    exit /b 1
) else (
    echo Git is installed
)

:: Setup vcpkg
echo.
echo [3/5] Setting up vcpkg...
if not exist "vcpkg" (
    echo Cloning vcpkg...
    git clone https://github.com/Microsoft/vcpkg.git
    if errorlevel 1 (
        echo ERROR: Failed to clone vcpkg
        pause
        exit /b 1
    )
) else (
    echo vcpkg directory already exists
)

cd vcpkg

:: Bootstrap vcpkg
echo Bootstrapping vcpkg...
if not exist "vcpkg.exe" (
    call bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo ERROR: Failed to bootstrap vcpkg
        pause
        exit /b 1
    )
) else (
    echo vcpkg.exe already exists
)

:: Install dependencies
echo.
echo [4/5] Installing dependencies...

:: Clean any corrupted builds
echo Cleaning vcpkg build cache...
if exist "buildtrees" (
    rmdir /s /q buildtrees
    echo Build cache cleaned
)
if exist "packages\cpr_x64-windows-static" (
    rmdir /s /q packages\cpr_x64-windows-static
)
if exist "packages\curl_x64-windows-static" (
    rmdir /s /q packages\curl_x64-windows-static
)
if exist "packages\zlib_x64-windows-static" (
    rmdir /s /q packages\zlib_x64-windows-static
)

echo Installing cpr (C++ Requests library)...
vcpkg.exe install cpr:x64-windows-static --clean-after-build
if errorlevel 1 (
    echo ERROR: Failed to install cpr
    echo Retrying with verbose output...
    vcpkg.exe install cpr:x64-windows-static --clean-after-build --debug
    if errorlevel 1 (
        pause
        exit /b 1
    )
)

echo Installing nlohmann-json...
vcpkg.exe install nlohmann-json:x64-windows-static --clean-after-build
if errorlevel 1 (
    echo ERROR: Failed to install nlohmann-json
    pause
    exit /b 1
)

echo Installing freetype (font rendering library)...
vcpkg.exe install freetype:x64-windows-static --clean-after-build
if errorlevel 1 (
    echo ERROR: Failed to install freetype
    pause
    exit /b 1
)

echo Note: Dynamic versions not needed - using static linking only...

echo Integrating vcpkg with Visual Studio...
vcpkg.exe integrate install
if errorlevel 1 (
    echo WARNING: Failed to integrate vcpkg with Visual Studio
    echo You may need to run this as administrator
)

cd ..

:: Initialize submodules
echo.
echo [5/5] Initializing submodules...
echo Initializing AmalgamLoader and Blackbone submodules...
git submodule update --init --recursive
if errorlevel 1 (
    echo ERROR: Failed to initialize submodules
    pause
    exit /b 1
) else (
    echo Submodules initialized successfully
)

:: Restore NuGet packages
echo.
echo [6/6] Restoring NuGet packages...
where nuget >nul 2>&1
if errorlevel 1 (
    echo WARNING: NuGet not found in PATH
    echo Downloading NuGet...
    if not exist "nuget.exe" (
        powershell -Command "Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile 'nuget.exe'"
        if errorlevel 1 (
            echo ERROR: Failed to download NuGet
            pause
            exit /b 1
        )
    )
    echo Restoring packages with downloaded NuGet...
    nuget.exe restore Amalgam.sln
    if errorlevel 1 (
        echo ERROR: Failed to restore NuGet packages
        echo Retrying with detailed output...
        nuget.exe restore Amalgam.sln -Verbosity detailed
    )
) else (
    echo Restoring packages with system NuGet...
    nuget restore Amalgam.sln
    if errorlevel 1 (
        echo ERROR: Failed to restore NuGet packages
        echo Retrying with detailed output...
        nuget restore Amalgam.sln -Verbosity detailed
    )
)

:: Clean any existing Visual Studio build artifacts
echo.
echo Cleaning Visual Studio build artifacts...
if exist ".vs" (
    rmdir /s /q .vs
    echo .vs folder cleaned
)
if exist "Amalgam\x64" (
    rmdir /s /q Amalgam\x64
    echo Amalgam x64 build folder cleaned
)
if exist "build" (
    rmdir /s /q build
    echo Build folder cleaned
)

echo.
echo ================================
echo        Setup Complete!
echo ================================
echo.
echo You can now:
echo 1. Open Amalgam.sln in Visual Studio 2022
echo 2. Build the project (Release/x64 recommended)
echo 3. Output will be in: output\x64\Release\
echo 4. AmalgamLoader.exe includes runtime signature randomization
echo.
echo Available build configurations:
echo - Release
echo - ReleaseAVX2
echo - ReleaseFreetype  
echo - ReleaseFreetypeAVX2
echo.
echo Dependencies installed:
echo - cpr (C++ Requests) - x64-windows-static (for Amalgam DLL only)
echo - nlohmann-json - x64-windows-static (for Amalgam DLL only)
echo - freetype (font rendering) - x64-windows-static (for Amalgam DLL only)
echo - boost (via NuGet)
echo - libolm (embedded in source)
echo - AmalgamLoader (submodule - standalone executable)
echo - Blackbone (nested submodule in AmalgamLoader)
echo.
echo Build Features:
echo - Runtime signature randomization with embedded hash detection
echo - Generic naming (no identifying strings)
echo - Self-contained processing state (no external marker files)
echo - Static linking: Amalgam DLL embeds dependencies, AmalgamLoader is standalone
echo.
pause