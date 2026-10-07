# Game Jam v0.1 第 9–11 部分适配

本文件描述当前实现；旧文档中的测试钥匙、空气墙和路线已被替换。正式玩法使用 A、B、C、ABC 四张地图；`RebornTitle` 为标题。其他艺术测试和变体地图保留原状。

| StoryStep | 阶段 | 地图 | 当前目标与后续 |
|---|---|---|---|
| 0 | 1 | Basic_roomA | Photo_A：照片“快逃！入口就是出口！”，返回安全屋，前往 B |
| 1 | 1 | Basic_roomB | Warning_B：房间 3 信息与怪物预警；检视关闭后窗户演出、首次追逐；返回安全屋，前往 A |
| 2 | 2 | Basic_roomA | 取得桌上 Introduction_A 提示（计入 Clue_A），返回安全屋，前往 C |
| 3 | 2 | Basic_roomC | Testament_C：遗言与第三扇门线索，开启全局线索栏；前往 B |
| 4 | 2 | Basic_roomB | 取得 Clue_B 并集齐 A/C/B 三份线索；返回安全屋，直接传送到 ABC 的 A 安全屋，进入第三阶段 |
| 6 | 3 | Basic_roomABC_unchange1 | A 获取 FinalMessage，沿途所有木门解锁；A→B→C→最终出口 |
| 7 | 完成 | RebornTitle | 白光→黑屏→标题；开始游戏重置新一轮 |

## 木门与安全屋

`AHSStoryDirector` 管理剧情、安全屋和计时。旧 `ReturnBarrier` 永远关闭碰撞，实际阻挡为 `AHSWoodDoor.WoodDoorCollision`。木门绕真实门轴旋转，并向远离玩家的一侧打开；身体完全通过门扇后立即锁门、关闭并播放音效，关闭过程不挤压玩家。获得对应线索后才能打开木门返回。回到安全屋后停表并启用白光入口，关闭的木门遮住白光。

单房间地图通过 `DA_JamRoomA/B/C` 按阶段和剧情节点选择路由。ABC 在同一个世界中使用三个区域管理器及各自安全出生点，独立暂停或启动 60 秒计时。出生高度通过实际地面三角面测量配置。

超时回滚进入当前房间时的背包、线索和剧情触发快照，重置时间后重新进入当前地图。三次受伤耗尽生命后，血红白光遮盖画面，恢复满血并返回最近安全位置；阶段、物品及倒塌状态保留，安全屋木门允许重新打开。

## 章节演出

- 第一阶段 A 取得照片并关闭检视后开始追逐；B 检视信息后播放窗户演出，结束后开始追逐。
- A 两张画首次空白，后续进入恢复内容；第二阶段墙面出现血字，大桌移动，角色以字幕反馈。
- 吊灯判定水平 5 米范围：首次进入闪两次，之后重新进入闪一次；停留和离开不触发，边界有 20 厘米迟滞。
- 第三阶段 A 的最终信息触发书架倒塌，2.5 秒后怪物出现。后续所有木门只需要这一份信息，不再搜索。
- B 普通灯熄灭，应急灯引导窄道；两侧书架留出 2 米通路及转弯空间。
- C 放大桌子并保留两侧绕行；玩家接近出口区域后，怪物从出口前出现，可绕桌引开。
- 第二阶段怪物速度较慢；第三阶段远处快速追近、视野范围内正常接近、近处慢速。接触后双方弹开，怪物极慢移动 5 秒。安全屋内停止追击及追逐音乐。
- 白色字幕居中放在物品栏上方。标题字样和更大的莫比乌斯环采用动态粒子点阵，播放追逐 BGM。

## 素材与配置

声音位于 `/HorrorSystems/Audio/Jam`。误命名为 WAV 的 MP4/MP3 先转换为兼容 PCM；原文件保留在工程外。脚步另提取单步片段，避免每步叠加整段录音。音效集中在 `Config/DefaultGame.ini` 的 `HorrorSystems.HSSettings`，门、书架和挂画 Actor 也可单独覆盖。

`Tools/ImportJamAudio.py` 需要 `REBORN_JAM_AUDIO_ROOT`、`REBORN_FFMPEG` 和可选 `REBORN_VERIFICATION_DIR`。`Tools/ConfigureJamStory.py` 为当前作者地图绑定玩法；正常游戏无需重复运行，所有地图与资源已保存。

## 当前复测入口

- `Jam.PhysicalWalkthrough`：从标题实际步行到每份线索，E 拾取、数字双击检视，实际通过木门和白光，走完整结局并重开。
- `Jam.FinalChaseNavigation`、`Jam.FinalTableLure`：A/B/C 真实导航追逐及 C 绕桌引开出口怪物。
- `Jam.PaintingInteraction`：原点偏移的真实挂画可选择并交换可见位置。
- `Jam.TimeoutRecovery`：实际超时换图、回安全屋、清除本房间物品及恢复操控。

以上测试名称均加 `HorrorSystems.` 前缀。详细本轮 15 项核对和修复见工程的 `Docs/FullPlaytest.md`。

- `HorrorSystems.Jam.FullStory`：实际加载完整三阶段，验证门、线索、检视暂停、演出、速度状态、死亡恢复、灯光重入和回标题。
- `HorrorSystems.Progression.FinalEscape`：碰撞扫描寻找路线，再用真实 CharacterMovement 从最终信息走到远端出口。
- `HorrorSystems.Progression.FirstRoomPath`：打开木门后步行离开 A 安全屋。
- `HorrorSystems.Basic.RoomAMovement`：行走、跑步、慢走、蹲下和视角。
- `HorrorSystems.UI.PickupDoubleNumber`、`HorrorSystems.UI.Inspection3D`：E 拾取、数字双击、3D 旋转缩放、暂停与关闭。
- `HorrorSystems.Basic.WindowCamera`：在 B 检查运镜碰撞及窗外怪物可视性。

日志、报告和截图保存在工程外。旧测试钥匙与旧路线的历史测试不作为当前剧情验收入口。

一键复测：从工程目录运行 `Tools/VerifyGameJam.ps1 -EngineRoot <UE5.8目录> -Visual`。脚本按独立场景启动测试，并检查报告中的失败数量；报告默认保存到工程外。
