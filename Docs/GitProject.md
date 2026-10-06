# Git 工程与本机产物

Git 仓库位于 `C:\Projects\Game Design Projects\Reborn\Reborn`。保留 Reborn.uproject、Config、Source、Plugins/HorrorSystems 的源码与 Content，以及 ArtSource、工具脚本和文档。

本次 0.8.3 的光照、桌面物品及 E 提示实现已在工程中；8 项相关引擎测试通过。完整日志与截图移至仓库外：`C:\Projects\Game Design Projects\Reborn\LocalArtifacts\Cleanup_20261006_195326`。此处同时保留旧打包、历史归档、IDE 设置和编译缓存，未删除。

.gitignore 已排除 Binaries、Intermediate、Saved、DerivedDataCache、IDE 用户设置、Deliverables、Portable、Archive、FileOpenOrder 日志与 Python 缓存。InspectionMeshSource 中的 .obj 是道具模型源文件，明确保留；它们不是编译产生的 .obj。

此前被 Git 跟踪的旧产物、归档与 IDE 文件已从工作目录移出，因此会显示为删除。提交本次修改时，应包含这些删除及更新后的 .gitignore，之后对应产物将被忽略。本次没有暂存、提交、推送或改写 Git 历史。

C++ 工程的本机编译产物已清出仓库。首次重新打开 Reborn.uproject 时，按虚幻引擎提示重新编译模块；也可在 VS 中构建 RebornEditor / Win64 / Development。Binaries 与 Intermediate 会自动重建并被 Git 忽略。

不要忽略或删除 Plugins/HorrorSystems/Content 下的 uasset、umap、Maps/_GENERATED、Characters 及 Art；这些是游戏必需资源。大资源通过现有 .gitattributes 中的 Git LFS 规则管理。源码、配置、素材和工具修改由你通过 uGit 检查后提交。
