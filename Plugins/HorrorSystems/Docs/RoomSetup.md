# 三房间与阶段配置（0.5.0）

## 已接入 Basic 地图

三张地图 Basic_roomA / Basic_roomB / Basic_roomC 各有一个玄关、锁门、窗户、移动抽屉和追踪怪物。首次进入每张地图播放约 9 秒窗户演出：镜头移入窗户、上下黑边、怪物慢走经过、看向窗内、快速离场、镜头回到玩家。演出期间禁用输入和追踪，不消耗房间时间。演出每张地图每次游戏只播放一次，超时重试不重播。

玄关不计时。角色胶囊完全离开玄关后，顶端 60 秒条开始消耗，整个玄关的 Pawn 空气墙开启，防止从其他方向返回。指南针在时间条下方。左上面板显示当前房间当前阶段所需线索。门有实体碰撞，拿到关键线索后解除碰撞，走入门触发传送。

默认每阶段按 A→B→C→A；C 出口推进阶段。第三阶段 C 出口触发全屏白光并显示结束画面，可以重新开始。每阶段每房间有一把钥匙；全程共九件必需道具，加一张可选纸条，恰好不超过十格容量。

超时会撤销本次进入当前房间后新增的物品、已拾取记录和线索，重置所有房间时间，重载当前房间回到玄关。此前房间的物品保留，当前物品恢复可拾取；抽屉与怪物位置随关卡重载复位。进入新房间时该房间有新的完整预算；其他房间的预算独立保存。检视暂停世界及倒计时。鼠标快捷栏模式不暂停倒计时。

## 放入正式的三个关卡

所有系统与依赖资源位于此插件内。先设置 World Settings 的 GameMode 为 HSGameMode，并配置 NavMeshBoundsVolume、HSVisionRig、HSMonster。

1. 每张地图放一个 **HSRoomDirector**。给 `Rules` 指定 UHSRoomRules 数据资产，如 `/HorrorSystems/Rooms/DA_RoomA`。`RoomId` 必须唯一，`RoomName` 用于线索栏，`Duration` 默认 60 秒。
2. 把 Director 放在玄关中心，调整 **SafeArea** 组件的 BoxExtent 包住可活动区域。玄关的实体墙只留一个出口；玩家胶囊完全离开体积才封闭返回空间。无需另建空气墙 Actor。
3. 在玄关放 **PlayerStart**，将 PlayerStartTag 分别设置为 `Safe_A`、`Safe_B`、`Safe_C`，与 Director.SafeSpawnTag 一致；默认起点另设 `Default`。起点必须位于 SafeArea 内，离传送触发框和墙体保持距离。
4. 放 **HSPortal**，启用 `bUseStageRoute`，将 `RoomRules` 指向同一数据资产。Marker 是锁门实体网格，Trigger 是门的入口区域；可替换为正式门模型并调整组件尺度。保留相互邻近的门与触发框。
5. 数据资产的 **Routes** 每阶段配置一项：`Stage`=1/2/3、`TargetRoom`、`Destination`=正式关卡软引用、`RequiredClues`=该阶段关键线索 ID。`bAdvanceStage` 控制通过后推进阶段；`bFinishAtFinalStage` 在第三阶段通过时结束游戏。连接关系可在每阶段分别指定，不必一直是循环。
6. 放 **HSPickup**，指定 ItemData、稳定且唯一的 PickupId、ClueId。该 ClueId 必须与 RequiredClues 一致。MinimumStage/MaximumStage 控制在哪些阶段出现。线索按阶段和房间独立保存，同名线索不会解锁其他房间。`ClueLabels` 配置线索栏中的名称。
7. 房间配置不完整或缺少当前阶段路由时，门保持锁定。普通非阶段传送门可禁用 bUseStageRoute，使用原有 Destination/SpawnTag。

