[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$runner = Join-Path $PSScriptRoot 'test-all.ps1'

function Invoke-Runner([hashtable]$fixtures) {
    $directory = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
    New-Item -ItemType Directory -Path $directory | Out-Null
    try {
        foreach ($name in $fixtures.Keys) {
            Set-Content -LiteralPath (Join-Path $directory $name) -Value $fixtures[$name]
        }
        $output = & pwsh -NoProfile -File $runner -ScriptDirectory $directory 2>&1 | Out-String
        [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = $output }
    }
    finally {
        Remove-Item -LiteralPath $directory -Recurse -Force
    }
}

$result = Invoke-Runner @{
    'test-passes.ps1' = "Write-Output 'fine'"
    'test-throws.ps1' = "throw 'broken'"
}
if ($result.ExitCode -eq 0) {
    throw "The runner exits 0 when a test script throws.`n$($result.Output)"
}
if ($result.Output -notmatch 'FAILED: test-throws\.ps1') {
    throw "The runner does not name the test script that threw.`n$($result.Output)"
}
if ($result.Output -match 'FAILED: test-passes\.ps1') {
    throw "The runner names a passing test script as failed.`n$($result.Output)"
}

if ($result.Output -notmatch '1 of 2 test scripts passed\.\s+Failed:\s+test-throws\.ps1') {
    throw "The runner does not end with a summary listing the failed test scripts.`n$($result.Output)"
}

$result = Invoke-Runner @{
    'test-a-exits.ps1' = 'exit 0'
    'test-b-throws.ps1' = "throw 'broken'"
}
if ($result.ExitCode -eq 0) {
    throw "The runner exits 0 when a test script after one that exits throws.`n$($result.Output)"
}
if ($result.Output -notmatch 'FAILED: test-b-throws\.ps1') {
    throw "A test script that exits stops the runner before later scripts.`n$($result.Output)"
}

$result = Invoke-Runner @{
    'test-all.ps1' = 'exit 1'
    'test-passes.ps1' = "Write-Output 'fine'"
}
if ($result.Output -match 'Running test-all\.ps1') {
    throw "The runner runs test-all.ps1 itself.`n$($result.Output)"
}
if ($result.ExitCode -ne 0) {
    throw "The runner fails when every test script passes.`n$($result.Output)"
}
