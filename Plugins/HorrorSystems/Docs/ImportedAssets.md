# 已接入的美术和音频

原始素材从 Reborn 外层目录的压缩包解压到工程 `ArtSource`，原压缩包保持不动。运行资源位于插件 `/HorrorSystems/Art` 和 `/HorrorSystems/Audio/Imported`，迁移到新工程时复制完整插件，并复制 DefaultGame.ini 中 HSSettings 的配置段。

## 家具替换

| 原场景名称关键词 | 已使用模型 |
| --- | --- |
| Bookshelf / Bookcase / 书架 / 书柜 | 书架 |
| Drawer / Cabinet / 抽屉 | 抽屉柜 |
| Iron Cabinet / Locker / 铁柜 | 铁柜 |
| Sofa / Couch / 沙发 | 沙发 |

替换只修改现有 StaticMeshComponent 的网格和材质，原 Actor、组件名称、位置、缩放和交互引用保留。导入模型按照原白模的占地和高度生成适配网格，避免重新排布家具；碰撞使用网格几何。没有匹配模型的桌子、椅子、货架、挂画等保持原样。

Basic_roomA 替换 7 处，Basic_roomB 13 处，Basic_roomC 12 处，Basic_roomABC 36 处。倒塌书架和可交互柜体同样沿用原有逻辑。

## 怪物

提供的 butcher 模型原本没有可用于当前动画的骨架。已按照现有 Manny 骨架绑定四肢与躯干权重，导入为 SKM_Butcher，并使用原来的动画实例与移动动画。追逐、三档速度、扣命、弹开、五秒僵直和窗外演出仍由原怪物 Actor 控制。绑定源文件在 ArtSource/Butcher/ButcherRig.blend。

## 音频

| 提供的录音 | 游戏用途 |
| --- | --- |
| 脚步.wav | 玩家走路、慢走、蹲行；跑步使用同源脚步并调整节奏 |
| 怪脚 .wav | 怪物移动脚步，随距离衰减 |
| 环境背景音_缩混.wav | 各房间循环环境底声 |
| 第二阶段低压力恐怖氛围音乐.wav | 第二阶段起循环场景音乐 |
| 主角压力状态缩混.wav | 离怪物较近时渐入的压力声 |
| 第一阶段怪物登场缩混.wav | 首次窗外怪物演出 |

脚步录音原本包含连续多步。保留完整录音，同时截取单步片段供按行走距离触发，避免每一步叠加一整段录音。截取区间记录在 ArtSource/Audio/Derived/FootstepDerivation.json。暂停检视时世界和游戏音频暂停。

没有提供落地、书架倒塌、挂画交换音效，这三个设置继续留空。中文引擎通过 **编辑 → 项目设置 → 游戏 → Reborn Horror Systems → Audio / Art** 调整音频和怪物模型；场景音频 Actor 为 Reborn_SceneAudio。

## 重建工具

这些脚本供开发时重建，正常打开工程不需要执行：

1. PrepareArtAssets.py 在 UE Python 中导入原素材；首次导出 Manny 参考 FBX 需启用 AllowCommandletRendering。
2. RigButcher.py 在 Blender 后台绑定并导出 SKM_Butcher.fbx。
3. ApplyArtAssets.py 在 UE Python 中导入骨骼模型、配置材质，并按名称更新四个 Basic 场景。

修改场景前，初次白模和美术替换前的备份保存在 Saved/Verification/BasicBeforeAdaptation、BasicBeforeArt。旧 Test 关卡已归档在 Archive/LegacyTestScenes，不再作为游戏入口或打包地图。
