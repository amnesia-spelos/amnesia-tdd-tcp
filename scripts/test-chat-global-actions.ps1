$ErrorActionPreference = 'Stop'

# While the Chat Composer owns input, typed keys must not fire the global ScreenShot or
# PrintInfo actions; they still reach the GUI. PrintInfo only fires while script debug is on.

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$gameRoot = Join-Path $repositoryRoot 'src\amnesia\src\game'
$inputSource = Get-Content -Raw (Join-Path $gameRoot 'LuxInputHandler.cpp')

function Get-FunctionBody([string]$source, [string]$signature) {
    $start = $source.IndexOf($signature)
    if ($start -lt 0) { throw "Could not find $signature." }
    $open = $source.IndexOf('{', $start)
    $depth = 0
    for ($i = $open; $i -lt $source.Length; $i++) {
        if ($source[$i] -eq '{') { $depth++ }
        elseif ($source[$i] -eq '}') {
            $depth--
            if ($depth -eq 0) { return $source.Substring($open, $i - $open + 1) }
        }
    }
    throw "Could not isolate $signature."
}

function Get-ActionCondition([string]$body, [string]$action) {
    $match = [regex]::Match($body, "if\s*\(([^{]*BecameTriggerd\($action\)[^{]*)\)\s*\{")
    if (-not $match.Success) { throw "Could not find the $action check." }
    return $match.Groups[1].Value
}

$globalInput = Get-FunctionBody $inputSource 'void cLuxInputHandler::UpdateGlobalInput()'
if ($globalInput -notmatch 'bool\s+bChatOwnsInput\s*=\s*gpBase->mpChatHandler\s*&&\s*gpBase->mpChatHandler->OwnsInput\(\)') {
    throw 'Global input does not ask the chat handler whether it owns input.'
}

foreach ($action in 'eLuxAction_ScreenShot', 'eLuxAction_PrintInfo') {
    $condition = Get-ActionCondition $globalInput $action
    if ($condition -notmatch '!\s*bChatOwnsInput\s*&&') {
        throw "$action fires while the Chat Composer owns input."
    }
}

$printInfo = Get-ActionCondition $globalInput 'eLuxAction_PrintInfo'
if ($printInfo -notmatch 'gpBase->mpDebugHandler->GetScriptDebugOn\(\)\s*&&[^&]*BecameTriggerd\(eLuxAction_PrintInfo\)') {
    throw 'PrintInfo dumps a render frame while script debug is off.'
}
$screenShot = Get-ActionCondition $globalInput 'eLuxAction_ScreenShot'
if ($screenShot -match 'GetScriptDebugOn') {
    throw 'ScreenShot is gated on script debug.'
}

$guiForwarding =$globalInput.IndexOf('CurrentStateSendsInputToGui()')
if ($guiForwarding -lt 0) {
    throw 'Global input no longer forwards input to the GUI.'
}
if ($globalInput.Substring($guiForwarding) -match 'bChatOwnsInput') {
    throw 'Forwarding input to the GUI is gated on chat ownership; typed keys would not reach the Chat Composer.'
}

Write-Host 'Global actions stay silent while the Chat Composer owns input.'
exit 0
