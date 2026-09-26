[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'BuildCommon.ps1')

Initialize-HPL2Dependencies -RepositoryRoot $repositoryRoot
$msbuild = Get-MSBuildPath

$commonArguments = @(
    '/m',
    '/nologo',
    '/p:Configuration=Release',
    '/p:Platform=Win32'
)

Invoke-MSBuildProject -MSBuild $msbuild -ProjectPath (Join-Path $repositoryRoot 'src\HPL2\core\_HPL2_2010.vcxproj') -Description 'HPL2 (Release|Win32)' -CommonArguments $commonArguments
Invoke-MSBuildProject -MSBuild $msbuild -ProjectPath (Join-Path $repositoryRoot 'src\amnesia\src\game\Lux.vcxproj') -Description 'Amnesia (Release|Win32)' -CommonArguments $commonArguments

Write-Host "Build complete: $(Join-Path $repositoryRoot 'artifacts\Release\Amnesia.exe')"
