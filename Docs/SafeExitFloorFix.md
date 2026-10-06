# 安全屋出口地面修复

2026-10-05：在 Basic_roomABC、Basic_roomB、Basic_roomC 复现了走出安全屋后掉进坑、无法继续前进的问题。

原开门洞的脚本把名义地面高度 100 cm 当作切割体底部，但实际出口地面在 200 cm。切割体挖掉了门口约 100 cm 厚的地板，留下宽约 220 cm 的凹坑。玩家落下后，前方 100 cm 台阶超过角色可跨越高度，身后的安全屋空气墙又已启动。

修复从保存的原始房间网格重建开口，先通过几何射线读取真实地面，再把切割体底部设在地面上方 1 cm；保留地板，只切除门口墙体。四张 Basic 地图的六个安全屋出口均已修复，家具、怪物、物品及既有 Actor 配置不变。独立 A 场景额外叠放的房间地面原本覆盖了坑，因此此前未表现出同样掉落。

重建工具 ConfigureBasicGameplay.py 使用相同的 BasicGeometry.py 几何方法，避免再次生成凹坑。RepairSafeExitFloors.py 只修复现有房间网格，备份位于 Saved/Verification/BeforeSafeExitFloorRepair，修复记录为 SafeExitFloorRepair.json。

新增 HorrorSystems.Progression.SafeExitFloor 测试，沿出口每 50 cm 检查落地高度，并实际驱动角色从出生区走到大厅；ABC 场景依次验证三个房间。独立 A/B/C 和组合 ABC 的六段实际行走均通过。另复测组合地图的安全屋倒计时、空气墙及三阶段传送循环，均通过。

原来的安全屋测试使用水平胶囊扫掠，验证门洞和空气墙，却没有让角色受重力连续走过门口，漏掉了这处凹坑。新增测试补上了地面连续性与实际行走验证。
