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

$developerShell = Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Import-Module $developerShell
Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x86 -host_arch=x64' | Out-Null

$testDirectory = Join-Path $repositoryRoot 'tests\chat-model'
$sourceDirectory = Join-Path $repositoryRoot 'src\amnesia\src\game'
$outputDirectory = Join-Path $repositoryRoot 'artifacts\tests'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executable = Join-Path $outputDirectory 'ChatModelTests.exe'

& cl /nologo /EHsc /W4 /WX /I $sourceDirectory `
    (Join-Path $testDirectory 'ChatModelTests.cpp') `
    (Join-Path $sourceDirectory 'ChatModel.cpp') `
    /Fo:"$outputDirectory\" `
    /Fe:$executable
if ($LASTEXITCODE -ne 0) {
    throw "Chat model harness compilation failed with exit code $LASTEXITCODE."
}

& $executable
if ($LASTEXITCODE -ne 0) {
    throw "Chat model test failed with exit code $LASTEXITCODE."
}
