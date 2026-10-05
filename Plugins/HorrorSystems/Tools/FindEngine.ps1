param([string]$EngineRoot)
if ($EngineRoot -and (Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version'))) {
    $version = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
    if ($version.MajorVersion -eq 5 -and $version.MinorVersion -eq 8) { return $EngineRoot }
}
$launcherFile = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
if (Test-Path -LiteralPath $launcherFile) {
    $installs = (Get-Content -LiteralPath $launcherFile -Raw | ConvertFrom-Json).InstallationList
    foreach ($install in $installs) {
        if ($install.AppName -eq 'UE_5.8') { return $install.InstallLocation }
    }
}
throw 'UE 5.8 installation not found. Pass -EngineRoot with the engine directory.'
