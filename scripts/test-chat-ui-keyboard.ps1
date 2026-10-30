$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceCheck.ps1')
$textBoxHeader = Get-Content -Raw (Join-Path $repositoryRoot 'src\HPL2\core\include\gui\WidgetTextBox.h')
$textBoxSource = Get-Content -Raw (Join-Path $repositoryRoot 'src\HPL2\core\sources\gui\WidgetTextBox.cpp')
$chatSource = Get-Content -Raw (Join-Path $repositoryRoot 'src\amnesia\src\game\LuxChatHandler.cpp')

if ($textBoxHeader -notmatch 'void SetUIKeyboardEnabled\(bool abX\)') {
    throw 'Text boxes cannot opt out of the on-screen keyboard.'
}
if ($textBoxSource -notmatch 'mbUIKeyboardEnabled = true;') {
    throw 'Text boxes do not open the on-screen keyboard by default.'
}

$pressBody = Get-FunctionBody $textBoxSource 'bool cWidgetTextBox::OnUIButtonPress(const cGuiMessageData& aData)'
$keyboardOpen = $pressBody.IndexOf('CreatePopUpUIKeyboard(this)')
$enabledGuard = $pressBody.IndexOf('mbUIKeyboardEnabled')
if ($keyboardOpen -lt 0 -or $enabledGuard -lt 0 -or $enabledGuard -gt $keyboardOpen) {
    throw 'The on-screen keyboard opens even when a text box has turned it off.'
}

$onStartBody = Get-FunctionBody $chatSource 'void cLuxChatHandler::OnStart()'
$initStart = $onStartBody.IndexOf('mpComposer = mpSet->CreateWidgetTextBox(')
if ($initStart -lt 0) {
    throw 'Could not find where Chat Composer is created.'
}
if ($onStartBody.Substring($initStart) -notmatch '^[^}]*mpComposer->SetUIKeyboardEnabled\(false\)') {
    throw 'Chat Composer can open the on-screen keyboard.'
}

Write-Host 'Chat Composer never opens the on-screen keyboard.'
exit 0
