$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$clang = Get-Command clang -ErrorAction SilentlyContinue
if (-not $clang) { throw 'clang introuvable. Ouvrir Developer PowerShell, charger Launch-VsDevShell.ps1 et ajouter C:\Program Files\LLVM\bin au PATH.' }
$out = Join-Path $env:TEMP 's6sy771-board-bridge-tests.exe'
$src = @(
 (Join-Path $root 'touch\S6SY771\s6sy771_protocol.c'),
 (Join-Path $root 'touch\S6SY771\s6sy771_frame.c'),
 (Join-Path $root 'touch\S6SY771\s6sy771_transport.c'),
 (Join-Path $root 'touch\S6SY771\s6sy771_xnu_core.c'),
 (Join-Path $root 'touch\S6SY771\s6sy771_board_bridge.c'),
 (Join-Path $PSScriptRoot 'test_board_bridge.c')
)
& clang -std=c11 -Wall -Wextra -Werror $src -o $out
if ($LASTEXITCODE -ne 0) { throw 'Compilation du bridge échouée.' }
& $out
if ($LASTEXITCODE -ne 0) { throw 'Tests du bridge échoués.' }
