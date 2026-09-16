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

$testDirectory = Join-Path $repositoryRoot 'tests\legacy-game-interaction-protocol'
$sourceDirectory = Join-Path $repositoryRoot 'src\amnesia\src\game'
$outputDirectory = Join-Path $repositoryRoot 'artifacts\tests'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executable = Join-Path $outputDirectory 'LegacyProtocolContractTests.exe'

& cl /nologo /EHsc /W4 /WX /D_CRT_SECURE_NO_WARNINGS /I $sourceDirectory `
    (Join-Path $testDirectory 'LegacyProtocolContractTests.cpp') `
	(Join-Path $sourceDirectory 'GameInteractionGateway.cpp') `
	(Join-Path $sourceDirectory 'GameInteractionTransport.cpp') `
    (Join-Path $sourceDirectory 'LegacyGameInteractionProtocol.cpp') `
    /Fo:"$outputDirectory\" `
    /Fe:$executable /link ws2_32.lib
if ($LASTEXITCODE -ne 0) {
    throw "Legacy protocol harness compilation failed with exit code $LASTEXITCODE."
}

& $executable (Join-Path $testDirectory 'contract.jsonl')
if ($LASTEXITCODE -ne 0) {
    throw "Legacy protocol contract failed with exit code $LASTEXITCODE."
}
