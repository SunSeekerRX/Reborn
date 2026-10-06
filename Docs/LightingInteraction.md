# 光照与桌面交互（0.8.3）

四个游玩场景均使用更暗的环境光：天空光 0.03，方向光最高 0.08；窗外原灯和补光分别为 75/35 lm，并改为暗红色。保留白光传送门的强光，隐藏已禁用的旧测试出口发光模型。

玩家近距常亮光由 900 lm 降为 180 lm，半径由 450 cm 降为 330 cm；聚光灯强度降为 180，半径缩为 800 cm。怪物新增随身体移动的红光，65 lm、半径 280 cm，颜色 (1,0.025,0.01)。窗口演出角色也继承同一红光组件。

可在 项目设置→插件→Horror Systems 的 Vision 设置中调整 PlayerLightIntensity、PlayerLightRadius、FlashlightIntensity、FlashlightRange、MonsterRedLightIntensity、MonsterRedLightRadius；对应默认值保存在 Config/DefaultGame.ini。

四场景所有拾取物和可检视记录移动到现有桌子上，保持 Actor、组件、线索 ID、拾取 ID、阶段和倒塌绑定。使用网格三角形求交定位到实际桌面，模型底面距桌面 0.3 cm；过大的模型按尺寸缩小。各阶段互斥钥匙共用相同摆放位置，记录与普通便笺放在不同位置。

近处准星指向可交互物时，只在当前目标上方显示一个 E 按键提示。提示位置以实际网格顶面为准，投影到屏幕并适配界面缩放。检视、过场、标题、鼠标快捷栏模式及目标消失或移出范围时隐藏。精确视线检测优先于宽球扫描，避免桌面抢先挡住薄小钥匙；原墙面遮挡规则保持。

场景适配脚本：Plugins/HorrorSystems/Tools/ConfigureLightingInteraction.py。完整逐物体位置、桌面高度和光照前后参数保存于 Saved/Verification/LightingInteractionPlacement.json。
