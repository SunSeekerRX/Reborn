$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'Reborn.uproject'
$outputRoot = Join-Path $projectRoot 'Deliverables/WindowsGame'
$uat = 'C:/Datas/Workshops/GameDesignWorkshops/UnrealEngine/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat'
& $uat BuildCookRun "-project=$projectFile" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -iostore -archive "-archivedirectory=$outputRoot" -prereqs -utf8output
if ($LASTEXITCODE -ne 0) { throw "Windows packaging failed: $LASTEXITCODE" }
Write-Host "Windows game: $outputRoot/Reborn.exe"
