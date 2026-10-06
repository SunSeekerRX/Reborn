param([ValidateSet('Visual','Escape','Core')][string]$Group)
$ErrorActionPreference='Stop'
$taskRoot='C:\Projects\Game Design Projects\Reborn\Reborn'
$taskUE='C:\Datas\Workshops\GameDesignWorkshops\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
New-Item -ItemType Directory -Path "$taskRoot\Saved\Verification" -Force | Out-Null
$taskTests=switch($Group){
'Visual'{@(@('Basic_roomB','Scene.FurnitureFacing','LightingRegressionFacing'),@('Basic_roomB','Basic.Interactions','LightingRegressionInteractions'))}
'Escape'{@(@('Basic_roomABC','Progression.FinalEscape','LightingRegressionEscape'))}
'Core'{@(@('Basic_roomA','Basic.RoomAMovement','LightingRegressionMovement'),@('Basic_roomA','UI.PickupDoubleNumber','LightingRegressionPickup'),@('Basic_roomA','UI.Inspection3D','LightingRegressionInspection'),@('Basic_roomA','Progression.SafeWhiteTravel','LightingRegressionTravel'))}
}
if($Group -eq 'Escape'){$taskTests=,@('Basic_roomABC','Progression.FinalEscape','LightingRegressionEscape')}
foreach($taskTest in $taskTests){
$taskFlags=if($Group -eq 'Visual'){@('-windowed','-ResX=1280','-ResY=720','-HSVisualTest','-HSNewRoomArt')}else{@('-nullrhi')}
& $taskUE "$taskRoot\Reborn.uproject" "/HorrorSystems/Maps/$($taskTest[0])" -game -unattended -nosound -nop4 @taskFlags "-ExecCmds=DisableAllScreenMessages,Automation RunTests HorrorSystems.$($taskTest[1])" '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$taskRoot\Saved\Verification\$($taskTest[2])Report" "-abslog=$taskRoot\Saved\Verification\$($taskTest[2]).log" *> "$taskRoot\Saved\Verification\$($taskTest[2])Console.log"
if($LASTEXITCODE -ne 0){throw "$($taskTest[2]) failed $LASTEXITCODE"}
}



