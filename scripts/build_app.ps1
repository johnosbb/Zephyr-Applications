param(
    [string]$App      = 'esp32s3_demo',
    [string]$Board    = 'esp32s3_devkitc/esp32s3/procpu',
    [string]$Overlay  = 'boards/esp32s3_devkitc.overlay',
    [string]$BuildDir = '',
    [switch]$Clean,
    [switch]$Flash,
    [switch]$Monitor,
    [switch]$Debug,
    [string]$Port,
    # Default to your current GDB path; override with -GdbPath if needed
    [string]$GdbPath  = 'C:/Users/johno/zephyr-sdk-0.17.4/xtensa-espressif_esp32s3_zephyr-elf/bin/xtensa-espressif_esp32s3_zephyr-elf-gdb.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Activate Python virtual environment if present
$venv = Join-Path $PSScriptRoot '..\.venv\Scripts\Activate.ps1'
if (Test-Path $venv) { . $venv }

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildDir) { $BuildDir = "build\$App" }
$Source = "apps\$App"

# Optional clean
if ($Clean) {
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
}

# Build
& west build -p always -b $Board -d $BuildDir $Source -- "-DDTC_OVERLAY_FILE=$Overlay"

# Flash (disable runner's auto-monitor to avoid spurious error)
if ($Flash) {
    $flashArgs = @('flash','-d',$BuildDir)
    if ($Port) { $flashArgs += @('--esp-device',$Port) }
    & west @flashArgs
}

# Debug: launch GDB on the built ELF
if ($Debug) {
    if (-not (Test-Path $BuildDir)) {
        throw "Build directory '$BuildDir' not found."
    }

    $elfPath = Join-Path $BuildDir 'zephyr\zephyr.elf'
    if (-not (Test-Path $elfPath)) {
        throw "ELF file '$elfPath' not found (expected at '$elfPath')."
    }

    if (-not (Test-Path $GdbPath)) {
        throw "GDB executable not found at '$GdbPath'. Use -GdbPath to override."
    }

    # Launch GDB with the ELF. You can add -ex commands here if you want
    # to auto-connect to OpenOCD, e.g.:
    # & $GdbPath '-ex' 'target extended-remote localhost:3333' $elfPath
    & $GdbPath $elfPath
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

  # Rebuild and then launch GDB for debugging
  .\scripts\build_app.ps1 -App esp32s3_demo -Clean -Debug

  # Rebuild, flash, and debug with a custom GDB path
  .\scripts\build_app.ps1 -App esp32s3_demo -Clean -Flash -Debug `
      -GdbPath 'D:/tools/zephyr-sdk/xtensa-espressif_esp32s3_zephyr-elf-gdb.exe'
#>
