param(
    [switch]$Build,
    [switch]$Drive
)

$ErrorActionPreference = "Stop"

if (-not ([System.Management.Automation.PSTypeName]'System.Windows.Forms.Screen').Type) {
    Add-Type -AssemblyName System.Windows.Forms
}

if (-not ([System.Management.Automation.PSTypeName]'Smoke.NativeMethods').Type) {
Add-Type @"
using System;
using System.Runtime.InteropServices;
namespace Smoke {
    public static class NativeMethods {
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    }
}
"@
}

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function Assert-FileExists {
    param([string]$Path)
    Assert-True (Test-Path $Path) "Missing file: $Path"
}

function Read-JsonFile {
    param([string]$Path)
    Assert-FileExists $Path
    $raw = Get-Content $Path -Raw
    Assert-True (-not [string]::IsNullOrWhiteSpace($raw)) "Empty JSON file: $Path"
    return ($raw | ConvertFrom-Json)
}

function Read-KeyValueFile {
    param([string]$Path)
    Assert-FileExists $Path
    $map = @{}
    foreach ($line in Get-Content $Path) {
        if ($line -match '^\s*([^=]+)=(.*)$') {
            $map[$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }
    return $map
}

function Invoke-WithRetry {
    param(
        [scriptblock]$Action,
        [string]$Description,
        [int]$Attempts = 20,
        [int]$DelayMs = 250
    )

    $lastError = $null
    for ($attempt = 1; $attempt -le $Attempts; ++$attempt) {
        try {
            & $Action
            return
        }
        catch {
            $lastError = $_
            if ($attempt -eq $Attempts) { break }
            Start-Sleep -Milliseconds $DelayMs
        }
    }

    throw "Failed to $Description after $Attempts attempts. Last error: $lastError"
}

function Write-FileWithRetry {
    param(
        [string]$Path,
        [string]$Value
    )

    Invoke-WithRetry -Description "write file '$Path'" -Action {
        [System.IO.File]::WriteAllText($Path, $Value, [System.Text.Encoding]::UTF8)
    }
}

function Copy-FileWithRetry {
    param(
        [string]$Source,
        [string]$Destination
    )

    Invoke-WithRetry -Description "copy '$Source' to '$Destination'" -Action {
        [System.IO.File]::Copy($Source, $Destination, $true)
    }
}

function Get-FloatMetric {
    param(
        [hashtable]$Metrics,
        [string]$Name
    )
    Assert-True ($Metrics.ContainsKey($Name)) "Missing metric '$Name'"
    return [double]::Parse($Metrics[$Name], [System.Globalization.CultureInfo]::InvariantCulture)
}

function Get-IntMetric {
    param(
        [hashtable]$Metrics,
        [string]$Name
    )
    Assert-True ($Metrics.ContainsKey($Name)) "Missing metric '$Name'"
    return [int]$Metrics[$Name]
}

function Get-SmokeTargetScreen {
    $screens = [System.Windows.Forms.Screen]::AllScreens |
        Sort-Object @{ Expression = { if ($_.Primary) { 0 } else { 1 } } }, @{ Expression = { $_.Bounds.X } }, @{ Expression = { $_.Bounds.Y } }

    $secondary = $screens | Where-Object { -not $_.Primary } | Sort-Object @{ Expression = { $_.Bounds.X } }, @{ Expression = { $_.Bounds.Y } } | Select-Object -First 1
    if ($null -ne $secondary) {
        return $secondary
    }

    return $screens | Select-Object -First 1
}

function Invoke-CarScenario {
    param(
        [string]$Scenario,
        [string]$Root,
        [string]$GameExe,
        [string]$GamePath,
        [string]$ScenePath,
        [switch]$UseRealPlayStartup,
        [switch]$CaptureVideo
    )

    $enablePath   = Join-Path $Root "bin\\CarDriveAutotest.enable"
    $startedPath  = Join-Path $Root "bin\\CarDriveAutotest.started"
    $logPath      = Join-Path $Root "bin\\CarDriveAutotest.log"
    $statusPath   = Join-Path $Root "bin\\CarDriveAutotest.status"
    $debugTextPath = Join-Path $Root "bin\\CarDriveAutotest.debug.txt"
    $debugCsvPath  = Join-Path $Root "bin\\CarDriveAutotest.debug.csv"
    $touchdownTextPath = Join-Path $Root "bin\\CarDriveAutotest.touchdown.txt"
    $touchdownCsvPath  = Join-Path $Root "bin\\CarDriveAutotest.touchdown.csv"
    $gameBackupPath = Join-Path $Root "bin\\Game.autotest.backup.json"
    $videoPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.mkv" -f $Scenario)
    $videoLogPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.capture.log" -f $Scenario)
    $videoOutLogPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.capture.out.log" -f $Scenario)
    $videoProc = $null

    Copy-FileWithRetry -Source $GamePath -Destination $gameBackupPath
    if ($UseRealPlayStartup) {
        Copy-FileWithRetry -Source $ScenePath -Destination $GamePath
    } else {
        Write-FileWithRetry -Path $GamePath -Value "[]"
    }
    Set-Content -Path $enablePath -Value "1" -NoNewline
    foreach ($path in @($startedPath, $logPath, $statusPath, $debugTextPath, $debugCsvPath, $touchdownTextPath, $touchdownCsvPath)) {
        if (Test-Path $path) { Remove-Item $path -Force }
    }
    if (Test-Path $videoLogPath) { Remove-Item $videoLogPath -Force }
    if (Test-Path $videoOutLogPath) { Remove-Item $videoOutLogPath -Force }

    $env:TE_AUTOTEST_CAR = "1"
    $env:TE_AUTOTEST_MODE = $Scenario
    $env:TE_AUTOTEST_SHOW_COLLISION = "1"
    $env:TE_AUTOTEST_NOACTIVATE = "1"
    if ($UseRealPlayStartup) {
        $env:TE_AUTOTEST_PRESERVE_SCENE = "1"
    }

    $targetScreen = Get-SmokeTargetScreen
    $work = $targetScreen.WorkingArea
    $windowWidth = [Math]::Min(1600, [Math]::Max(960, $work.Width - 120))
    $windowHeight = [Math]::Min(900, [Math]::Max(540, $work.Height - 120))
    $windowX = $work.X + [Math]::Max(40, [Math]::Floor(($work.Width - $windowWidth) / 2))
    $windowY = $work.Y + [Math]::Max(40, [Math]::Floor(($work.Height - $windowHeight) / 2))
    $env:TE_WINDOW_X = [string]$windowX
    $env:TE_WINDOW_Y = [string]$windowY
    $env:TE_WINDOW_W = [string]$windowWidth
    $env:TE_WINDOW_H = [string]$windowHeight
    Write-Host ("[Smoke] Scenario '{0}' target screen {1}: {2},{3} {4}x{5}" -f $Scenario, $targetScreen.DeviceName, $windowX, $windowY, $windowWidth, $windowHeight)

    $proc = Start-Process -FilePath $GameExe -WorkingDirectory (Join-Path $Root "bin") -PassThru
    if ($CaptureVideo) {
        if (Test-Path $videoPath) { Remove-Item $videoPath -Force }
        $ffmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
        Assert-True (-not [string]::IsNullOrWhiteSpace($ffmpeg)) "ffmpeg is required for video capture."

        $windowHandle = 0
        $windowDeadline = (Get-Date).AddSeconds(8)
        while ((Get-Date) -lt $windowDeadline) {
            try { $proc.Refresh() } catch {}
            if ($proc.HasExited) { break }
            if ($proc.MainWindowHandle -ne 0) {
                $windowHandle = $proc.MainWindowHandle
                break
            }
            Start-Sleep -Milliseconds 100
        }
        Assert-True ($windowHandle -ne 0) "Unable to find GameLauncher window handle for video capture."

        $rect = New-Object Smoke.NativeMethods+RECT
        $gotRect = [Smoke.NativeMethods]::GetWindowRect([IntPtr]$windowHandle, [ref]$rect)
        Assert-True $gotRect "Unable to query GameLauncher window bounds for video capture."
        $captureWidth = $rect.Right - $rect.Left
        $captureHeight = $rect.Bottom - $rect.Top
        Assert-True ($captureWidth -gt 0 -and $captureHeight -gt 0) "GameLauncher window bounds are invalid for video capture."

        $ffmpegArgs = @(
            "-y",
            "-f", "gdigrab",
            "-framerate", "30",
            "-draw_mouse", "0",
            "-offset_x", $rect.Left,
            "-offset_y", $rect.Top,
            "-video_size", ("{0}x{1}" -f $captureWidth, $captureHeight),
            "-i", "desktop",
            "-c:v", "libx264",
            "-preset", "ultrafast",
            "-pix_fmt", "yuv420p",
            "-f", "matroska",
            $videoPath
        )
        Write-Host ("[Smoke] Scenario '{0}' capture rect: {1},{2} {3}x{4}" -f $Scenario, $rect.Left, $rect.Top, $captureWidth, $captureHeight)
        $videoProc = Start-Process -FilePath $ffmpeg -ArgumentList $ffmpegArgs -PassThru -WindowStyle Hidden -RedirectStandardError $videoLogPath -RedirectStandardOutput $videoOutLogPath
        Start-Sleep -Milliseconds 500
        if ($videoProc.HasExited) {
            throw "ffmpeg window capture exited immediately; see $videoLogPath"
        }
    }

    try {
        $startDeadline = (Get-Date).AddSeconds(10)
        while ((Get-Date) -lt $startDeadline -and -not (Test-Path $startedPath)) {
            if ($proc.HasExited) { break }
            Start-Sleep -Milliseconds 200
        }
        Assert-True (Test-Path $startedPath) "Scenario '$Scenario' did not start (.started file missing)."

        $started = @{}
        $startedDeadline = (Get-Date).AddSeconds(5)
        while ((Get-Date) -lt $startedDeadline) {
            $started = Read-KeyValueFile $startedPath
            if ($started.ContainsKey("scenario") -and -not [string]::IsNullOrWhiteSpace($started["scenario"])) {
                break
            }
            if ($proc.HasExited) { break }
            Start-Sleep -Milliseconds 100
        }
        Assert-True ($started["scenario"] -eq $Scenario) "Scenario '$Scenario' started wrong mode ('$($started["scenario"])')."
        Assert-True ($started["owner_model"] -eq "kenney_race_body.obj") "Scenario '$Scenario' spawned wrong model ('$($started["owner_model"])')."
        Assert-True ($started["owner_texture"] -eq "kenney_car_colormap.png") "Scenario '$Scenario' spawned wrong texture ('$($started["owner_texture"])')."
        Assert-True ($started["owner_texture_loaded"] -eq "1") "Scenario '$Scenario' failed to load the Kenney texture."
        Assert-True ($started["has_rigidbody"] -eq "1") "Scenario '$Scenario' spawned without RigidbodyComponent."

        $deadline = (Get-Date).AddSeconds(30)
        while ((Get-Date) -lt $deadline) {
            if (Test-Path $logPath) {
                $metrics = Read-KeyValueFile $logPath
                if ($metrics.ContainsKey("status")) { break }
            }
            if ($proc.HasExited) { break }
            Start-Sleep -Milliseconds 200
        }

        $metrics = Read-KeyValueFile $logPath
        Assert-True ($metrics["status"] -eq "COMPLETE" -or $metrics["status"] -eq "FAIL") "Scenario '$Scenario' wrote invalid status ('$($metrics["status"])')."
        Assert-True ($metrics["scenario"] -eq $Scenario) "Scenario '$Scenario' wrote log for wrong scenario ('$($metrics["scenario"])')."

        $scenarioLogPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.log" -f $Scenario)
        $scenarioDebugTextPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.debug.txt" -f $Scenario)
        $scenarioDebugCsvPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.debug.csv" -f $Scenario)
        $scenarioTouchdownTextPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.touchdown.txt" -f $Scenario)
        $scenarioTouchdownCsvPath = Join-Path $Root ("bin\\CarDriveAutotest.{0}.touchdown.csv" -f $Scenario)
        if (Test-Path $logPath) { Copy-Item -Path $logPath -Destination $scenarioLogPath -Force }
        if (Test-Path $debugTextPath) { Copy-Item -Path $debugTextPath -Destination $scenarioDebugTextPath -Force }
        if (Test-Path $debugCsvPath) { Copy-Item -Path $debugCsvPath -Destination $scenarioDebugCsvPath -Force }
        if (Test-Path $touchdownTextPath) { Copy-Item -Path $touchdownTextPath -Destination $scenarioTouchdownTextPath -Force }
        if (Test-Path $touchdownCsvPath) { Copy-Item -Path $touchdownCsvPath -Destination $scenarioTouchdownCsvPath -Force }
        if (Test-Path $videoPath) {
            Write-Host ("[Smoke] Scenario '{0}' video: {1}" -f $Scenario, $videoPath)
        }

        Write-Host ""
        Write-Host "[Smoke] Scenario '$Scenario' metrics:"
        foreach ($name in @(
            "max_contact_ratio", "last_contact_ratio", "max_rear_contact_ratio",
            "max_drive_force", "max_lateral_force", "max_brake_force",
            "max_wheel_under_terrain_depth", "max_body_under_terrain_depth",
            "max_wheel_body_distance", "max_support_point_plane_error",
            "max_suspension_total_force", "max_airborne_suspension_force",
            "max_grounded_pitch_rate_deg", "max_grounded_roll_rate_deg", "ground_flip_events", "landing_spike_events",
            "final_forward_distance", "final_right_distance", "final_yaw_delta",
            "all_wheels_connected", "all_wheels_grounded"
        )) {
            if ($metrics.ContainsKey($name)) {
                Write-Host ("  {0} = {1}" -f $name, $metrics[$name])
            }
        }

        return $metrics
    }
    finally {
        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force
        }
        if ($videoProc -and -not $videoProc.HasExited) {
            Stop-Process -Id $videoProc.Id -Force
        }
        if (Test-Path $enablePath) { Remove-Item $enablePath -Force }
        if (Test-Path $gameBackupPath) {
            Copy-FileWithRetry -Source $gameBackupPath -Destination $GamePath
            Remove-Item $gameBackupPath -Force -ErrorAction SilentlyContinue
        }
        Remove-Item Env:TE_AUTOTEST_CAR -ErrorAction SilentlyContinue
    Remove-Item Env:TE_AUTOTEST_MODE -ErrorAction SilentlyContinue
    Remove-Item Env:TE_AUTOTEST_PRESERVE_SCENE -ErrorAction SilentlyContinue
    Remove-Item Env:TE_AUTOTEST_SHOW_COLLISION -ErrorAction SilentlyContinue
    Remove-Item Env:TE_AUTOTEST_NOACTIVATE -ErrorAction SilentlyContinue
    Remove-Item Env:TE_WINDOW_X -ErrorAction SilentlyContinue
    Remove-Item Env:TE_WINDOW_Y -ErrorAction SilentlyContinue
    Remove-Item Env:TE_WINDOW_W -ErrorAction SilentlyContinue
    Remove-Item Env:TE_WINDOW_H -ErrorAction SilentlyContinue
    }
}

$root = Resolve-Path "."

$scenePath  = Join-Path $root "bin\\scene.json"
$editorPath = Join-Path $root "bin\\Editor.json"
$gamePath   = Join-Path $root "bin\\Game.json"
$prefabPath = Join-Path $root "bin\\Prefabs\\Car.prefab"
if (-not (Test-Path $prefabPath)) { $prefabPath = Join-Path $root "bin\\Prefabs\\Car.json" }

$scene  = Read-JsonFile $scenePath
$editor = Read-JsonFile $editorPath
$game   = Read-JsonFile $gamePath
$prefab = Read-JsonFile $prefabPath

# --- Prefab structure ---
Assert-True ($prefab.Count -ge 5) "Car prefab must contain car body + 4 wheels (found $($prefab.Count) entries)."

$names = @($prefab | ForEach-Object { $_.Name })
foreach ($required in @("Car", "WheelFL", "WheelFR", "WheelBL", "WheelBR")) {
    Assert-True ($names -contains $required) "Car prefab missing object: $required"
}

$car = $prefab | Where-Object { $_.Name -eq "Car" } | Select-Object -First 1
Assert-True ($null -ne $car) "Car object missing in prefab."
Assert-True ($null -ne $car.Components) "Car object has no components."

$componentTypes = @($car.Components | ForEach-Object { $_.type })
Assert-True ($componentTypes -contains "RigidbodyComponent") "Car prefab missing RigidbodyComponent."
Assert-True ($componentTypes -contains "CarController")      "Car prefab missing CarController."

# --- Kenney model & texture wired in prefab ---
Assert-True ($car.Model   -eq "kenney_race_body.obj")    "Car prefab body model must be kenney_race_body.obj (got '$($car.Model)')."
Assert-True ($car.Texture -eq "kenney_car_colormap.png") "Car prefab body texture must be kenney_car_colormap.png (got '$($car.Texture)')."

foreach ($wheelName in @("WheelFL", "WheelFR", "WheelBL", "WheelBR")) {
    $wheel = $prefab | Where-Object { $_.Name -eq $wheelName } | Select-Object -First 1
    Assert-True ($null -ne $wheel) "Missing wheel object: $wheelName"
    Assert-True ($wheel.Model -eq "kenney_wheel_default.obj") "Wheel $wheelName model must be kenney_wheel_default.obj (got '$($wheel.Model)')."
}

# --- Runtime wiring ---
$toolsLauncher = Join-Path $root "tools\\source\\Editor\\ToolsLauncherApp.cpp"
$gameCpp       = Join-Path $root "tools\\source\\Projects\\ExampleGame\\Game.cpp"
Assert-FileExists $toolsLauncher
Assert-FileExists $gameCpp

$initCallCount = @(Select-String -Path $toolsLauncher -Pattern "myGame->Init\(").Count
Assert-True ($initCallCount -ge 1) "Runtime init path is missing myGame->Init() call."

# --- Asset files ---
foreach ($requiredAsset in @(
    (Join-Path $root "ExampleGame\\Assets\\Textures\\Texture.dds"),
    (Join-Path $root "ExampleGame\\Assets\\Textures\\Texture2.dds"),
    (Join-Path $root "ExampleGame\\Assets\\Textures\\kenney_car_colormap.png"),
    (Join-Path $root "ExampleGame\\Assets\\Models\\kenney_race_body.obj"),
    (Join-Path $root "ExampleGame\\Assets\\Models\\kenney_wheel_default.obj"),
    (Join-Path $root "ExampleGame\\Assets\\Prefabs\\Car.prefab"),
    (Join-Path $root "bin\\Prefabs\\Car.prefab")
)) {
    Assert-FileExists $requiredAsset
}

Write-Output "[Smoke] JSON / resources / runtime-wiring checks passed."

if ($Build) {
    $msbuild = "D:\\VisualStudio\\MSBuild\\Current\\Bin\\MSBuild.exe"
    Assert-FileExists $msbuild
    & $msbuild "local\\ExampleGame.vcxproj" "/p:Configuration=Debug" "/p:Platform=x64" "/m"
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    Write-Output "[Smoke] Build passed (Debug x64)."
}

if ($Drive) {
    $msbuild = "D:\\VisualStudio\\MSBuild\\Current\\Bin\\MSBuild.exe"
    Assert-FileExists $msbuild
    & $msbuild "local\\GameLauncher.vcxproj" "/p:Configuration=Debug" "/p:Platform=x64" "/m:1"
    if ($LASTEXITCODE -ne 0) {
        throw "GameLauncher build failed with exit code $LASTEXITCODE"
    }

    $gameExe     = Join-Path $root "bin\\GameLauncher_Debug.exe"

    Assert-FileExists $gameExe

    $playStart = Invoke-CarScenario -Scenario "play_start" -Root $root -GameExe $gameExe -GamePath $gamePath -ScenePath $scenePath -UseRealPlayStartup -CaptureVideo
    $drop      = Invoke-CarScenario -Scenario "drop"       -Root $root -GameExe $gameExe -GamePath $gamePath -ScenePath $scenePath
    $forward   = Invoke-CarScenario -Scenario "forward"    -Root $root -GameExe $gameExe -GamePath $gamePath -ScenePath $scenePath
    $steerLeft = Invoke-CarScenario -Scenario "steer_left" -Root $root -GameExe $gameExe -GamePath $gamePath -ScenePath $scenePath
    $steerRight= Invoke-CarScenario -Scenario "steer_right" -Root $root -GameExe $gameExe -GamePath $gamePath -ScenePath $scenePath

    foreach ($scenario in @(
        @{ Name = "play_start"; Metrics = $playStart },
        @{ Name = "drop"; Metrics = $drop },
        @{ Name = "forward"; Metrics = $forward },
        @{ Name = "steer_left"; Metrics = $steerLeft },
        @{ Name = "steer_right"; Metrics = $steerRight }
    )) {
        $m = $scenario.Metrics
        $name = $scenario.Name
        $metricsDump = ($m.GetEnumerator() | ForEach-Object { "{0}={1}" -f $_.Key, $_.Value }) -join "`n"
        $wheelPenetrationLimit = if ($name -eq "play_start") { 0.30 } else { 0.18 }

        Assert-True ($m["status"] -eq "COMPLETE") "Scenario '$name' did not complete successfully.`nMetrics:`n$metricsDump"
        Assert-True ((Get-IntMetric $m "all_wheels_connected") -eq 1) "Scenario '$name': not all wheels were connected."
        Assert-True ((Get-FloatMetric $m "max_wheel_under_terrain_depth") -le $wheelPenetrationLimit) "Scenario '$name': wheel penetration too high."
        Assert-True ((Get-FloatMetric $m "max_body_under_terrain_depth") -le 0.08) "Scenario '$name': body penetration too high."
        Assert-True ((Get-FloatMetric $m "max_wheel_body_distance") -le 4.5) "Scenario '$name': wheel separated too far from chassis."
        Assert-True ((Get-FloatMetric $m "max_support_point_plane_error") -le 0.45) "Scenario '$name': support/contact alignment drifted too far from terrain."
        Assert-True ((Get-FloatMetric $m "max_airborne_suspension_force") -le 800.0) "Scenario '$name': suspension stayed too strong while airborne."
        Assert-True ((Get-IntMetric $m "ground_flip_events") -eq 0) "Scenario '$name': detected grounded flip/spin event."
        Assert-True ((Get-IntMetric $m "landing_spike_events") -eq 0) "Scenario '$name': detected landing explosion spike."
        Assert-True ((Get-FloatMetric $m "max_grounded_pitch_rate_deg") -le 260.0) "Scenario '$name': grounded pitch rate too high."
        Assert-True ((Get-FloatMetric $m "max_grounded_roll_rate_deg") -le 260.0) "Scenario '$name': grounded roll rate too high."
    }

    Assert-True ((Get-FloatMetric $playStart "max_wheel_body_distance") -le 3.5) "Play-start test: wheel separated too far from chassis during normal Play startup."
    Assert-True ((Get-FloatMetric $playStart "max_airborne_suspension_force") -le 400.0) "Play-start test: airborne suspension force too high during normal Play startup."
    Assert-True ((Get-IntMetric $playStart "ground_flip_events") -eq 0) "Play-start test: grounded flip/spin detected during normal Play startup."
    Assert-True ((Get-IntMetric $playStart "landing_spike_events") -eq 0) "Play-start test: landing spike detected during normal Play startup."

    Assert-True ((Get-FloatMetric $drop "last_contact_ratio") -ge 0.5) "Drop test: car never settled onto meaningful wheel contact."
    Assert-True ((Get-FloatMetric $drop "max_contact_ratio") -ge 0.5) "Drop test: contact profile too weak during landing."

    $forwardDistance = Get-FloatMetric $forward "final_forward_distance"
    $forwardRight = [math]::Abs((Get-FloatMetric $forward "final_right_distance"))
    Assert-True ($forwardDistance -ge 6.0) "Forward test: car did not move forward enough."
    Assert-True ($forwardDistance -ge ($forwardRight * 2.0)) "Forward test: car drifted sideways too much relative to forward motion."
    Assert-True ([math]::Abs((Get-FloatMetric $forward "final_yaw_delta")) -le 18.0) "Forward test: yaw drift too large for straight drive."
    Assert-True ((Get-FloatMetric $forward "max_drive_force") -gt 0.0) "Forward test: no drive force was applied."

    $leftForward = Get-FloatMetric $steerLeft "final_forward_distance"
    $leftRight = Get-FloatMetric $steerLeft "final_right_distance"
    $leftYaw = Get-FloatMetric $steerLeft "final_yaw_delta"
    Assert-True ($leftForward -ge 4.0) "Steer-left test: car did not keep moving forward."
    Assert-True ($leftYaw -le -8.0) "Steer-left test: yaw did not turn left."
    Assert-True ($leftRight -le -0.5) "Steer-left test: path did not shift left."

    $rightForward = Get-FloatMetric $steerRight "final_forward_distance"
    $rightRight = Get-FloatMetric $steerRight "final_right_distance"
    $rightYaw = Get-FloatMetric $steerRight "final_yaw_delta"
    Assert-True ($rightForward -ge 4.0) "Steer-right test: car did not keep moving forward."
    Assert-True ($rightYaw -ge 8.0) "Steer-right test: yaw did not turn right."
    Assert-True ($rightRight -ge 0.5) "Steer-right test: path did not shift right."

    Write-Output "[Smoke] Vehicle behavior smoke passed: drop, forward, steer-left, steer-right."
}
