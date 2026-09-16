$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $repositoryRoot 'src\amnesia\src\game\LuxChatHandler.cpp'
$source = Get-Content -Raw $sourcePath

$updateStart = $source.IndexOf('void cLuxChatHandler::Update(float afTimeStep)')
$canOpenStart = $source.IndexOf('bool cLuxChatHandler::CanOpenComposer() const')
if ($updateStart -lt 0 -or $canOpenStart -le $updateStart) {
    throw 'Could not isolate cLuxChatHandler::Update.'
}

$updateBody = $source.Substring($updateStart, $canOpenStart - $updateStart)
$conflictGuard = $updateBody.IndexOf('if(mModel.IsComposerOpen() && !CanOpenComposer()) CloseComposer(false);')
$focusInvariant = $updateBody.IndexOf('EnsureComposerFocus();')
if ($conflictGuard -lt 0 -or $focusInvariant -le $conflictGuard) {
    throw 'Chat focus is reclaimed without first yielding to conflicting input owners.'
}
if ($updateBody -notmatch 'if\s*\(mModel\.IsComposerOpen\(\)\)\s*\{[^}]*EnsureComposerFocus\(\)') {
    throw 'Open Chat Composer does not reassert focus during Update.'
}

$ensureStart = $source.IndexOf('void cLuxChatHandler::EnsureComposerFocus()')
$closeStart = $source.IndexOf('void cLuxChatHandler::CloseComposer(bool abSubmit)')
if ($ensureStart -lt 0 -or $closeStart -le $ensureStart) {
    throw 'Could not isolate cLuxChatHandler::EnsureComposerFocus.'
}
$ensureBody = $source.Substring($ensureStart, $closeStart - $ensureStart)
if ($ensureBody -notmatch 'GetFocusedSet\(\) != mpSet[^}]*SetFocus\(mpSet\)' -or
    $ensureBody -notmatch 'GetFocusedWidget\(\) != mpComposer[^}]*SetFocusedWidget\(mpComposer\)') {
    throw 'Chat focus invariant does not target both the Chat set and composer widget.'
}

$openStart = $source.IndexOf('void cLuxChatHandler::OpenComposer()')
if ($openStart -lt 0 -or $ensureStart -le $openStart) {
    throw 'Could not isolate cLuxChatHandler::OpenComposer.'
}
$openBody = $source.Substring($openStart, $ensureStart - $openStart)
if ($openBody -notmatch 'SetDrawMouse\(true\)') {
    throw 'Opening Chat Composer does not enable its mouse pointer.'
}
if ($ensureBody -notmatch 'SetSelectedText\([^;]+, 0\)') {
    throw 'Focusing Chat Composer does not initialize its caret for immediate keyboard input.'
}

$closeBody = $source.Substring($closeStart)
if ($closeBody -notmatch 'SetDrawMouse\(false\)') {
    throw 'Closing Chat Composer leaves its text-input mouse pointer visible.'
}

Write-Host 'Chat composer focus invariant is enforced.'
exit 0
