param(
    [string]$App      = 'esp32s3_demo',
    [string]$Board    = 'esp32s3_devkitc/esp32s3/procpu',
    [string]$Overlay  = 'boards/esp32s3_devkitc.overlay',
    [string]$BuildDir = '',
    [switch]$Clean,
    [switch]$Flash,
    [switch]$Monitor,
    [string]$Port
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$venv = Join-Path $PSScriptRoot '..\.venv\Scripts\Activate.ps1'
if (Test-Path $venv) { . $venv }

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildDir) { $BuildDir = "build\$App" }
$Source = "apps\$App"

if ($Clean) {
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
}

# Build
& west build -p always -b $Board -d $BuildDir $Source -- "-DDTC_OVERLAY_FILE=$Overlay"

# Flash (disable runner's auto-monitor to avoid spurious error)
if ($Flash) {
    $flashArgs = @('flash','-d',$BuildDir,'--skip-monitor')
    if ($Port) { $flashArgs += @('--esp-device',$Port) }
    & west @flashArgs
}

# Monitor: run from inside the build dir so west finds the config
if ($Monitor) {
    if (-not (Test-Path $BuildDir)) {
        throw "Build directory '$BuildDir' not found."
    }

    Push-Location $BuildDir
    try {
        $monArgs = @('espressif','monitor')
        if ($Port) { $monArgs += @('--esp-device', $Port) }
        & west @monArgs
    }
    finally {
        Pop-Location
    }
}

<#
Examples (run from D:\ESP32Zephyr\zephyrproject\applications):

  # Rebuild only
  .\scripts\build_app.ps1 -App esp32s3_demo -Clean

  # Rebuild and flash
  .\scripts\build_app.ps1 -App esp32s3_demo -Clean -Flash

  # Rebuild, flash, and open monitor on COM11
  .\scripts\build_app.ps1 -App esp32s3_demo -Clean -Flash -Monitor -Port COM11

  # Build a different app
  .\scripts\build_app.ps1 -App esp32s3_demo_advanced -Clean -Flash -Monitor
#>
