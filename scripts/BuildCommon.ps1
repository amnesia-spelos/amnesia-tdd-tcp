function Get-MSBuildPath {
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

    return $msbuild
}

function Initialize-HPL2Dependencies {
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $dependencyArchive = Join-Path $RepositoryRoot 'src\HPL2\dependencies.zip'
    $dependencyDirectory = Join-Path $RepositoryRoot 'src\HPL2\dependencies'
    $dependencyHeader = Join-Path $dependencyDirectory 'include\SDL2\SDL.h'
    $dependencyLibrary = Join-Path $dependencyDirectory 'lib\win32\SDL2.lib'

    if (-not (Test-Path -LiteralPath $dependencyHeader) -or -not (Test-Path -LiteralPath $dependencyLibrary)) {
        Write-Host 'Extracting bundled HPL2 dependencies...'
        Expand-Archive -LiteralPath $dependencyArchive -DestinationPath (Split-Path -Parent $dependencyDirectory) -Force
    }
}

function Invoke-MSBuildProject {
    param(
        [Parameter(Mandatory)]
        [string]$MSBuild,
        [Parameter(Mandatory)]
        [string]$ProjectPath,
        [Parameter(Mandatory)]
        [string]$Description,
        [Parameter(Mandatory)]
        [string[]]$CommonArguments
    )

    Write-Host "Building $Description..."
    & $MSBuild $ProjectPath @CommonArguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description build failed with exit code $LASTEXITCODE."
    }
}
