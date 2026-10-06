# 光照及桌面交互验证（0.8.3，2026-10-06）

8 项 UE 引擎测试通过，0 失败：LightingTableInteraction、FurnitureFacing、Basic.Interactions、RoomAMovement、FinalEscape、PickupDoubleNumber、Inspection3D、SafeWhiteTravel。报告、实际渲染截图和配置保留于仓库外 ../LocalArtifacts/Cleanup_20261006_195326/Deliverables/Verification/LightingInteraction。

四场景 14 个拾取物/记录已定位于现有桌面。逐物体模型底面距射线求得的实际桌面 0.3 cm，保持 Actor、组件、阶段、线索与倒塌关联。引擎运行时检查了桌面下方碰撞，验证当前阶段钥匙可被准星选中，提示位置按实际网格顶面计算；真实输入模拟将 E 按下、等待输入处理后释放，确认物品入栏并移除提示。

检查玩家较暗点光和聚光强度，以及怪物红光的颜色、强度及根组件绑定。实际渲染确认较暗桌面可辨认、E 显示在物品上方、怪物周围出现红光；已隐藏禁用的旧出口光块。

原有交互、3D 检视、挂画交换、新书架检视关闭后倒塌、三处书架倒塌后完整逃离、WASD/Shift/Ctrl 移动和五次安全屋传送继续通过。逃离通路测试关闭怪物伤害，专门验证家具碰撞。实际 Slate 控件事件测试未模拟操作系统物理鼠标点击。临时拾取测试对象赋值前的 ItemData 警告保留在汇总。

Development 编译、Windows Shipping 打包与打包程序 30 秒启动响应检查通过。此前版本记录保留于同一仓库外目录的 Deliverables/Verification/NewRoomArt、InteractionArt、TitleRecovery。
