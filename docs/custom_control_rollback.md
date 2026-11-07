<!-- /** @file custom_control_rollback.md @brief 可编程控制功能的基线与回退说明。 */ -->

# 可编程控制回退基线

实施前分支：`main-custom-control`。`main` 与该分支均为 `f430f16d75e4058485fc7bd27cdfd9124654fd72`。

实施前已有改动备份在 `backup/custom-control-baseline/`：两份设计文档原件、`working.patch`、`staged.patch`。补丁以二进制形式保存，包含 dependency.zip 原有暂存及工作区差异。实现期间不移动 main、不改写历史、不自动提交。

另已导出 `backup/custom-control-baseline/main-f430f16d.zip`（33,307,450 字节），包含上述 main 提交的全部受跟踪文件。可解压至独立目录检查原始源码，不会携带本次未提交修改；该源码包不含被 Git 忽略的依赖目录，原有依赖未被本次新增控制依赖替换。原依赖仍在项目 `dependency/` 内。

归档使用 Windows CRLF 换行。归一化换行后，已核对其中的 CMakeLists.txt、世界场景实现和原控制接口，其 Git blob 摘要均与 main 一致。

需要回退时，先保留当前工作（含未跟踪源码）。可在仓库外用 `git archive main` 导出基线运行，或在保留当前修改后切换 main。未提交修改会跨分支保留，仅切换分支不会自动撤销它们；不要直接执行 reset --hard 或 clean。备份目录被忽略，须在清理仓库前另存。main 指针及上述提交对象未改动即可恢复原始源码。