蓝图通过 **Get Game Instance Subsystem → HSProgression** 查询 Stage、ActiveRoom、Remaining、HasClue 等。自动阶段推进由 CommitExit 完成。需要主动切换阶段时，调用当前 **HSRoomDirector.ChangeStage(1/2/3)**，它回滚本次房间获取并重载玄关，刷新阶段限定道具；返回 bool 表示是否接受。检视、演出、传送中不应发起额外阶段切换。

## 拾取、检视和移动物件

HSCharacter 默认绑定 `/HorrorSystems/Characters/Interaction/AM_FirstPersonPickup`，原生动画实例提供 **DefaultSlot**。短片提供手腕转动/手指收拢，右臂组件空间 Two-Bone IK 伸向真实道具位置，无根运动。第一人称身体保留四肢，隐藏头颈，相机不跟随头部动画。PickupPoseAlpha 和 PickupTargetLocation 可供正式动画蓝图使用。

正式角色需匹配自己的 Skeleton、Animation Blueprint 和 Montage Slot，把角色的 PickupMontage 替换为兼容动画。系统按 Montage 长度等待再打开自动检视；`ItemData.bInspectOnPickup` 控制自动检视，R/右键仍可手动打开可旋转的 3D 模型与文字检视。当前提供的是模板骨骼测试动作，正式角色资源可以直接替换。

放 **HSMovableProp**，替换 Mesh，配置 `OpenOffset`、`OpenRotation`、`MoveDuration`。按 E 对准无遮挡且 2.8 米内的物件可往返移动/开合；ClueOnOpen 可在首次打开时授予线索。超时重载会恢复物件初始变换。

## 窗户演出

放 **HSWindowSequence** 在窗户中心，链接到 Director.WindowSequence。窗户白模必须留真实开口，确保观看点与怪物路径之间无遮挡。

- CameraOffset：相对窗户的观看点，默认向室内 350 cm，高 175 cm。
- WindowViewRotation：相对 Actor 的观看方向，默认朝 -Y。Camera FOV 默认 65°。
- MonsterStart / MonsterLookPoint / MonsterEnd：相对窗户位置，单位 cm；怪物走到 LookPoint，看向窗内后快速移到 End。脚底需匹配外侧地面高度。
- Duration：整体演出时长，默认 9 秒。上下黑边各占屏高 12%，HUD 在演出期间隐藏。
- 可以禁用 Director.bPlayFirstEntrySequence。演出中的怪物是独立临时角色，不会把房间追踪怪物移动到窗外；演出结束自动销毁。

## 怪物状态与音效

HSMonster.MovementState 提供 **Slow / Normal / Fast**，默认距离/可见性自动切换：近处或可见慢 160 cm/s，中距离普通 300，远处快 540。速度变化平滑，导航每 0.3 秒刷新目标；125 cm 停止，180 cm 外恢复。`SetMovementState(State,false)` 可强制指定速度档，传 true 切回自动控制。玄关、窗户演出、检视暂停和游戏结束时停止追踪。未接入攻击、死亡或杀死玩家。

音效可直接替换 SoundBase：HSCharacter.FootstepSound、HSMonster.FootstepSound/PresenceSound、HSMovableProp.InteractionSound、ItemData.PickupSound/InspectSound、HSPortal.TravelSound、HSAmbientZone.Sound。怪物脚步与自身循环音具备空间衰减；PresenceSound 要开启 Looping。当前声音为合成测试音效。

## 重建与验证

旧 Test 地图生成工具已归档到 Archive/LegacyTestScenes。当前 Basic 配置使用 Tools/ConfigureBasicGameplay.py，美术导入和应用使用 PrepareArtAssets.py、RigButcher.py、ApplyArtAssets.py；这些重建工具不需要在正常游玩时执行。

工程根目录 `构建与验证.ps1 -Visual` 验证真实地图、Slate、导航、阶段、回滚、动画、相机与物理碰撞。7 项自动化测试：Inventory、IntegrationAvailable、TimerRollbackAndStages、RoomFlow、EndToEnd、MovementEdges、RigAndCamera。游戏完成状态和首次演出记录仅保存在当前游戏会话，退出后重新开始。
