[CmdletBinding()]
param(
    [string]$ScriptDirectory = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'

# Each script runs in its own process: they report failure by throwing, and some end with exit,
# either of which would stop this runner if called in-process.
$scripts = Get-ChildItem -LiteralPath $ScriptDirectory -Filter 'test-*.ps1' |
    Where-Object { $_.Name -ne 'test-all.ps1' } |
    Sort-Object Name

$failedNames = @()
foreach ($script in $scripts) {
    Write-Host "Running $($script.Name)"
    & pwsh -NoProfile -File $script.FullName
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $($script.Name) (exit code $LASTEXITCODE)"
        $failedNames += $script.Name
    }
}

Write-Host ''
Write-Host "$($scripts.Count - $failedNames.Count) of $($scripts.Count) test scripts passed."
if ($failedNames.Count -gt 0) {
    Write-Host 'Failed:'
    foreach ($name in $failedNames) {
        Write-Host "  $name"
    }
    exit 1
}
