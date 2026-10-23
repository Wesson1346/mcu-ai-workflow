# install.ps1 - Install mcu-project-orchestrator skill (Windows)
#
# Usage:
#   .\install.ps1           install (skip if already installed)
#   .\install.ps1 -Force    overwrite existing installation
#
# What it does:
#   1. Copies orchestrator/ into  %USERPROFILE%\.workbuddy\skills\mcu-project-orchestrator
#   2. Replaces {{AI_SKILL_ROOT}} with the real absolute path of this repo's AI-Skill folder

param([switch]$Force)

$ErrorActionPreference = "Stop"

$repoRoot  = Split-Path -Parent $MyInvocation.MyCommand.Path
$skillRoot = Join-Path $repoRoot "AI-Skill"
$source    = Join-Path $repoRoot "orchestrator"
$target    = Join-Path $env:USERPROFILE ".workbuddy\skills\mcu-project-orchestrator"

if (-not (Test-Path $skillRoot)) {
    Write-Host "ERROR: AI-Skill folder not found at: $skillRoot" -ForegroundColor Red
    Write-Host "Run this script from the repository root."
    exit 1
}
if (-not (Test-Path $source)) {
    Write-Host "ERROR: orchestrator folder not found at: $source" -ForegroundColor Red
    exit 1
}

if (Test-Path $target) {
    if (-not $Force) {
        Write-Host "Already installed at: $target" -ForegroundColor Yellow
        Write-Host "Re-run with -Force to overwrite."
        exit 0
    }
    Write-Host "Removing existing installation..."
    Remove-Item $target -Recurse -Force
}

New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item -Path (Join-Path $source "*") -Destination $target -Recurse -Force

$skillRootFwd = $skillRoot.Replace("\", "/")
$utf8NoBom    = New-Object System.Text.UTF8Encoding($false)
$patched      = 0

Get-ChildItem -Path $target -Recurse -Filter *.md | ForEach-Object {
    $text = [System.IO.File]::ReadAllText($_.FullName, [System.Text.Encoding]::UTF8)
    if ($text.Contains("{{AI_SKILL_ROOT}}")) {
        $text = $text.Replace("{{AI_SKILL_ROOT}}", $skillRootFwd)
        [System.IO.File]::WriteAllText($_.FullName, $text, $utf8NoBom)
        $patched++
    }
}

Write-Host ""
Write-Host "Installed : $target"    -ForegroundColor Green
Write-Host "Skill root: $skillRootFwd" -ForegroundColor Green
Write-Host "Patched   : $patched file(s)"
Write-Host ""
Write-Host "Next step:"
Write-Host "  Open any project folder in your AI IDE and say:"
Write-Host "    I want to build a temperature-controlled fan"
Write-Host ""
Write-Host "  The orchestrator will first show a plan card and WAIT for your confirmation."
