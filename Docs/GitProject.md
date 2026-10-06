# Git 工程与本机产物

Git 仓库位于 `C:\Projects\Game Design Projects\Reborn\Reborn`。保留 Reborn.uproject、Config、Source、Plugins/HorrorSystems 的源码与 Content，以及 ArtSource、工具脚本和文档。

已按 Game Jam 策划案第 9–11 部分实现并从标题实走完整三阶段，15 项引擎测试通过。最新修复、备份、编译缓存、截图和报告位于仓库外 `C:\Projects\Game Design Projects\Reborn\LocalArtifacts\FullPlaytest_20261007`。详细核对见 `Docs/FullPlaytest.md`。音频原件和此前实现报告仍保留在 `LocalArtifacts/GameJam_20261007`，历史产物在 `LocalArtifacts/Cleanup_20261006_195326`。

.gitignore 已排除 Binaries、Intermediate、Saved、DerivedDataCache、IDE 用户设置、Deliverables、Portable、Archive、FileOpenOrder 日志与 Python 缓存。InspectionMeshSource 中的 .obj 是道具模型源文件，明确保留；它们不是编译产生的 .obj。

本次没有暂存、提交、推送或改写 Git 历史。通过 uGit 提交时需包含全部修改，以及新增的 `Content/Audio/Jam`、`Content/Materials/Jam`、`Content/Story`、HSStoryActors、HSParticleTitle 和相关工具、测试、文档。四张玩法地图均已保存，B 场景的实际文件是 `Plugins/HorrorSystems/Content/Maps/Basic_roomB.umap`，第三阶段实际地图为 `Basic_roomABC_unchange1.umap`。`Metrial_test` 未修改。

C++ 工程的本机编译产物已清出仓库。首次重新打开 Reborn.uproject 时，按虚幻引擎提示重新编译模块；也可在 VS 中构建 RebornEditor / Win64 / Development。Binaries 与 Intermediate 会自动重建并被 Git 忽略。

不要忽略或删除 Plugins/HorrorSystems/Content 下的 uasset、umap、Maps/_GENERATED、Characters 及 Art；这些是游戏必需资源。大资源通过现有 .gitattributes 中的 Git LFS 规则管理。源码、配置、素材和工具修改由你通过 uGit 检查后提交。
