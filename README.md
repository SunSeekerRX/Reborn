# Reborn UE 5.8.2 第一人称测试工程

打开 `Reborn.uproject`，进入 Basic_roomA，点击 Play。

核心操作：第一人称 WASD 移动，鼠标转向，滚轮平滑调整视野角度，Shift 奔跑，Ctrl 短按蹲下/长按慢走，E 拾取，R 暂停检视，Tab 操作十格快捷栏，1–0 选择。取得当前阶段关键线索后走进门，按 A→B→C→A 循环；第三阶段 C 出口完成游戏。

主角自身带常亮近距光源，周围约 4.5 米持续可见，随移动和蹲起跟随；远处保留渐暗视野。亮度和范围可在系统设置的 Vision 分组调整。

当前游戏界面已移除操作说明、快捷键文字、拾取通知、测试标牌和道具悬浮标签；保留顶部 60 秒读条和其下指南针、左上线索栏、准星、十格图标、悬停道具介绍和检视内容。操作说明只保留在本文件和插件说明中。

所有可迁移系统位于 **Plugins/HorrorSystems**，包括 C++、UI、人物资源、道具、音效、视野材质和测试地图。迁移及配置说明详见 `Plugins/HorrorSystems/README_中文.md`。

已提供 `Portable/HorrorSystems_Source.zip`：解压后的 HorrorSystems 文件夹直接放到正式工程的 Plugins 中，再按照迁移说明启用并编译。压缩包不含旧 DLL 和编译缓存。

本机验证记录见 `Docs/Verification.md`；实际运行截图与 UE 测试报告位于 `Saved/Verification`。

Windows Shipping 可运行游戏位于本文件夹内 `Deliverables/WindowsGame`，运行其中的 `Reborn.exe`，Alt+F4 退出。完整工程压缩包为 `Deliverables/Reborn_Project.zip`，游戏压缩包为 `Reborn_Windows.zip`。游戏压缩包需要整体解压，不能只拷贝 exe。若目标电脑缺少运行库，可运行游戏包内 `Engine/Extras/Redist/en-us/vc_redist.x64.exe`。

重新打包可运行 `Tools/PackageWindows.ps1`；它执行 Shipping Build/Cook/Stage/Pak/IoStore/Archive，并包含四张 Basic 关卡和插件资源。`Tools/PackageDeliverables.py` 创建工程与游戏压缩包，排除编译缓存、旧 DLL、调试符号与临时文件。

本项目使用本机 UE 5.8.2 与 Visual Studio 2022 C++ 工具链。第一轮启动可能编译着色器。角色使用模板蒙皮与双腿/拾取右臂 IK 测试姿势，已接入提供的家具、屠夫模型和六类录音，未匹配家具保持白模。

0.5.0：首次进入每个房间自动播放窗户演出。玄关内不计时，完全走出后开始倒计时并禁止返回。超时返回当前房间玄关，撤销本次房间获得的物品与线索，重置时间；此前房间的物品保留。正式房间、阶段路由、关键线索、运镜、拾取动画与音效配置详见 `Plugins/HorrorSystems/Docs/RoomSetup.md`。

统一交付目录：工程根目录为 Reborn；当前游戏、压缩包与验证报告位于 Deliverables；旧版本存放于 Archive。
