# 安全屋与白光传送（0.7.0）

> 本文记录 0.7.0 的初始路线。当前三关顺序、返程阶段衔接和第三关出口已按用户明确需求改为 0.7.1，请以 ThreeStageFlow.md 为准。

第一关、第二关使用 Basic_roomA、Basic_roomB、Basic_roomC 独立地图；第三关使用 Basic_roomABC 对应的组合地图。目前 Basic_roomABC 是资源重定向，实际保存地图为 Basic_roomABC_unchange1，传送规则已引用实际地图。

## 玩家流程

1. 从 A 安全屋出生，安全屋内不计时。
2. 完全走出安全区域后，开始 60 秒倒计时，并封住返程入口。
3. 获取当前房间、当前阶段所需的关键物品后，任务变成“赶紧返回安全屋”，空气墙立即解除。此时外面仍计时。
4. 返回安全区域后，倒计时停止，任务变成“进入白光传送门”，安全屋后侧的新白光门打开。
5. 走进白光门后暂时锁定操作，画面渐白，在完全遮挡后加载下一地图，再从对应安全屋恢复视角与操作。下一房间重新获得完整 60 秒，离开安全屋前不计时。
6. 超时仍执行原有回滚：返回当前房间安全出生点，清除本次在该房间取得的物品及线索，恢复倒计时和可拾取物品。

第一关路线 A→B→C→第二关 A；第二关 A→B→C→第三关 ABC 的 Safe_A。C 后的阶段衔接是本次采用的默认安排，可在规则资源中修改。ABC 本次只适配第三关出生和房间识别，第三关具体玩法另行配置。

## 在中文 UE 编辑器中配置

打开 A/B/C，场景中新增的传送 Actor 分别命名为 Reborn_SafePortal_A/B/C。原房屋、家具模型保持原样，原大厅出口的 Travel Enabled 关闭，避免绕过安全屋返程流程。新门只在允许传送时显示；封闭时不额外挡住出生点。

选中 Reborn_RoomDirector（或原有房间管理器），在“细节”中检查：

- Rules：DA_SafeTravelRoomA/B/C。
- Return To Safe After Objective：开启。
- Safe Center / Safe Extent：安全区域的中心和半尺寸。
- Safe Spawn Tag：Safe_A / Safe_B / Safe_C，对应 Player Start 的 Player Start Tag。
- Minimum Stage：独立房间为 1；组合第三关管理器为 3。

选中新白光门，检查 Travel Enabled、Requires Safe Return、White Light Travel 都开启，Source Rules 与当前管理器一致，Spawn Tag 指向目的房间出生标签。Trigger 控制穿门范围，Marker 是新增的占位白光模型，Portal Light 是附加光源；可以替换这些新增组件的美术或调整位置，无需更换原有房屋 Actor。

关键物品的 Clue Id 要与规则 Required Clues 一致。当前每个房间使用阶段对应的 Key_1 / Key_2。只有规则所需线索全部取得，才允许返程开门；普通物品不会提前开门。规则内 Stage 1 / Stage 2 Routes 可修改目的地图、出生标签与是否进入下一阶段。

## 代码和配置工具

- HSRoomActors：离开安全屋、计时、解封、返程完成状态。
- HSWorldActors：白光门显示、进入触发及阶段路线。
- HSWorldState：跨地图持续白光覆盖、加载期间遮挡、输入恢复。
- HSOverlay：寻找物品 / 返回安全屋 / 进入白光门任务文本。
- Plugins/HorrorSystems/Tools/ConfigureSafeTravel.py：当前地图的安全屋和传送规则配置工具。执行前会备份地图，校验原有模型的资源、变换与材质没有被改动。

旧 ConfigureRoomAPreview.py 会关闭跨房间出口，不应再次用于本版本。ConfigureBasicGameplay.py 是旧白模重建工具，可能重建玩法和模型；当前手工布景请使用 ConfigureSafeTravel.py 适配，不要运行白模重建。

## 验证

SafeWhiteTravel 自动化以真实移动进入触发区，连续完成六次地图切换，检查每次的地图、阶段、出生安全区域、60 秒预算、输入锁定/恢复及加载前后白光覆盖。ReturnToSafe 分别验证 A/B/C 解封与返程停止计时。另验证移动、检视、超时重载与物品回滚。报告与实际渲染截图保存在 Deliverables/Verification/SafeTravel。

白光用于掩盖正常地图加载，因此加载较慢的机器可能停留白屏更久；本次不是后台异步流送方案。
