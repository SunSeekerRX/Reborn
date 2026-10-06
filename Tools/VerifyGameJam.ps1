param(
    [string]$EngineRoot,
    [ValidateSet('All','Story','Movement','Window','Detailed')][string]$Group='All',
    [string]$ReportRoot,
    [switch]$Visual
)
$ErrorActionPreference='Stop'
$taskProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskEngine=& (Join-Path $taskProjectRoot 'Plugins/HorrorSystems/Tools/FindEngine.ps1') -EngineRoot $EngineRoot
$taskEditor=Join-Path $taskEngine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if(!$ReportRoot){$ReportRoot=Join-Path (Split-Path $taskProjectRoot -Parent) ('LocalArtifacts/GameJamVerification_'+(Get-Date -Format 'yyyyMMdd_HHmmss'))}
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
if($ReportRoot.StartsWith($taskProjectRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'ReportRoot must be outside the Git project.'}
New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null
$taskRuns=@()
if($Group -in @('All','Detailed')){
    $taskRuns+=,@('Basic_roomABC_unchange1','Jam.FinalChaseNavigation')
    $taskRuns+=,@('Basic_roomABC_unchange1','Jam.FinalTableLure')
    $taskRuns+=,@('Basic_roomB','Jam.PaintingInteraction')
    $taskRuns+=,@('RebornTitle','Jam.PhysicalWalkthrough')
    $taskRuns+=,@('Basic_roomA','Jam.TimeoutRecovery')
}
if($Group -in @('All','Story')){
    $taskRuns+=,@('Basic_roomA','Jam.FullStory')
    $taskRuns+=,@('Basic_roomA','Progression.FirstRoomPath')
    $taskRuns+=,@('Basic_roomABC_unchange1','Progression.FinalEscape')
}
if($Group -in @('All','Movement')){
    $taskRuns+=,@('Basic_roomA','Basic.RoomAMovement')
    $taskRuns+=,@('Basic_roomA','UI.PickupDoubleNumber')
    $taskRuns+=,@('Basic_roomA','UI.Inspection3D')
    $taskRuns+=,@('Basic_roomA','Inventory.CapacitySwapAndSession')
    $taskRuns+=,@('Basic_roomA','Progression.TimerRollbackAndStages')
    $taskRuns+=,@('Basic_roomA','Combat.SceneOnePursuit')
}
if($Group -in @('All','Window')){$taskRuns+=,@('Basic_roomB','Basic.WindowCamera')}
foreach($taskRun in $taskRuns){
    $taskName=$taskRun[1];$taskLog=Join-Path $ReportRoot ($taskName+'.log')
    $taskReport=Join-Path $ReportRoot ($taskName+'Report')
    $taskFlags=@(if($Visual){'-windowed';'-ResX=1280';'-ResY=720';'-HSVisualTest'}else{'-nullrhi'})
    & $taskEditor (Join-Path $taskProjectRoot 'Reborn.uproject') ('/HorrorSystems/Maps/'+$taskRun[0]) -game -unattended -nop4 @taskFlags "-ExecCmds=DisableAllScreenMessages,Automation RunTests HorrorSystems.$taskName" '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$taskReport" "-abslog=$taskLog" *> (Join-Path $ReportRoot ($taskName+'Console.log'))
    if($LASTEXITCODE -ne 0){throw "$taskName process failed: $LASTEXITCODE"}
    $taskSummary=Get-Content -LiteralPath (Join-Path $taskReport 'index.json') -Raw | ConvertFrom-Json
    if($taskSummary.failed -gt 0 -or ($taskSummary.succeeded+$taskSummary.succeededWithWarnings) -lt 1){throw "$taskName did not pass; see $taskLog"}
    Write-Host "PASS $taskName"
}
Write-Host "Reports: $ReportRoot"
