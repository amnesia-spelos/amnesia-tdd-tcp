$ErrorActionPreference = 'Stop'

# The Enter or Escape that closes the Chat Composer must not reach any other GUI set
# or input state until that keystroke is released.

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$gameRoot = Join-Path $repositoryRoot 'src\amnesia\src\game'
$chatSource = Get-Content -Raw (Join-Path $gameRoot 'LuxChatHandler.cpp')
$chatHeader = Get-Content -Raw (Join-Path $gameRoot 'LuxChatHandler.h')
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

if ($chatHeader -notmatch 'bool OwnsInput\(\) const') {
    throw 'Chat handler does not expose whether it still owns input after the Chat Composer closes.'
}

$guiRouting = Get-FunctionBody $inputSource 'bool cLuxInputHandler::CurrentStateSendsInputToGui()'
if ($guiRouting -notmatch 'mpChatHandler->OwnsInput\(\)') {
    throw 'GUI input is not kept on the Chat set while the closing keystroke is held.'
}

$close = Get-FunctionBody $chatSource 'void cLuxChatHandler::CloseComposer(bool abSubmit)'
if ($close -match 'SetFocus\(mpPreviousFocus\)' -or
    $close -notmatch 'if\s*\(\s*!mbAwaitingKeystrokeRelease\s*\)\s*RestorePreviousFocus\(\)') {
    throw 'Closing the Chat Composer restores the previous focus before its keystroke is released.'
}

$entered = Get-FunctionBody $chatSource 'bool cLuxChatHandler::ComposerEntered(iWidget* apWidget, const cGuiMessageData& aData)'
if ($entered -notmatch 'CloseComposerByKeystroke\(true\)') {
    throw 'Submitting with Enter does not hold input until Enter is released.'
}

$handle = Get-FunctionBody $chatSource 'bool cLuxChatHandler::HandleInput(cInput* apInput)'
if ($handle -notmatch 'BecameTriggerd\(eLuxAction_Exit\)\)\s*CloseComposerByKeystroke\(false\)') {
    throw 'Closing with Escape does not hold input until Escape is released.'
}
if ($handle -notmatch 'if\s*\(\s*mbAwaitingKeystrokeRelease\s*\)\s*\{[^}]*IsComposerKeystrokeHeld\(apInput\)\)\s*return true;[^}]*RestorePreviousFocus\(\);[^}]*return true;') {
    throw 'Chat input ownership does not end with a focus restore once the closing keystroke is released.'
}
if ($handle.IndexOf('mbAwaitingKeystrokeRelease') -gt $handle.IndexOf('IsComposerOpen()')) {
    throw 'The pending closing keystroke is not handled before the open Chat Composer.'
}

$held = Get-FunctionBody $chatSource 'bool cLuxChatHandler::IsComposerKeystrokeHeld(cInput* apInput) const'
foreach ($action in 'eLuxAction_UIPrimary', 'eLuxAction_UISecondary', 'eLuxAction_Exit') {
    if ($held -notmatch "IsTriggerd\($action\)") {
        throw "Closing keystroke is considered released while $action is still held."
    }
}

if ($chatSource -match 'mbConsumedComposerInput' -or $chatHeader -match 'mbConsumedComposerInput') {
    throw 'The one-frame composer input swallow is still present alongside keystroke ownership.'
}

Write-Host 'Chat composer closing keystroke stays with the Chat set until released.'
exit 0
