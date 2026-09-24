# OS Labs

各实验使用独立源码目录，目录内容来自课程对应分支，不以前一个实验的完成版本为起点。

| 目录 | 内容 | 课程分支 | 课程初始标签 | 本仓库基线标签 | 状态 |
| --- | --- | --- | --- | --- | --- |
| `lab1/` | Unix 实用程序与启动调试 | `util` | `util-26-fall` | `lab1-base` | 已完成 |
| `lab2/` | 进程与系统调用 | `syscall` | `syscall-26-fall-v2` | `lab2-v2-base` | 已完成（含附加题） |
| `lab3/` | 锁 | `lock` | `lock-26-fall` | `lab3-base` | 初始代码 |
| `lab4/` | 页表 | `pgtbl` | `pgtbl-26-fall` | `lab4-base` | 初始代码 |

## 开发与运行

先配置课程要求的 RISC-V 工具链、QEMU、GDB 和 Python，再进入对应目录：

```sh
cd lab1
make qemu
```

在该目录运行 `make grade` 执行当前实验的课程测试。Lab1 已通过 9 项测试，得分 60/60；Lab2 必做任务为 100/100，`make grade-extra` 附加题为 20/20。Lab3 和 Lab4 仍为初始代码。

调试 Lab1 时，在一个终端运行 `make qemu-gdb CPUS=1`，另一个终端在同目录运行 `make gdb`，随后输入 `source commands.gdb`。

每个目录使用自己的构建产物与磁盘镜像。默认 GDB 端口可能相同，如需同时调试多个实验，应分别指定不同的 `GDBPORT`。

## 保存进度和导出补丁

所有目录归根目录的同一个 Git 仓库管理。可按实验创建功能分支，例如 `git switch -c work/lab2`；编译时进入对应目录即可，不需要切到课程的原始分支。

在实验目录中提交修改，再导出补丁：

```sh
cd lab1
git add Makefile commands.gdb user/sleep.c user/pingpong.c user/find.c
git commit -m "Update Lab1"
make diff
```

各目录的 `make diff` 仅导出当前实验相对其独立基线的已提交修改；Lab2 使用 `lab2-v2-base`。补丁内部路径为 `user/...`、`kernel/...` 等，不带 `labN/` 前缀，可应用到课程对应初始代码。

`labN-base` 是本仓库导入快照的标签，不是课程仓库原标签的重命名。Lab2 更新课程基线后另设 `lab2-v2-base`，旧 `lab2-base` 仍保留。克隆时需获取这些标签，已有浅克隆可执行 `git fetch origin --tags`。

补丁导出后应在课程初始代码上执行 `git apply --check`、`git apply` 并重新测试。`commit.patch` 是本地产物，不纳入 Git。课程旧版的 `handin`、`tarball` 等目标依赖原仓库布局，本仓库使用指导书要求的 `make diff` 提交方式。

## 公开内容范围

版本库仅保存实验代码、必要构建文件、原始许可证及经过检查的公共说明。根目录采用白名单，并默认忽略报告、课件、模板、开发与部署文档、个人环境脚本、日志、截图、补丁、凭据和构建产物；这些材料应保存在本地忽略目录。

新增公开文档时，应先检查内容，再有针对性地调整白名单。不要用 `git add -f` 强行加入个人资料。`.gitignore` 不会检查已有源码中的内容，也不会自动清除已提交的敏感信息；提交前仍需查看 `git diff --cached`。

## 来源

- [课程指导书](https://os-labs.p.cs-lab.top/)
- [课程初始代码](https://gitee.com/ftutorials/xv6-oslabs-hitsz)
- Lab1 起点：`5aeed89880c77f3c3f39d3fcc312c2634ec631ce`
- Lab2 更新后起点：`1d66d77b5473d368324c9e8ad233c6fa9e55acf7`（`syscall-26-fall-v2`）
- Lab3 起点：`fed5864e1920de450f8e1c6ee3a3f424eab3e8f5`
- Lab4 起点：`6e312f7efdf97cd3553007f30943bbfd5b0018ef`

各目录保留课程源码的 `LICENSE` 和 `README`。
