[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$dependencyArchive = Join-Path $repositoryRoot 'src\HPL2\dependencies.zip'
$dependencyDirectory = Join-Path $repositoryRoot 'src\HPL2\dependencies'
$dependencyHeader = Join-Path $dependencyDirectory 'include\SDL2\SDL.h'
$dependencyLibrary = Join-Path $dependencyDirectory 'lib\win32\SDL2.lib'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer was not found. Install Visual Studio 2026 with the Desktop development with C++ workload.'
}

$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) {
    throw 'The Visual Studio C++ x86/x64 build tools are not installed. Add the Desktop development with C++ workload.'
}

$msbuild = Join-Path $visualStudio 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild was not found at $msbuild. Repair the Visual Studio installation."
}

if (-not (Test-Path -LiteralPath $dependencyHeader) -or -not (Test-Path -LiteralPath $dependencyLibrary)) {
    Write-Host 'Extracting bundled HPL2 dependencies...'
    Expand-Archive -LiteralPath $dependencyArchive -DestinationPath (Split-Path -Parent $dependencyDirectory) -Force
}

$commonArguments = @(
    '/m',
    '/nologo',
    '/p:Configuration=Release',
    '/p:Platform=Win32'
)

Write-Host 'Building HPL2 (Release|Win32)...'
& $msbuild (Join-Path $repositoryRoot 'src\HPL2\core\_HPL2_2010.vcxproj') @commonArguments
if ($LASTEXITCODE -ne 0) {
    throw "HPL2 build failed with exit code $LASTEXITCODE."
}

Write-Host 'Building Amnesia (Release|Win32)...'
& $msbuild (Join-Path $repositoryRoot 'src\amnesia\src\game\Lux.vcxproj') @commonArguments
if ($LASTEXITCODE -ne 0) {
    throw "Amnesia build failed with exit code $LASTEXITCODE."
}

Write-Host "Build complete: $(Join-Path $repositoryRoot 'artifacts\Release\Amnesia.exe')"
