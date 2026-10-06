# Reborn 恐怖游戏基础系统

适用：UE **5.8.2**，Windows，单机第一人称。系统代码、模板人物资源、Basic 地图、数据资产和已导入素材全部放在这个插件目录。

## 立即体验

打开 `Reborn.uproject`，启动地图是 `Basic_roomA`，点击编辑器 Play。

| 操作 | 效果 |
| --- | --- |
| WASD / 鼠标 | 相机相对移动 / 转动视角 |
| 鼠标滚轮 | 向上缩窄视野、向下扩大视野；60°–95° 平滑 FOV 缩放，UI 模式不缩放 |
| 按住左 Shift | 站立奔跑 |
| 短按左 Ctrl | 切换蹲下（低于 0.22 秒） |
| 长按左 Ctrl + 移动 | 站立慢走；松开不切换蹲下 |
| E | 拾取准星指向、距离 2.8 米内且无遮挡的道具 |
| 1–9 / 0 | 选择第 1–10 格 |
| Tab | 显示鼠标，操作快捷栏；再次按下恢复移动 |
| 左键 / 拖动 | 选中格子 / 交换两格物品（包括空格） |
| 悬停 | 鼠标右上方的半透明简介框 |
| R / 快捷栏右键 | 检视选中物品；暂停场景，显示图片与文字 |
| Esc / R / 关闭按钮 | 关闭检视，恢复进入检视前的鼠标模式 |
| 取得关键线索后走进门 | 按当前阶段连接切换 A/B/C，保留此前房间进度 |

快捷栏固定十格，不叠加物品，不含独立背包窗口。第十一件物品拒绝拾取，仍留在场景中。测试房间每阶段各有一件必需线索，全程九件，加一张可选纸条；容量边界由自动化验证。

当前 HUD 不显示教学说明、快捷键、格子数字、拾取文字通知或测试标牌；道具与传送门不再显示悬浮标签。功能性的道具介绍、检视正文、关闭按钮、倒计时、线索栏和指南针保留。满格仍会拒绝拾取，但不弹文字提示。

## 迁移到正式项目

1. 将整个 **`HorrorSystems` 文件夹**复制到目标工程的 `Plugins/HorrorSystems`。无需复制测试宿主的 `Source/Reborn`。
2. 启用插件，使用 UE 5.8.2 编译目标工程。目标工程建议为 C++ 工程；纯蓝图工程需要先添加一个 C++ 类，让 UE 建立编译目标。跨引擎版本请重新编译，勿复用旧 DLL。
3. 在关卡 World Settings 设置 GameMode Override 为 **HSGameMode**，或继承它制作正式游戏模式。它会使用 HSCharacter 和 HSPlayerController。
4. 关卡放置 **PlayerStart**；默认 `PlayerStartTag=Default`，传送落点用 `Arrival` 或自定义标签。每张关卡至少放一个 **NavMeshBoundsVolume**，覆盖可行走区域，按 P 检查绿色导航网格。
5. 放一个 **HSVisionRig** 配置视野和雾；摆放 **HSMonster**、**HSPickup**、**HSPortal** 和 **HSAmbientZone**。复制测试地图中的实例也是可行的。
6. 将原工程 Config/DefaultGame.ini 中 [/Script/HorrorSystems.HSSettings] 配置段复制到新工程，保留美术与音频绑定；设置 r.CustomDepth=3 以启用交互轮廓高亮。
7. 若需打包，Project Settings → Packaging → Additional Asset Directories to Cook 添加 `/HorrorSystems`，并把正式地图加入打包地图列表。模板角色/音效通过插件路径读取，必须包含在 Cook 内容中。

插件资源没有依赖宿主 `/Game` 的运行时路径。测试图位于 `/HorrorSystems/Maps/Basic_roomA`、`Basic_roomB`、`Basic_roomC` 和 `Basic_roomABC`。Content Browser 需要勾选 **Show Plugin Content（显示插件内容）**。

## 可配置位置

Project Settings → Game → **Reborn Horror Systems**：快走/奔跑/慢走/蹲走速度，Ctrl 判定阈值，拾取距离，AI 慢/普通/快速度和范围，停下/恢复半径，视野清晰/隐藏距离、雾浓度和手电距离。UE 距离单位为厘米、速度为厘米/秒。隐藏距离应大于清晰距离，AI 远半径应大于近半径。

Camera 分组可调默认/最小/最大 FOV、滚轮步长、平滑速度、鼠标灵敏度及站立/蹲下眼部相对高度。默认 FOV 85°、范围 60°–95°、步长 5°、灵敏度 0.65；俯仰限制 -80°～80°。摄像机直接跟随胶囊体，人物朝向与鼠标水平朝向同步，A/D 横移，S 后退。站立眼部在胶囊中心上方 68 cm，蹲下 50 cm；蹲起使用高度补偿和平滑过渡，并以球形扫描防止眼部穿过低天花板。镜头保持在角色身旁，不再使用第三人称弹簧臂。头颈骨骼在玩家网格中隐藏，四肢和身体保留，摄像机不跟随头部动画抖动。

创建新道具：右键 → Miscellaneous → Data Asset → **HSItemData**，填写名字、介绍、图标、检视图片、拾取/检视 SoundBase。然后给 HSPickup 设置 ItemData 和稳定的 **PickupId**。同一关卡中 PickupId 必须唯一；不同关卡允许相同 ID，持久键会加入关卡名。不要复制一个已配置 ID 的道具后保留相同 ID。

