$logFile = "$HOME\Documents\Amnesia\Main\hpl.log"
if (Test-Path $logFile) {
    $lines = Get-Content $logFile
    $printingMessage = $false

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]

        if ($line -match '^\[amnesia-tdd-tcp\]') {
            Write-Output $line

            if ($i + 1 -lt $lines.Count -and $lines[$i + 1] -match '^<message>') {
                $i++
                do {
                    Write-Output $lines[$i]
                    $i++
                } while ($i -lt $lines.Count -and $lines[$i] -notmatch '^</message>')
                
                # Also print the closing tag if found
                if ($i -lt $lines.Count) {
                    Write-Output $lines[$i]
                }
            }
        }
    }
} else {
    Write-Output "Log file not found: $logFile"
}
