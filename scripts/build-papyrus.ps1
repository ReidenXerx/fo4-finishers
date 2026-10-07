<#
.SYNOPSIS
  Compiles papyrus\ into build\pack\Scripts (the release pack) against the reconstructed base game sources.
  ASCII only (Windows PowerShell 5.1 reads BOM-less UTF-8 as ANSI).
#>
[CmdletBinding()]
param(
    [string] $Base     = 'D:\F4CustomMods\PapyrusBase\Source\Base',
    [string] $Compiler = 'D:\GOGGames\Fallout 4 GOTY\Papyrus Compiler\PapyrusCompiler.exe',
    # F4SE's script sources; searched before the base sources.
    [string] $F4SE     = 'D:\GOGGames\Fallout 4 GOTY\Data\Scripts\Source'
)
$ErrorActionPreference = 'Stop'
$root    = Split-Path -Parent $PSScriptRoot
$sources = Join-Path $root 'papyrus'
$out     = Join-Path $root 'build\pack\Scripts'
if (-not (Test-Path $Compiler)) { throw "No Papyrus compiler at $Compiler." }
if (-not (Test-Path (Join-Path $Base 'Institute_Papyrus_Flags.flg'))) { throw "No Institute_Papyrus_Flags.flg in $Base." }
New-Item -ItemType Directory -Force $out | Out-Null
$imports = @($F4SE, $Base, $sources) -join ';'
& $Compiler $sources -import="$imports" -output="$out" -flags='Institute_Papyrus_Flags.flg' -all -optimize
if ($LASTEXITCODE -ne 0) { throw "Papyrus compile failed with exit code $LASTEXITCODE." }
$pex = Get-ChildItem -Path $out -Recurse -Filter *.pex
Write-Host ("{0} .pex in {1}" -f $pex.Count, $out)