传送门：设置 Destination 关卡软引用、DestinationSpawnTag 和 PortalName。落点应离门触发框至少 2 米，避免落地后马上再次传送。无效目标会拒绝传送，并且不会修改快捷栏。

音效：HSCharacter.FootstepSound、ItemData.PickupSound/InspectSound、HSPortal.TravelSound、HSAmbientZone.Sound/Volume/AudibleRadius。已接入提供的录音，未提供的落地、倒塌和换画音效继续留空；正式场景可自行配置 SoundWave 或 SoundCue。环境音资源需要开启 Looping。

角色：HSCharacter 使用 UE Manny 已蒙皮的模板骨骼和 Idle/Walk/Run BlendSpace，沿用原始皮肤权重和四肢骨骼层级。蹲下在组件空间降低骨盆，双腿 Two-Bone IK 保持脚底位置与骨段长度，膝盖朝前弯曲；拾取时右臂 IK 朝实际道具位置伸出，保留手部方向。动画线程使用主线程 PreUpdate 缓存的目标。仍属于程序化测试动作，**不是正式动作捕捉动画**。生产动画蓝图可以使用角色的 `bIsCrouched`、速度、`PickupPoseAlpha`、`PickupTargetLocation`，或响应 `OnItemPickedUp` 播放自己的 Montage。PickupMontage 可配置，但替换 Animation Blueprint 时需提供对应 Slot。

## AI 逻辑

HSMonster 使用胶囊碰撞、CharacterMovement 和 NavMesh 寻路，AHSPursuitController 运行真正的 UE Behavior Tree：Sequence → Target Service（0.1 秒更新）→ Pursue Task。默认树在运行时构建，可在 Gameplay Debugger/AI Debug 中观察黑板；没有手工编辑的 BT Graph 资产。可给控制器的 ChaseTree 指定自定义行为树，继续使用原生 Task/Service。

黑板：`Target` Actor、`Distance` Float、`State` Int。状态对应 CatchUp=0、Approach=1、Waiting=2、Blocked=3。慢/普通/快三档为 160 / 300 / 540 cm/s，800–1800 cm 为普通档；在玩家屏幕内且无遮挡时提前减速。速度以 650 cm/s² 平滑变化，实际加速受 CharacterMovement 限制。距离 125 cm 停止，超过 180 cm 恢复，避免边界抖动。每 0.3 秒更新寻路请求，玩家移动会持续改变目标。路径失败则重试，不直接穿墙或瞬移。

## 视野、碰撞与暂停

HSVisionRig 的 M_DistanceFade 基于 SceneDepth 在清晰/隐藏距离间逐渐压暗，超过最大距离完全黑。指数雾与相机手电共同营造近距恐怖视野。墙体和地板仍保留碰撞、导航；限制的是渲染视野。

主角自带常亮的全方向点光源 **PlayerAuraLight**，跟随眼部位置，蹲起和转向时持续照亮周围。默认半径 450 cm、亮度 900 流明，半径限制在 ClearRadius 内，边缘自然衰减；动态阴影保留墙体遮挡。无需场景灯、无需开关或电池。Project Settings → Game → Reborn Horror Systems → Vision 中可调整 **PlayerLightRadius / PlayerLightIntensity**。原有前方光束仍用于方向补光，近距点光独立于光束方向。

检视 UI 使用原生 Slate（并非手工 Widget Blueprint）。打开检视后世界暂停，玩家移动和相机输入锁定，UI 继续接收 Esc/R。关闭只解除该检视窗口自己创建的暂停；若正式项目另有暂停管理器，可统一接入该控制器方法。

## 验证与开发

- `Tests/PolicyTests.cpp`：十格索引、Ctrl 阈值、速度优先级和 AI 速度连续性。
- UE Automation：`HorrorSystems.Inventory.CapacitySwapAndSession`。
- UE 游戏运行 Automation：`HorrorSystems.Runtime.EndToEnd`，测试真实模板角色、输入移动、蹲走/慢走、拾取、暂停、NavMesh 追踪、三关卡往返与物品保留。
- UE 游戏运行 Automation：`HorrorSystems.Runtime.RigAndCamera`，验证第一人称相机挂接/朝向、头部隐藏、四肢骨骼存在、蹲下脚底高度和腿骨长度、FOV 缩放边界、UI 模式禁止缩放、上下俯仰和眼部高度。EndToEnd 另外验证右手朝拾取目标移动、手臂骨段长度不变。
- `Tools/BuildTestContent.py`：UE Editor Python 重建测试内容。**重建会覆盖三个测试地图，不要在已改成正式关卡后运行。**
- `Tools/GenerateMedia.py`：重建占位图标、纸条图片与音效源文件，需要 Pillow。

本版会话状态跨关卡保留，关闭游戏后清空；没有 SaveGame 落盘、多玩家复制、战斗伤害或正式美术场景。

## 0.5.0 房间阶段扩展

新增 60 秒读条、玄关空气墙、超时物品/线索回滚、阶段控制门锁、三阶段循环、窗户首次运镜与演出、移动抽屉、三档怪物速度和空间音效、白光结束画面。详细正式关卡接入步骤见 [Docs/RoomSetup.md](Docs/RoomSetup.md)。默认 PickupMontage 已绑定 AS/AM_FirstPersonPickup 与原生 DefaultSlot；先完成动作再按道具设置进入检视。
