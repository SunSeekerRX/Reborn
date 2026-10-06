# 拾取、检视与家具朝向验证（0.8.1，2026-10-06）

7 项相关引擎测试通过，0 失败：PickupDoubleNumber、FurnitureFacing、RoomAMovement、FinalEscape、Basic.Interactions、Inspection3D、SafeWhiteTravel。完整报告、配置和实际渲染截图见 Deliverables/Verification/InteractionArt。

确认靠近不自动拾取，实际 E 输入把物品放入快捷栏；拾取动画结束后不自动检视。单按数字键只选择，连续两次打开正确物品；长按、不同格子、空格子及超时双按无误触。还通过实际 Slate 格子和覆盖层控件的事件回调验证了鼠标左键双击、单击选择及鼠标模式下的数字双按。这部分未模拟操作系统物理鼠标点击。

四个游戏场景内 4 个参考人偶均设为仅编辑器、游戏隐藏且无碰撞；62 处命名家具适配正面与尺寸，保留原 Actor 及组件。实际渲染确认书架开口、抽屉和铁柜门朝向房间内。

书架旁记录先收入快捷栏，再双按数字检视。多张记录同时收入后，分别检视只触发各自书架；检视期间暂停时间，关闭后开始倒塌。更新后的完整逃离测试将三处书架全部倒下后，通过真实角色移动和 Shift 输入沿 98 个路径点跑至最终出口并返回标题。此通路测试关闭怪物伤害以隔离家具碰撞。

检视模型旋转、缩放及暂停恢复，WASD/Shift/Ctrl 移动，挂画选择与交换，以及五次跨场景安全屋传送回归通过。Development 编译和 Windows Shipping 打包通过；打包程序启动响应检查通过。警告保留于汇总：临时拾取测试对象在赋值前会输出一次缺少 ItemData 的提示，该提示属于测试构造过程。

此前 0.8.0 的标题和三条生命恢复验证保留于 Deliverables/Verification/TitleRecovery。
