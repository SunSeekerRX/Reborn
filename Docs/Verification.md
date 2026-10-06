# 当前：全流程实走复测（2026-10-07）

本轮 15 项通过、0 失败。从标题用真实移动和输入走完 A→B→A→C→B→A→ABC→结局→重新开始，并修正纸条选择、白光入口、挂画中心、隐藏怪物下落、C 出口追逐和演出可视性。详细策划案核对、修复及测试边界见 [FullPlaytest.md](FullPlaytest.md)。最新报告位于工程外 `../LocalArtifacts/FullPlaytest_20261007/FinalPass`。

下列 10 项为此前的验证记录，不能替代本轮新增的实际通路、桌面拾取和真实追逐验证。

## 历史：Game Jam 策划案第 9–11 部分验证

当前版本 10 项 UE 测试通过，0 失败。报告位于工程外 `../LocalArtifacts/GameJam_20261007/AcceptanceStory`、`AcceptanceCore`、`AcceptanceWindow`。完整三阶段在实际渲染客户端中重复验证；此前失败的门扇挤压、测试出生高度、书架通路和测试之间的地图初始化问题均已修正并重新通过。

- FullStory：A→B→A→C→B→A→ABC→标题，剧情节点和阶段一致；安全屋停表、木门开关及锁定、线索解锁、白光切换、B 阅读后演出、死亡恢复保留线索、ABC 最近已访问安全屋、三档速度、灯光重入和挂画变化。
- FirstRoomPath：真实 CharacterMovement 打开木门，从 A 安全屋走到主房间；木门向远离玩家的一侧开启，身体完全通过后关门。
- FinalEscape：倒塌后通过碰撞扫描生成 119 个路径点，再实际行走至远端出口并返回标题。此项关闭怪物伤害，单独验证家具、门框、台阶和走廊碰撞。
- RoomAMovement、PickupDoubleNumber、Inspection3D：移动、跑步、慢走、蹲下、视角、E 拾取、数字双击、3D 检视旋转缩放、暂停及关闭。
- CapacitySwapAndSession、TimerRollbackAndStages：十格容量、交换、会话与超时回滚。
- SceneOnePursuit：实际第二阶段 A 怪物自主追到静止玩家，物理接触扣一条命并进入僵直。
- WindowCamera：B 主房间读完线索后的运镜全程逐帧扫碰撞，到达实际窗洞，窗外怪物可见，演出结束恢复玩家视角。

RebornEditor Development 与 Reborn Win64 Shipping 编译成功。Windows 资源烘焙加载了四张玩法地图和标题，1094 个包完成、0 错误；旧 Imported 音乐有一条 96 kHz 采样率性能提示，当前游戏使用新导入的 48 kHz Jam 音乐。空白画布已补齐 Nanite 使用标记。

595 个 UE 资源及关卡文件的包头均有效，27 个 Jam 音频及 24 个剧情资源齐全，地图和新增资产使用 Git LFS。工具脚本语法检查及 `git diff --check` 通过。测试使用引擎输入事件与实际运行世界；未覆盖所有操作系统鼠标动作、硬件和任意玩家路线，不能据此保证不存在任何未知问题。

可运行 `Tools/VerifyGameJam.ps1 -EngineRoot <UE5.8目录> -Visual` 复测。报告默认输出到仓库外，脚本检查测试报告的实际结果，不只检查进程退出码。

## 历史：光照及桌面交互验证（0.8.3，2026-10-06）

8 项 UE 引擎测试通过，0 失败：LightingTableInteraction、FurnitureFacing、Basic.Interactions、RoomAMovement、FinalEscape、PickupDoubleNumber、Inspection3D、SafeWhiteTravel。报告、实际渲染截图和配置保留于仓库外 ../LocalArtifacts/Cleanup_20261006_195326/Deliverables/Verification/LightingInteraction。

四场景 14 个拾取物/记录已定位于现有桌面。逐物体模型底面距射线求得的实际桌面 0.3 cm，保持 Actor、组件、阶段、线索与倒塌关联。引擎运行时检查了桌面下方碰撞，验证当前阶段钥匙可被准星选中，提示位置按实际网格顶面计算；真实输入模拟将 E 按下、等待输入处理后释放，确认物品入栏并移除提示。

检查玩家较暗点光和聚光强度，以及怪物红光的颜色、强度及根组件绑定。实际渲染确认较暗桌面可辨认、E 显示在物品上方、怪物周围出现红光；已隐藏禁用的旧出口光块。

原有交互、3D 检视、挂画交换、新书架检视关闭后倒塌、三处书架倒塌后完整逃离、WASD/Shift/Ctrl 移动和五次安全屋传送继续通过。逃离通路测试关闭怪物伤害，专门验证家具碰撞。实际 Slate 控件事件测试未模拟操作系统物理鼠标点击。临时拾取测试对象赋值前的 ItemData 警告保留在汇总。

Development 编译、Windows Shipping 打包与打包程序 30 秒启动响应检查通过。此前版本记录保留于同一仓库外目录的 Deliverables/Verification/NewRoomArt、InteractionArt、TitleRecovery。
