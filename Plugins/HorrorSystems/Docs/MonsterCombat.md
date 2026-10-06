# 场景1怪物与生命

当前场景 `/HorrorSystems/Maps/Basic_roomA` 已放置 `SceneOne_PursuingMonster` 和导航边界。安全屋内停止追击，离开后实时追踪玩家，按距离及可见性自动切换慢速160、中速300、快速540厘米/秒。

玩家三条命跨关卡保留，底部十格物品栏下方细红条分别显示全部、三分之二、三分之一。怪物胶囊接触玩家扣一条命，双方轻微弹开；怪物进入5秒低速恢复，速度30厘米/秒。玩家至少5.25秒内免疫重复命中，怪物必须恢复且与玩家分离后才能再次攻击。安全屋、检视及运镜期间不扣命。

三条命耗尽后停止计时及追击、锁定移动并显示重新开始按钮。重新开始重载当前关卡并恢复生命与游戏进度。房间超时仍采用原有回安全屋逻辑，不恢复损失的生命。

迁移到新地图：使用HSGameMode，放置HSMonster并勾选“Can Damage Player”，添加覆盖可行走区域的Nav Mesh Bounds Volume。保证地面碰撞和导航可达。演出用怪物保留Cinematic Actor，不参与伤害。Stagger Duration及Stagger Speed可在详情面板调整。

原场景的静态展示人偶保留外观，设为不阻挡Pawn，避免其未导入导航的骨骼碰撞卡住追逐角色。

验证：`HorrorSystems.Combat.SceneOneContact` 在Basic_roomA运行，覆盖导航路径、真实碰撞扣命、安全保护、弹开、重复接触、5秒恢复、三档速度、失败冻结及生命重置。`HorrorSystems.Combat.SceneOnePursuit` 验证怪物自主追逐、实际接触和扣命。移动、安全屋与跨地图流程回归通过；实际渲染确认细红血条位于十格物品栏下。
