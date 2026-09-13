# publish.ps1 — Build Monix, create GitHub release, upload exe
# Usage: .\publish.ps1 -Version "v1.0.0" [-Draft]
param(
  [Parameter(Mandatory=$true)]
  [string]$Version,
  [switch]$Draft,
  [switch]$Prerelease
)

$ErrorActionPreference = "Stop"
$ghExe = "C:\Program Files\GitHub CLI\gh.exe"
$repo = "belfegor442/Monix.release"
$exePath = "build\Monix.exe"

Write-Host "=== Monix Release Publisher ===" -ForegroundColor Cyan
Write-Host "Version: $Version"
Write-Host "Repo:    $repo"

# Check gh auth
Write-Host "`nChecking GitHub auth..."
& $ghExe auth status 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
  Write-Host "ERROR: Not logged into GitHub. Run: & '$ghExe' auth login" -ForegroundColor Red
  exit 1
}

# Build
Write-Host "`nBuilding Monix..."
$invocationDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $invocationDir
try {
  & powershell -ExecutionPolicy Bypass -File "build.ps1"
  if ($LASTEXITCODE -ne 0) { throw "Build failed" }
} finally {
  Pop-Location
}

# Verify exe exists
if (-not (Test-Path $exePath)) {
  Write-Host "ERROR: $exePath not found after build" -ForegroundColor Red
  exit 1
}

$exeSize = (Get-Item $exePath).Length / 1MB
Write-Host "Built Monix.exe ($([math]::Round($exeSize, 1)) MB)"

# Create release notes
$releaseNotes = @"
## Monix $Version

### System Monitor & Diagnostics
- Full Win98-themed dashboard with 5 tabs (USAGE/AI/TASKS/LOG/SETTINGS)
- Real-time CPU, GPU, RAM, disk, and network monitoring
- SCRAM risk engine with 32 rule types
- CRT shader post-processing toggle (Ctrl+T)
- Auto-update system via GitHub Releases

### Changes
- Initial public release with auto-update support
- Win98 UI theme with authentic look and feel
"@

$notesFile = Join-Path $env:TEMP "monix_release_notes.md"
Set-Content -Path $notesFile -Value $releaseNotes -Encoding UTF8

# Build gh release command
$releaseArgs = @("release", "create", $Version, $exePath, "--repo", $repo, "--title", "Monix $Version", "--notes-file", $notesFile, "--target", "main")

if ($Draft) { $releaseArgs += "--draft" }
if ($Prerelease) { $releaseArgs += "--prerelease" }

# Rename exe to include version
$releaseExe = "Monix-$Version.exe"
Copy-Item $exePath "build\$releaseExe" -Force
$releaseArgs[-1] = "build\$releaseExe"  # Replace the exe path with versioned name

Write-Host "`nCreating GitHub release $Version..."
Write-Host "Command: $ghExe $($releaseArgs -join ' ')"
& $ghExe @releaseArgs

if ($LASTEXITCODE -eq 0) {
  Write-Host "`nRelease $Version published successfully!" -ForegroundColor Green
  $releaseUrl = "https://github.com/$repo/releases/tag/$Version"
  Write-Host "URL: $releaseUrl"
  
  # Get the exe download URL
  $assetsJson = & $ghExe api "repos/$repo/releases/tags/$Version" --jq '.assets[0].browser_download_url' 2>&1
  if ($LASTEXITCODE -eq 0) {
    Write-Host "Download: $assetsJson"
  }
} else {
  Write-Host "ERROR: Failed to create release" -ForegroundColor Red
  exit 1
}

# Cleanup
Remove-Item $notesFile -ErrorAction SilentlyContinue
