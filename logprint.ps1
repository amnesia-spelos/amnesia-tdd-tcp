$logFile = "$HOME\Documents\Amnesia\Main\hpl.log"
if (Test-Path $logFile) {
    Get-Content $logFile | Where-Object { $_ -match '^\[amnesia-tdd-tcp\]' } | ForEach-Object { Write-Output $_ }
} else {
    Write-Output "Log file not found: $logFile"
}
