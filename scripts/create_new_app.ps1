param(
    [Parameter(Mandatory = $true)]
    [string]$NewApp,                              # name of the app folder to create (under apps/)
    [string]$Template = "esp32s3_demo",           # existing app folder to clone from (under apps/)
    [switch]$RenameProject,                       # also rewrite project(...) in CMakeLists.txt
    [switch]$CleanArtifacts                       # remove build/zephyr/etc if the template accidentally has them
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$AppsRoot    = (Resolve-Path (Join-Path $PSScriptRoot "..\apps")).Path
$Source      = Join-Path $AppsRoot $Template
$Destination = Join-Path $AppsRoot $NewApp

if (-not (Test-Path $Source)) {
    Write-Error "Template '$Template' not found under $AppsRoot"
    exit 1
}
if (Test-Path $Destination) {
    Write-Error "Destination '$NewApp' already exists under $AppsRoot"
    exit 1
}

# Copy everything from the template (excluding any nested .git dirs)
Copy-Item -Recurse -Force $Source $Destination
if (Test-Path (Join-Path $Destination ".git")) {
    Remove-Item -Recurse -Force (Join-Path $Destination ".git")
}

# Optionally clean common build artifacts that might exist in the template
if ($CleanArtifacts) {
    @(
        "build", "zephyr", "CMakeFiles", "CMakeCache.txt",
        "cmake_install.cmake", "compile_commands.json",
        "*.elf", "*.bin", "*.hex", "*.map", "*.log", "*.dts", "*.dtb", ".config"
    ) | ForEach-Object {
        Get-ChildItem -Path $Destination -Recurse -Force -ErrorAction SilentlyContinue -Filter $_ |
            Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Optionally rewrite the CMake project() line to match the new app name
if ($RenameProject) {
    $cmake = Join-Path $Destination "CMakeLists.txt"
    if (Test-Path $cmake) {
        $content = Get-Content $cmake -Raw
        # Replace an existing project(...) line; if none found, insert one after find_package(Zephyr)
        if ($content -match '(?ms)^\s*project\s*\([^)]+\)\s*$') {
            $content = $content -replace '(?ms)^\s*project\s*\([^)]+\)\s*$', "project($NewApp LANGUAGES C)"
        } else {
            $content = $content -replace '(?ms)(find_package\s*\(\s*Zephyr[^\)]*\)\s*\r?\n)', "`$1project($NewApp LANGUAGES C)`r`n"
        }
        Set-Content -Path $cmake -Value $content -NoNewline
    } else {
        Write-Warning "No CMakeLists.txt found in $Destination; skipping project rename."
    }
}

Write-Host "Created new app:" $Destination


# # Create from the default template (apps/esp32s3_demo)
# .\applications\scripts\create_new_app.ps1 -NewApp esp32s3_demo_advanced -RenameProject -CleanArtifacts

# # Create using a different template app you’ve made (apps/sensor_base)
# .\applications\scripts\create_new_app.ps1 -NewApp my_sensor_app -Template sensor_base -RenameProject -CleanArtifacts
