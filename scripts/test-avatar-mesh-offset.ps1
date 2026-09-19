[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer was not found.'
}

$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) {
    throw 'The Visual Studio C++ x86/x64 build tools are not installed.'
}

# Unlike the other harnesses, this one links the engine's math, so HPL2 must be built first.
$hplDirectory = Join-Path $repositoryRoot 'src\HPL2'
$hplLibrary = Join-Path $hplDirectory 'lib\HPL2_2010.lib'
if (-not (Test-Path -LiteralPath $hplLibrary)) {
    throw 'The HPL2 library was not found. Run scripts\build-windows.ps1 first.'
}

$developerShell = Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Import-Module $developerShell
Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x86 -host_arch=x64' | Out-Null

$testDirectory = Join-Path $repositoryRoot 'tests\avatar-mesh-offset'
$sourceDirectory = Join-Path $repositoryRoot 'src\amnesia\src\game'
$outputDirectory = Join-Path $repositoryRoot 'artifacts\tests'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executable = Join-Path $outputDirectory 'AvatarMeshOffsetTests.exe'
$dependencyLibraryPath = '/LIBPATH:' + (Join-Path $hplDirectory 'dependencies\lib\win32')

# The math never calls SDL2 or zlib, so delay-loading them lets the harness run without the game's DLLs.
& cl /nologo /EHsc /MD /W3 /WX /DWIN32 /DNDEBUG /DUSE_SDL2 /DWINDOWS_IGNORE_PACKING_MISMATCH `
    /I $sourceDirectory /I (Join-Path $hplDirectory 'core\include') /I (Join-Path $hplDirectory 'dependencies\include') `
    (Join-Path $testDirectory 'AvatarMeshOffsetTests.cpp') `
    /Fo:"$outputDirectory\" `
    /Fe:$executable /link $dependencyLibraryPath $hplLibrary `
    legacy_stdio_definitions.lib delayimp.lib /DELAYLOAD:SDL2.dll /DELAYLOAD:zlibwapi.dll
if ($LASTEXITCODE -ne 0) {
    throw "Avatar mesh offset harness compilation failed with exit code $LASTEXITCODE."
}

& $executable
if ($LASTEXITCODE -ne 0) {
    throw "Avatar mesh offset test failed with exit code $LASTEXITCODE."
}
