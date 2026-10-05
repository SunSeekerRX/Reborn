# Reborn 工程改名

工程目录、uproject、主模块、Game/Editor Target、启动与构建脚本、产品标题和交付文件名统一为 Reborn。HorrorSystems 内部模块和资源挂载路径保留，以维持既有地图/蓝图资源引用。

RebornEditor Win64 Development 编译成功；改名后在 Reborn 工程执行 7 项回归测试，全部通过、0 警告、0 失败。最新结果：`Saved/Verification/Automation/index.json`、`RuntimeTests.log` 和 `RenameBuildAndTests.log`。

# 0.5.0 最新验证（2026-10-05）

UE 5.8.2 Editor Development 编译成功。7 项 D3D12 实机自动化全部通过，0 测试警告、0 失败：Inventory、IntegrationAvailable、TimerRollbackAndStages、RoomFlow、EndToEnd、MovementEdges、RigAndCamera。

RoomFlow 实际加载 A/B/C 九次进门，验证首次窗户演出及输入恢复、怪物转向窗内、玄关无计时、离开后空气墙物理阻挡、锁门碰撞、E 移动抽屉、E 拾取线索、唯一 DefaultSlot 和正在播放的 PickupMontage、检视暂停计时、超时回到当前玄关与物品恢复、阶段推进和第三阶段结束。TimerRollbackAndStages 验证保留前房间物品、撤销本房间新增物品/线索、独立预算、全部预算重置、失效路由拒绝。

MovementEdges 实测直线和斜向同速、左右平移/后退、奔跑墙体碰撞、脚底落地、锁定输入、怪物慢/普通/快速度及空间音效配置。EndToEnd 与 RigAndCamera 覆盖原有奔跑、Ctrl 模式、十格检视拖动、导航追踪、第一人称相机与骨骼 IK。

最新原始结果：`Saved/Verification/RoomVisualAutomation/index.json`、`RoomVisualTests.log`。截图：WindowCinematic、WindowMonsterLook、RoomSafe、FirstPersonPickup、RoomInspection、RoomTimeoutReset、EndingWhiteFlash、GameEnding（均为 png）。

音效使用合成占位资产，已绑定玩家脚步、怪物脚步和自身循环音、物件移动、拾取/检视/传送；怪物声音有空间衰减。正式角色、美术、动作捕捉与正式录音仍可在插件配置处替换。未实现攻击或玩家死亡。

## 旧版本验证记录

# 本机验证记录

验证时间：2026-10-04。环境：UE 5.8.2、Windows 11、Visual Studio 2022 14.44、RTX 5060 Ti。

## 已通过

- RebornEditor / Win64 / Development：编译成功。
- Reborn / Win64 / Development：编译成功（不代表已经 Cook/打包为发行游戏）。
- 原生 PolicyTests：0 failures，覆盖十格容量/索引、Ctrl 短长按判定、移动速度优先级、AI 速度连续性。
- UE Automation 实际 D3D12 运行：3 succeeded，0 succeededWithWarnings，0 failed，0 notRun。
- ContentAudit：64 个插件资源，0 个宿主 `/Game` 依赖，Test_A/B/C 均通过道具、传送门、怪物和导航范围检查。

## 实际游戏验证范围

Inventory.CapacitySwapAndSession 验证十格容量、第十一件拒绝、重复道具键拒绝、空格交换、选中跟随和会话重置。

Runtime.EndToEnd 验证真实模板角色和动画实例、WASD 输入、Shift 奔跑、Ctrl 长按慢走/短按蹲下、蹲下速度优先级、NavMesh 怪物追踪和近距减速、深度视野材质、E 对准实际场景道具拾取、拾取动作状态、检视时世界与怪物暂停、关闭检视恢复、实际 Slate 鼠标拖动 1→6 格、A→B→C→A 传送以及道具/选中格/已拾取记录保留。

RigAndCamera 验证第一人称相机直接挂接胶囊体、使用控制器视角、身体水平朝向同步、头颈隐藏、四肢十二个骨骼、滚轮 FOV 缩放及上下限、UI 模式不改变缩放、上下俯仰、蹲下眼部降低、最终眼部位置保持在胶囊范围内。双腿 IK 蹲下时骨盆下降 38 cm，脚底高度误差小于 0.1 cm，腿骨长度保持不变；EndToEnd 同时检查拾取时右手接近真实道具目标与手臂长度保持不变。修复前的对照结果保留在 `Saved/Verification/RigCameraBefore` 和 `FirstPersonBefore`。

最新第一人称截图：`Saved/Verification/Gameplay.png`、`Crouch.png`、`FirstPersonCrouch.png`、`Inspection.png`、`Hotbar.png`。`RigCrouch.png` 是上一版第三人称 IK 的对照截图。

主角近距常亮点光源：RigAndCamera 检查 PlayerAuraLight 的挂接、可见性、亮度、短半径及阴影。实际 D3D12 对照截图 `PlayerLightOff.png` / `PlayerLightOnly.png` 中关闭场景灯和前方光束，并转向 180°：关掉点光时附近全黑，仅启用主角点光时近处地面持续可见、远处仍黑，验证不依赖场景灯或前方光束。亮度默认 900 流明，半径 450 cm。

原始结果：`Saved/Verification/VisualAutomation/index.json`、`VisualTests.log`、`ContentAudit.json`、`ContentAudit.log`。

## 可复现

在工程目录运行 `构建与验证.ps1 -Visual`，重新编译并执行实际渲染测试；不带 -Visual 则使用 NullRHI。运行 `Tools/TestPolicy.cmd` 验证独立策略函数。

测试场景和系统已实现；正式关卡、美术、专门制作的蹲下/拾取动画与正式音效仍需后续替换。会话跨关卡保留，退出游戏后清空。

## 无提示界面与 Windows 包

已移除 HUD 教学文字、快捷栏数字与名称、拾取通知、检视暂停/键位说明，以及三张地图的 WelcomeSign/PortalSign；隐藏拾取物和传送门的世界标签。道具正文仅保留叙事内容。最新 Gameplay.png / Inspection.png 已检查清理效果。

Win64 Shipping BuildCookRun 成功，包含 A/B/C、原生模块、插件资源、Pak/IoStore 和运行库安装程序。日志：`Saved/Verification/Packaging.log`；复现脚本：`Tools/PackageWindows.ps1`。最终游戏与工程压缩包位于工程内部 `Deliverables`。

清理地图后发现部分缓存导航缺失，已在 HSGameMode::StartPlay 对动态导航执行完整构建。回归测试不主动修复导航，并新增远处怪物导航投影及到玩家路径检查；三项自动化测试全部成功。最新结果：`Saved/Verification/Automation/index.json` 与 `RuntimeTests.log`。
