$ErrorActionPreference = 'Stop'

# The Enter or Escape that closes the Chat Composer must not reach any other GUI set
# or input state until that keystroke is released.

$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceCheck.ps1')
$gameRoot = Join-Path $repositoryRoot 'src\amnesia\src\game'
$chatSource = Get-Content -Raw (Join-Path $gameRoot 'LuxChatHandler.cpp')
$chatHeader = Get-Content -Raw (Join-Path $gameRoot 'LuxChatHandler.h')
$inputSource = Get-Content -Raw (Join-Path $gameRoot 'LuxInputHandler.cpp')

if ($chatHeader -notmatch 'bool OwnsInput\(\) const') {
    throw 'Chat handler does not expose whether it still owns input after the Chat Composer closes.'
}

$guiRouting = Get-FunctionBody $inputSource 'bool cLuxInputHandler::CurrentStateSendsInputToGui()'
if ($guiRouting -notmatch 'mpChatHandler->OwnsInput\(\)') {
    throw 'GUI input is not kept on the Chat set while the closing keystroke is held.'
}

$close = Get-FunctionBody $chatSource 'bool cLuxChatHandler::CloseComposer(bool abSubmit, bool abRestoreFocus)'
if ($close -match 'SetFocus\(mpPreviousFocus\)' -or
    $close -notmatch 'if\s*\(\s*abRestoreFocus\s*\)\s*RestorePreviousFocus\(\)') {
    throw 'Closing the Chat Composer cannot defer restoring the previous focus.'
}

$byKeystroke = Get-FunctionBody $chatSource 'void cLuxChatHandler::CloseComposerByKeystroke(bool abSubmit)'
if ($byKeystroke -notmatch 'mbAwaitingKeystrokeRelease\s*=\s*CloseComposer\(abSubmit,\s*false\)') {
    throw 'Closing by keystroke restores focus immediately, or awaits release after a rejected submission.'
}

$restore = Get-FunctionBody $chatSource 'void cLuxChatHandler::RestorePreviousFocus()'
if ($restore -notmatch 'GetFocusedSet\(\) == mpSet[^;]*SetFocus\(mpPreviousFocus\)') {
    throw 'Restoring focus after the closing keystroke overrides a set that took focus meanwhile.'
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
