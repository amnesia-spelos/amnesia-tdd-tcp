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

$hpl2Library = Join-Path $repositoryRoot 'src\HPL2\lib\HPL2_2010.lib'
if (-not (Test-Path -LiteralPath $hpl2Library)) {
    Invoke-MSBuildProject -MSBuild $msbuild -ProjectPath (Join-Path $repositoryRoot 'src\HPL2\core\_HPL2_2010.vcxproj') -Description 'HPL2 (Release|Win32)' -CommonArguments $commonArguments
}

Invoke-MSBuildProject -MSBuild $msbuild -ProjectPath (Join-Path $repositoryRoot 'src\HPL2\tools\editors\leveleditor\leveleditor.vcxproj') -Description 'Level Editor (Release|Win32)' -CommonArguments $commonArguments

Write-Host "Build complete: $(Join-Path $repositoryRoot 'artifacts\Release\LevelEditor.exe')"
