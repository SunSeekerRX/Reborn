# Reborn UE 5.8.2 第一人称测试工程

打开 `Reborn.uproject`，默认进入 RebornTitle，点击运行后显示像素莫比乌斯环标题页，通过“开始游戏”进入 A 安全屋。

当前 0.8.3：调暗房间和玩家光照，怪物新增随身暗红光；拾取物及记录摆在现有桌面上，当前交互目标上方显示 E。说明见 `Docs/LightingInteraction.md`。

此前 0.8.2：已替换安全屋和房间墙面、8 处桌子、2 处新书架，并新增 6 盏桌面煤油灯。素材适配与配置见 `Docs/NewRoomArt.md`。

此前 0.8.1：修正游戏中静止参考人偶、家具正面朝向和铁柜横躺问题。靠近后按 E 拾取，物品先进入快捷栏；同一数字键连按两次或鼠标双击格子后检视。书架记录也先入栏，关闭该记录检视后触发对应倒塌。说明见 `Docs/InteractionArtFix.md`。标题、通关白光和最近安全区满血恢复规则保持不变，见 `Docs/TitleRecovery.md`。

核心操作：第一人称 WASD 移动，鼠标转向，滚轮平滑调整视野角度，Shift 奔跑，Ctrl 短按蹲下/长按慢走，E 拾取。1–9、0 单按选择格子，0.35 秒内连按两次检视；Tab 显示鼠标后左键双击格子也可检视，拖动可换位。R/右键检视仍兼容，检视时暂停，模型可拖动旋转、滚轮缩放。

当前 0.7.1：第一关 A→B，取得钥匙后返回 B 安全屋即进入第二关；第二关 C→B→A，取得三房间线索后返回 A 安全屋即进入第三关；第三关进入 ABC 大场景的 A 安全屋，取得最终信息后跑到地图远端白光出口，显示 REBORN 标题。第一、二关取得线索后解除空气墙，回安全屋才停表、开白光门。第三关暂保留 60 秒连续预算。完整流程及配置见 `Docs/ThreeStageFlow.md`。

主角自身带常亮近距光源，周围约 3.3 米由较暗常亮光照明，随移动和蹲起跟随；远处保留渐暗视野。亮度和范围可在系统设置的 Vision 分组调整。

当前游戏界面已移除操作说明、拾取通知、测试标牌和道具悬浮标签；保留顶部 60 秒读条和其下指南针、左上线索栏、准星、十格图标与格子编号、悬停道具介绍和检视内容。操作说明只保留在本文件和插件说明中。

所有可迁移系统位于 **Plugins/HorrorSystems**，包括 C++、UI、人物资源、道具、音效、视野材质和测试地图。迁移及配置说明详见 `Plugins/HorrorSystems/README_中文.md`。

迁移时直接复制 `Plugins/HorrorSystems` 到新工程的 Plugins 目录，按插件说明启用并编译。Git 工程仅保留源码与资源，编译缓存、旧打包文件和验证日志已移到仓库外。

本机验证记录见 `Docs/Verification.md`；截图和 UE 测试报告保留于仓库外 `../LocalArtifacts/Cleanup_20261006_195326/Saved/Verification`。Git 清理说明见 `Docs/GitProject.md`。

Windows Shipping 可运行游戏位于本文件夹内 `Deliverables/WindowsGame`，运行其中的 `Reborn.exe`，Alt+F4 退出。完整工程压缩包为 `Deliverables/Reborn_Project.zip`，游戏压缩包为 `Reborn_Windows.zip`。游戏压缩包需要整体解压，不能只拷贝 exe。若目标电脑缺少运行库，可运行游戏包内 `Engine/Extras/Redist/en-us/vc_redist.x64.exe`。

重新打包可运行 `Tools/PackageWindows.ps1`；它执行 Shipping Build/Cook/Stage/Pak/IoStore/Archive，并包含四张 Basic 关卡和插件资源。`Tools/PackageDeliverables.py` 创建工程与游戏压缩包，排除编译缓存、旧 DLL、调试符号与临时文件。

本项目使用本机 UE 5.8.2 与 Visual Studio 2022 C++ 工具链。第一轮启动可能编译着色器。角色使用模板蒙皮与双腿/拾取右臂 IK 测试姿势，已接入提供的家具、屠夫模型和六类录音，未匹配家具保持白模。

0.5.0：首次进入每个房间自动播放窗户演出。玄关内不计时，完全走出后开始倒计时并禁止返回。超时返回当前房间玄关，撤销本次房间获得的物品与线索，重置时间；此前房间的物品保留。正式房间、阶段路由、关键线索、运镜、拾取动画与音效配置详见 `Plugins/HorrorSystems/Docs/RoomSetup.md`。

统一交付目录：工程根目录为 Reborn；当前游戏、压缩包与验证报告位于 Deliverables；旧版本存放于 Archive。
