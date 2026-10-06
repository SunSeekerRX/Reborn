# 新增房间素材（0.8.2）

安全屋墙面使用“安全屋-墙.png”，其余房间墙面使用“墙.png”。四个实际游玩地图 Basic_roomA、Basic_roomB、Basic_roomC、Basic_roomABC_unchange1 均已适配；逻辑路径 Basic_roomABC 仍通过重定向进入实际地图。

房屋整块网格共用材质槽，因此用世界位置区分安全屋及房间，用表面方向保留原地面、天花和台阶。为空的材质槽也覆盖到，保持原始模型和碰撞。

替换 8 处桌子（2 处大型桌子的旧四条桌腿各自隐藏并保留于编辑器），保留原 Actor、位置与组件。煤油灯无现有占位模型，按各房间桌面新增 6 盏装饰灯，使用模型三角形射线求交定位于实际桌面；玻璃单独设置透明材质。装饰灯无碰撞，不额外修改角色近距照明。

书架使用新旧混合：B 房间的可倒塌书架和 ABC 的 Bookshelf2 使用新版本，其余 4 处保留旧版。新书架按原物体尺寸与房间内侧方向拟合，保留倒塌组件、轴心、触发记录和碰撞；仅第三关开启相关倒塌玩法。

安全屋地面、房间木地板暂未替换，因为本次要求指定墙面；货架素材只保留源文件，尚未指定替换。新素材源文件位于 ArtSource/NewRoomArt，导入资源位于 Plugins/HorrorSystems/Content/Art。

可重用脚本位于 Plugins/HorrorSystems/Tools：ImportNewRoomArt.py、ApplyNewRoomArt.py、FinishRoomWallMaterials.py、PlaceRoomLanterns.py。导入完成后依次执行应用、材质补齐和煤油灯桌面校准脚本。UE 测试入口为 Tools/VerifyNewRoomArt.ps1。
