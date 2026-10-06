# Basic 场景交互

游戏默认启动 `Basic_roomA`。`Basic_roomA/B/C` 使用独立关卡传送；`Basic_roomABC` 的三个区域使用同一关卡内的传送。A→B→C→A，C 出口推进阶段，第三阶段 C 出口结束。旧 Test 地图和旧测试工具在工程 `Archive/LegacyTestScenes` 中，不参与插件打包。

原房间白模和家具布局保留。每个房间绑定安全屋、60 秒倒计时、锁门与阶段钥匙、导航、追逐怪物、近距离窗户演出。复制的房间壳体只打开安全屋出口和窗户；原始生成网格没有修改。初始地图备份在 `Saved/Verification/BasicBeforeAdaptation`。

## 书柜

每个房间的 `Reborn_Bookshelf_*` 为 **HSCollapsingObstacle**，保留原书柜 Actor 与动画、碰撞组件，外观已替换为提供的书架模型。旁边的 `Reborn_BookshelfRecord_*` 为 **HSInspectTrigger**，准星对准并按 **E** 打开 3D 检视。检视暂停世界、倒计时和怪物。左键拖动物体旋转，滚轮缩放，双击恢复。关闭页面后书柜绕底部向通道倒下，落地后继续阻挡角色并更新动态导航。

可配置 `Obstacle`、`ItemData`、`FallPivotOffset`、`FallAxis`、`FallAngle`、`FallDuration`、`CollapseSound`。更换模型时重新设置底部旋转支点。倒塌一次后不再重复触发，重新加载关卡恢复。

## 挂画与高亮

三张挂画为 **HSSwapPainting**，同房间设置相同 `SwapGroup`。准星指向第一张、按 **E** 记住它；再指向另一张、按 **E**，两张通过错开的弧线交换位置。重复按第一张不会移动。动画期间不接受再次交换，完成后可再次选择。离开交互距离会清除待交换选择。

挂画是蓝色高亮，拾取物、书柜旁记录、可移动柜体是暖黄色。角色统一维护一个高亮目标，遵守准星方向、2.8 米距离和墙体遮挡。记住的第一张画在没有其他焦点时继续高亮；准星移到第二张或其他物体时高亮转移，避免同时高亮两个。

## 音效配置

中文引擎：**编辑 → 项目设置 → 游戏 → Reborn Horror Systems → Audio**。

- `Walk Footstep Sound`：普通、慢走及蹲行的脚步。
- `Run Footstep Sound`：Shift 跑步脚步。
- `Landing Sound`：落地声。
- `Collapse Sound`：书柜开始倒塌时播放。
- `Painting Swap Sound`：两张挂画开始交换时播放一次。

走路和跑步已绑定提供的脚步录音，怪物脚步、环境、第二阶段 BGM、压力与窗外登场音效也已接入；详见 ImportedAssets.md。未提供落地、倒塌与换画音效，三个对应槽位继续留空。书柜的 `Collapse Sound` 和挂画的 `Swap Sound` 可单独覆盖全局设置。音效使用物体世界位置，脚步按移动距离触发。

## 后续场景

系统代码和依赖资源都位于 HorrorSystems 插件。新场景设置游戏模式 **HSGameMode**，放置 PlayerStart 和上述 Actor；按 `RoomSetup.md` 配置房间规则。组合地图的 Director 需要启用 `Use Room Bounds` 并调整 `RoomBounds`；传送门启用 `Local Travel`，设置目标房间及目标出生标签。

`Tools/ConfigureBasicGameplay.py` 是四张 Basic 地图的编辑器配置脚本，供重新适配使用。它会重新生成配置演员，运行前应保存场景；首次运行的原始备份不会覆盖。
