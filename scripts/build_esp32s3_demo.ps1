param(
    [string]$App      = 'esp32s3_demo',                       # new param: app name under apps/
    [string]$Board    = 'esp32s3_devkitc/esp32s3/procpu',
    [string]$Overlay  = 'boards/esp32s3_devkitc.overlay',
    [string]$BuildDir = '',                                   # auto-filled below
    [switch]$Clean,
    [switch]$Flash,
    [switch]$Monitor,
    [string]$Port                                          # e.g. 'COM5'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Optional: activate Zephyr venv
$venv = Join-Path $PSScriptRoot '..\.venv\Scripts\Activate.ps1'
if (Test-Path $venv) { . $venv }

# Build paths relative to repo root
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildDir) { $BuildDir = "build\$App" }
$Source = "apps\$App"

if ($Clean) {
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
}

# ---- Build ----
$buildArgs = @(
    'build','-p','always',
    '-b', $Board,
    '-d', $BuildDir,
    $Source,
    '--', "-DDTC_OVERLAY_FILE=$Overlay"
)
& west @buildArgs

# ---- Flash (optional) ----
if ($Flash) {
    $flashArgs = @('flash','-d',$BuildDir)
    if ($Port) { $flashArgs += @('--esp-device', $Port) }
    & west @flashArgs
}

# ---- Monitor (optional) ----
if ($Monitor) {
    $monArgs = @('espressif','monitor')
    if ($Port) { $monArgs += @('--esp-device', $Port) }
    & west @monArgs
}

<#
Examples:
  # Rebuild only
  .\build_app.ps1 -App esp32s3_demo -Clean

  # Rebuild and flash
  .\build_app.ps1 -App esp32s3_demo -Clean -Flash

  # Rebuild, flash, and open monitor on COM11
  .\build_app.ps1 -App esp32s3_demo -Clean -Flash -Monitor -Port COM11
#>
