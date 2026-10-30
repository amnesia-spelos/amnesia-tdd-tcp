$ErrorActionPreference = 'Stop'

# While the Chat Composer owns input, typed keys must not fire the global ScreenShot or
# PrintInfo actions; they still reach the GUI. PrintInfo only fires while script debug is on.

$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceCheck.ps1')
$gameRoot = Join-Path $repositoryRoot 'src\amnesia\src\game'
$inputSource = Get-Content -Raw (Join-Path $gameRoot 'LuxInputHandler.cpp')

function Get-ActionCondition([string]$body, [string]$action) {
    $match = [regex]::Match($body, "if\s*\(([^{;]*BecameTriggerd\($action\)[^{;]*)\)\s*\{")
    if (-not $match.Success) { throw "Could not find the $action check." }
    return $match.Groups[1].Value
}

$globalInput = Get-FunctionBody $inputSource 'void cLuxInputHandler::UpdateGlobalInput()'
if ($globalInput -notmatch 'bool\s+bChatOwnsInput\s*=\s*gpBase->mpChatHandler\s*&&\s*gpBase->mpChatHandler->OwnsInput\(\)') {
    throw 'Global input does not ask the chat handler whether it owns input.'
}

# BecameTriggerd latches the press until release, so it must run every frame. Gating the call
# itself would leave a press held across the end of chat ownership to fire on the next frame.
foreach ($action in 'eLuxAction_ScreenShot', 'eLuxAction_PrintInfo') {
    $condition = Get-ActionCondition $globalInput $action
    if ($condition -notmatch "^\s*mpInput->BecameTriggerd\($action\)\s*&&") {
        throw "$action is not latched while its gates are closed; a held key fires once they open."
    }
    if ($condition -match '\|\|' -or $condition -notmatch '&&\s*!\s*bChatOwnsInput\b') {
        throw "$action fires while the Chat Composer owns input."
    }
}

$printInfo = Get-ActionCondition $globalInput 'eLuxAction_PrintInfo'
if ($printInfo -notmatch '&&\s*gpBase->mpDebugHandler->GetScriptDebugOn\(\)') {
    throw 'PrintInfo dumps a render frame while script debug is off.'
}
$screenShot = Get-ActionCondition $globalInput 'eLuxAction_ScreenShot'
if ($screenShot -match 'GetScriptDebugOn') {
    throw 'ScreenShot is gated on script debug.'
}

# Typed keys reach the Chat Composer through the GUI forwarding, so chat ownership may only
# gate the two actions: once declared, once per action condition.
if ($globalInput -notmatch 'if\s*\(\s*CurrentStateSendsInputToGui\(\)\s*\)') {
    throw 'Global input no longer forwards input to the GUI unconditionally of chat ownership.'
}
if ([regex]::Matches($globalInput, '\bbChatOwnsInput\b').Count -ne 3) {
    throw 'Chat ownership gates more than the global actions; typed keys may not reach the Chat Composer.'
}

Write-Host 'Global actions stay silent while the Chat Composer owns input.'
exit 0
