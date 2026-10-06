# Reborn UE 5.8.2 第一人称测试工程

最新完整实走与策划案第 9–11 部分核对：15 项 UE 测试通过、0 失败，详见 [FullPlaytest.md](Docs/FullPlaytest.md)。本轮报告、截图、备份和编译缓存在工程外 `../LocalArtifacts/FullPlaytest_20261007`。

当前版本按《破笼归途》策划案 v0.1 第 9–11 部分及补充要求实现。第一阶段 A→B→A，第二阶段 A→C→B→A，第三阶段连通 ABC→最终出口→标题。安全屋木门替代空气墙，剧情照片、遗言和信息替代测试钥匙；三次受伤后通过血红白光满血返回最近安全位置并保留进度。详细说明及当前复测入口见 [GameJamFlow.md](Plugins/HorrorSystems/Docs/GameJamFlow.md)。

Git 工程只保留源码、配置和 UE 资源。本次不生成压缩包、不提交或推送；原始音频、备份、编译缓存与测试报告置于工程外 `../LocalArtifacts/GameJam_20261007`。提交时请包含新增的 `Audio/Jam`、`Materials/Jam`、`Story` 目录及修改的四张玩法地图，并通过 Git LFS 上传资源。

打开 `Reborn.uproject`，默认进入 RebornTitle，点击运行后显示粒子点阵莫比乌斯环标题页，通过“开始游戏”进入 A 安全屋。

历史版本 0.8.3：调暗房间和玩家光照，怪物新增随身暗红光；拾取物及记录摆在现有桌面上，当前交互目标上方显示 E。说明见 `Docs/LightingInteraction.md`。

此前 0.8.2：已替换安全屋和房间墙面、8 处桌子、2 处新书架，并新增 6 盏桌面煤油灯。素材适配与配置见 `Docs/NewRoomArt.md`。

此前 0.8.1：修正游戏中静止参考人偶、家具正面朝向和铁柜横躺问题。靠近后按 E 拾取，物品先进入快捷栏；同一数字键连按两次或鼠标双击格子后检视。当前第三阶段由最后信息触发 A 书架倒塌。说明见 `Docs/InteractionArtFix.md`。标题、通关白光和最近安全区满血恢复规则保持不变，见 `Docs/TitleRecovery.md`。

核心操作：第一人称 WASD 移动，鼠标转向，滚轮平滑调整视野角度，Shift 奔跑，Ctrl 短按蹲下/长按慢走，E 拾取。1–9、0 单按选择格子，0.35 秒内连按两次检视；Tab 显示鼠标后左键双击格子也可检视，拖动可换位。R/右键检视仍兼容，检视时暂停，模型可拖动旋转、滚轮缩放。

主角自身带常亮近距光源，周围约 3.3 米由较暗常亮光照明，随移动和蹲起跟随；远处保留渐暗视野。亮度和范围可在系统设置的 Vision 分组调整。

当前游戏界面已移除操作说明、拾取通知、测试标牌和道具悬浮标签；保留顶部 60 秒读条和其下指南针、左上线索栏、准星、十格图标与格子编号、悬停道具介绍和检视内容。操作说明只保留在本文件和插件说明中。

所有可迁移系统位于 **Plugins/HorrorSystems**，包括 C++、UI、人物资源、道具、音效、视野材质和测试地图。迁移及配置说明详见 `Plugins/HorrorSystems/README_中文.md`。

迁移时直接复制 `Plugins/HorrorSystems` 到新工程的 Plugins 目录，按插件说明启用并编译。Git 工程仅保留源码与资源，编译缓存、旧打包文件和验证日志已移到仓库外。

本机验证记录见 `Docs/Verification.md`；本轮报告和截图保留于仓库外 `../LocalArtifacts/GameJam_20261007`。Git 清理说明见 `Docs/GitProject.md`。


重新打包可运行 `Tools/PackageWindows.ps1`；它执行 Shipping Build/Cook/Stage/Pak/IoStore/Archive，并包含四张 Basic 关卡和插件资源。`Tools/PackageDeliverables.py` 创建工程与游戏压缩包，排除编译缓存、旧 DLL、调试符号与临时文件。

本项目使用本机 UE 5.8.2 与 Visual Studio 2022 C++ 工具链。第一轮启动可能编译着色器。角色使用模板蒙皮与双腿/拾取右臂 IK 测试姿势，已接入提供的家具、屠夫模型和六类录音，未匹配家具保持白模。
