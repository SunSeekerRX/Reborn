# 第一场景出生区域修复（0.6.2）

默认游戏地图仍为 `/HorrorSystems/Maps/Basic_roomA`，新游戏进入第一阶段。

A 场景的 `CubeGridToolOutput2` 是另一套完整房间白模，叠放在第一房间内。它的额外地板和墙体抬高出生点，并封住前往大厅的路径。该 Actor 已保留为编辑器参考：仅编辑器使用、游戏中隐藏、关闭 Actor 和网格组件碰撞。组合场景 Basic_roomABC 中同名的合法 B 房间不受影响。

Safe_A 出生点恢复为 (-200, -1020, 292)。`RepairFirstRoomStart.py` 只修改 A 场景；`ConfigureBasicGameplay.py` 也应用同样设置，避免重新配置时再次引入问题。原始地图备份位于 Saved/Verification/BeforeFirstRoomStartRepair。

新增 `HorrorSystems.Progression.FirstRoomPath` 自动测试：从实际出生点使用角色移动、重力和碰撞，走过安全屋出口并转入大厅；同时验证第一房间、第一阶段。修复前在 StaticMeshActor_10 墙体处失败，组合场景对照通过；修复后独立 A 场景通过。
