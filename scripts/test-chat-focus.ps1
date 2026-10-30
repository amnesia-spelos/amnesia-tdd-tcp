$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceCheck.ps1')
$sourcePath = Join-Path $repositoryRoot 'src\amnesia\src\game\LuxChatHandler.cpp'
$source = Get-Content -Raw $sourcePath

$updateBody = Get-FunctionBody $source 'void cLuxChatHandler::Update(float afTimeStep)'
$conflictGuard = $updateBody.IndexOf('if(mModel.IsComposerOpen() && !CanOpenComposer()) CloseComposer(false);')
$focusInvariant = $updateBody.IndexOf('EnsureComposerFocus();')
if ($conflictGuard -lt 0 -or $focusInvariant -le $conflictGuard) {
    throw 'Chat focus is reclaimed without first yielding to conflicting input owners.'
}
if ($updateBody -notmatch 'if\s*\(mModel\.IsComposerOpen\(\)\)\s*\{[^}]*EnsureComposerFocus\(\)') {
    throw 'Open Chat Composer does not reassert focus during Update.'
}

$ensureBody = Get-FunctionBody $source 'void cLuxChatHandler::EnsureComposerFocus()'
if ($ensureBody -notmatch 'GetFocusedSet\(\) != mpSet[^}]*SetFocus\(mpSet\)' -or
    $ensureBody -notmatch 'GetFocusedWidget\(\) != mpComposer[^}]*SetFocusedWidget\(mpComposer\)') {
    throw 'Chat focus invariant does not target both the Chat set and composer widget.'
}

$openBody = Get-FunctionBody $source 'void cLuxChatHandler::OpenComposer()'
if ($openBody -notmatch 'SetDrawMouse\(true\)') {
    throw 'Opening Chat Composer does not enable its mouse pointer.'
}
if ($ensureBody -notmatch 'SetSelectedText\([^;]+, 0\)') {
    throw 'Focusing Chat Composer does not initialize its caret for immediate keyboard input.'
}

$closeBody = Get-FunctionBody $source 'bool cLuxChatHandler::CloseComposer(bool abSubmit, bool abRestoreFocus)'
if ($closeBody -notmatch 'SetDrawMouse\(false\)') {
    throw 'Closing Chat Composer leaves its text-input mouse pointer visible.'
}

Write-Host 'Chat composer focus invariant is enforced.'
exit 0
