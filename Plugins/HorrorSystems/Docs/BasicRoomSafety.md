# Basic_room 安全屋

## 当前场景1：Basic_roomA

新版场景路径为 `/HorrorSystems/Maps/Basic_roomA`。原有白模、门洞和家具全部保留，新增 `SceneOne_SafeArea` 管理器。

地面 Z=200，出生点调整为 (-200, -1020, 300)，朝向 Yaw=90，面向出口。安全区中心 (-200, -850, 400)，半尺寸 (200, 250, 200)，覆盖小房间从地板到天花板的内部区域。PlayerStartTag=Safe，GameMode=HSGameMode。规则仍为 DA_BasicRoomSafety，60 秒。

重新配置此场景使用 `Tools/ConfigureSceneOneSafety.py`，无需 GeometryScripting，不要使用旧的 ConfigureBasicRoomSafety.py 覆盖新布局。

新版场景编译与 BasicRoomSafety 运行测试通过：SafeArea 内保持 60 秒；角色实际高度的胶囊扫掠通过门洞；完全离开后倒计时启动；实体空气墙阻挡返回；尝试返回后计时继续。报告为 Saved/Verification/SceneOneSafetyAutomation/index.json，0 警告、0 失败。

## 初版白模记录

已接入 `/HorrorSystems/Maps/Basic_room` 与 `/Game/Basic_room` 两份关卡。

出生小房间内保持 60 秒，不消耗时间。角色胶囊完全离开安全区后，开始倒计时并启用阻挡 Pawn 的空气墙。尝试返回不会暂停计时。倒计时结束沿用 HSRoomDirector 的回滚逻辑，重新加载当前关卡、返回 Safe 出生点，并恢复安全屋和 60 秒预算。

在世界大纲中选择 **BasicRoom_SafeArea**：

- `Rules`：`/HorrorSystems/Rooms/DA_BasicRoomSafety`，Duration=60。
- `SafeArea`：中心 (-200, -850, 300)，半尺寸 (200, 250, 200)，单位厘米。
- `ReturnBarrier`：与安全区相同，进入关卡时没有碰撞，离开后自动阻挡玩家。
- `SafeSpawnTag`：Safe；关卡中的 PlayerStart 已设置相同标签。
- 首次窗户演出关闭，安全屋行为不依赖演出。

白模使用 `SM_BasicRoomSafeExit`，保留原始 CubeGridToolOutput 资源。新网格在朝向大厅的墙上提供 160 厘米宽、300 厘米高的门洞，并使用复杂碰撞作为简单碰撞，使门洞可通行。原始关卡备份位于 Saved/Verification/BasicRoomBeforeSafety。

调整安全区时，必须包住整个出生小房间，并保证 Safe 出生点在其中。无需手动设置 ReturnBarrier 的启用时间；角色离开后代码会同步其尺寸并开启碰撞。

`Tools/ConfigureBasicRoomSafety.py` 是本关卡的配置工具，需要 GeometryScripting，仅在编辑器内运行。正式关卡运行不依赖 GeometryScripting。不要在已有未保存编辑内容时重新执行该配置工具。

运行验证：以 Basic_room 为游戏地图执行自动化测试 `HorrorSystems.Progression.BasicRoomSafety`，检查屋内冻结、门口胶囊边界、实体门洞通行、出门启动以及空气墙回返阻挡。

验证结果：RebornEditor 编译通过；BasicRoomSafety 实际关卡测试通过，0 警告、0 失败。记录位于 Saved/Verification/BasicRoomSafetyBuild.log、BasicRoomSafetyTest.log 和 BasicRoomSafetyAutomation/index.json。白模工具插件标为 Optional，因此未安装该编辑工具时也能打开并运行已有白模关卡。
