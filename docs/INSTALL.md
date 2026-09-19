# DLSSG SM86 融合版

**中文** · [English](INSTALL.en.md)

**把发布包根目录的所有文件（`version.dll`、`winmm.dll`、`dbghelp.dll`、`dinput8.dll` 和 `dlssg_sm86.ini`）放到游戏实际渲染 EXE 旁，照常启动。** 四个代理不用挑：哪个先被游戏加载，哪个就是本 mod，其余自动待机、只做转发（见下面“多个代理同时存在”）。运行时无需 Python 或 PowerShell 启动器。

代理 DLL 内嵌一个配套的原版 DLSSG 运行库（含模型和管线）及其 SM86 后端，默认 `Mode=Bundled`。游戏请求不同版本的 DLSSG 时，统一加载这套内置实现。首次运行将配套文件释放到 `%LOCALAPPDATA%\DlssgSm86\bundles\<bundle-id>`，校验后加载；以后复用缓存，损坏时自动恢复。

**出厂 INI 只有两种模式**（见下面“常规使用”）。所有诊断 / 兼容性 / 性能实验开关仍然被解析和支持，但不在出厂 INI 里；缺失时各自取安全且经过验证的默认值，完整清单见后面的“高级 / 诊断键”。

## 两种构建变体（内嵌哪个运行库）

一次构建只内嵌**一个**运行库，`manifest.json` 的 `runtime_model` 写明是哪一个；两个变体的 INI 键完全相同：

| 变体 | 产物 | 内嵌运行库 | 最大生成帧 | 备注 |
|---|---|---|---|---|
| **310.1** | `dlssg-release-x64.zip` | `nvngx_dlssg.dll` 310.1.0.0 | 3（4×） | 优化内核、跨内核合并、图像内核补丁全部可用 |
| **310.9** | `dlssg-release-x64-310.9.zip` | `nvngx_dlssg_310.9.1.dll` 310.9.1.0 | 5（6×） | 原生 6×；优化内核、跨内核合并与图像内核补丁均已可用（63 个变体行：60 个网络变体 + 3 个按 310.9.1 RVA 重新推导的图像补丁）。`Router=SM75` 现在也可用（3070 前向 JIT 与真 Turing（RTX 2080 Ti）都已验证）。`HardwareBilinear` 仍不可用 |

构建方式：`powershell -File build.ps1 -RuntimeModel 310.1|310.9`（310.9 的运行库不随源码分发，默认从
`assets/runtime/nvngx_dlssg_310.9.1.dll` 读取，可用 `-Runtime3109 <路径>` 指定）。出厂 INI 里的
`MaxGeneratedFrames=3`（4×）在两种构建上都直接生效；想要 6× 需要 310.9 构建并**自己把它改成 5**（见下）。

## 多个代理同时存在（根目录四个代理）

发布包根目录带四个工具类代理：`version.dll`、`winmm.dll`、`dbghelp.dll`、`dinput8.dll`。它们内嵌的运行库、后端与行为完全相同，区别只有文件名和转发目标。**全部复制过去就行，不需要判断游戏会加载哪一个**：

- 进程里第一个被加载的那个代理（DllMain 先跑的）拿到一个进程内的命名标记，成为 **active**：它挂 `LoadLibrary` 钩子、读 INI、加载后端。
- 之后加载的同族代理成为 **standby**：照常解析并转发自己那份系统 DLL 的全部导出（转发是完整的），但**不装钩子、不读 INI、不写日志**，所以不会有第二份帧生成，也不会重复释放缓存。
- `[Logging] Level=2` 时 loader 的 `configuration` 记录里有一项 `proxies`，形如 `{"active":"version.dll","standby":["winmm.dll"]}`，可以直接看出这局是谁在干活。
- 卸载时把这四个（以及 `dlssg_sm86.ini`）一起删掉/还原。

`dxgi.dll` 与 `d3d12.dll` 在渲染热路径上、且加载顺序敏感，所以**不在根目录**，放在 `alternatives\` 里；只有四个工具类代理都没被游戏加载时才手动复制其中**一个**过去，说明见 `alternatives\README.md`。

## INI 放在哪里、何时生效

配置文件固定命名为 `dlssg_sm86.ini`，放在代理 DLL 所在目录（= 渲染 EXE 目录）。**修改后完全退出并重新启动游戏**，当前没有热重载。

- 布尔开关填写 `0` 或 `1`。以 `;` 开头的行为注释。
- `Mode` 等枚举值不区分大小写。
- 日志目录、缓存目录可以填写绝对路径；相对路径以代理 DLL / INI 所在目录为基准。自定义路径不会展开 `%LOCALAPPDATA%`、`%TEMP%` 等环境变量，请填写实际路径；使用系统默认缓存目录时将 `CacheDirectory` 留空。
- **某个键写错（非法数字、枚举拼错、越界）只影响这个键**：该键回退到自己的默认值，日志里记一条 `configuration_warning{section,key,value,default,reason}`（Level 1 即可见），mod 照常启用，其余键不受影响。例如 `MaxGeneratedFrames=99` 会回到 `3`、`Router=typo` 会回到 `Auto`、`[Runtime] Path` 指向的文件不可用会回到 `Mode=Bundled`。
- `configuration_error`（整个 mod 关闭、保留游戏原始的 DLSSG 加载）现在只留给**整份配置根本读不了**的情况（内存不足、`[Logging] Directory` / `[Debug] CaptureDirectory` / `[Runtime] Path` 写成了无法构成路径的字符串）。正常的拼写错误不会再走到这里。
- 后端不支持 INI 要求的某个开关时（例如 310.1 构建上写 `Preset=B`，或 `[Backends]` 指向一个老 ABI 的外部后端），loader 会**去掉那几个标志位继续安装**，并记一条 `kernel_selection_unsupported{requested_flags,supported_flags,stripped_flags,effect:"installed without them"}`；该开关无效，其余功能照常。

## 常规使用：两种模式

出厂 `dlssg_sm86.ini` 的完整内容：

```ini
[General]
Enabled=1

[FrameGeneration]
Optimized=1
MaxGeneratedFrames=3

[Compatibility]
Preset=Auto

[Logging]
Level=1
Directory=dlssg_sm86\logs

[Runtime]
Mode=Bundled
CacheDirectory=
```

只有一个决定“怎么跑帧生成”的开关：`[FrameGeneration] Optimized`。它是一个**一致性档位**，判据只有一条：
**允许生成的画面偏离官方 NVIDIA 运行库多远**。档位越高越快、离官方画面越远；其余所有旋钮都由档位决定，
普通用户不需要逐个理解。

| 档位 | 设置 | 一致性保证 | 这一档打开了什么 |
|---|---|---|---|
| **0 原厂（stock）** | `Optimized=0` | **与官方运行库逐位一致** | 什么加速都不做。**注意这不是“不替换内核”**：SM86 路线仍然生效，后端替换进去的是从运行库里提取出来的**原厂内核镜像**（真 SM86 卡用 cubin，其它架构用 PTX），数值与运行库完全一致。它不等于 `KernelImage=Original`——在 **310.9 构建 + Ampere** 上尤其重要：310.9.1 的原厂图像内核是 sm_89 blob，在 Ampere 上根本创建不出来，所以 Ampere 上的“原厂数值”只能靠这条路径。 |
| **1 逐位一致（默认，推荐）** | `Optimized=1` | **与官方运行库逐位一致**（两份真实游戏抓取、320 张图实测） | 全部**可证明不改变输出**的加速：63 个重写内核变体 + 全部跨内核合并、精确图像内核补丁（310.9：P1/P3/P4/P7）。等价于 `OptimizedKernels=1, ImagePatches=1, SkipRepeatedRealCopy=0, HardwareBilinear=0, ChainBlock0=0, LaunchChains=0, PlainVariant=0, DisableFusions=0`。（`SkipRepeatedRealCopy` 自 0.3.2 之后的版本起不再随任何档位打开，要用请显式写 `1`，见下面。） |
| **2 快（有损）** | `Optimized=2` | **不再逐位一致**；实测最差 PSNR 仍在约 50 dB 以上（两份抓取） | 档位 1 + 达到 PSNR 门槛的有损图像内核行。**仅 310.9 构建**；310.1 构建上没有这些内核，会自动退回档位 1 并记一条 `kernel_selection_unsupported`。 |
| **3 最快（有损）** | `Optimized=3` | **不再逐位一致**，画质代价最大 | 全部仍然更快的有损加速：每一行有损图像内核；在 **310.1** 构建上另外打开 `HardwareBilinear=1`（纹理单元双线性，旋转时最底一行可差 12 LSB）。慢的或有结构性伪影的变体已永久退役，任何档位都不会启用。 |

档位是**累加且单调**的：更高的档位只会再加东西，不会撤回低档位的加速。想逐个覆盖某一项，用下面“高级 / 诊断键”
里的 `ImagePatches` / `SkipRepeatedRealCopy` / `HardwareBilinear` / `ImageApprox` / `ImageApproxMask`——它们在档位
之后生效，可以把档位打开的东西再关掉，也可以在低档位上单独打开某一项。

其余出厂键：

| 配置项 | 节 | 默认值 | 说明 |
|---|---|---|---|
| `Enabled` | `[General]` | `1` | `1` 启用 DLSSG 重定向、适配和帧生成；`0` 关闭：游戏原样加载它自己的 DLSSG（在 Ampere 上就意味着没有帧生成）。代理仍转发系统 DLL 的原有导出。 |
| `Optimized` | `[FrameGeneration]` | `1` | 一致性档位 `0`–`3`，见上表。`[Compatibility] OptimizedKernels` 是它的**向后兼容别名**（同样接受 `0`–`3`，所以老 INI 里的 `OptimizedKernels=1` 仍然是档位 1）：两者都在时 `Optimized` 优先，只有别名时用别名，都没有时默认档位 0。**值写错或超范围时回退到档位 1** 并记一条 `configuration_warning`——写了这个键的人本意是要打开加速，不该被静默退回原厂。 |
| `MaxGeneratedFrames` | `[FrameGeneration]` | `3` | 上报的最大“额外生成帧”数量的上限：`5`=最多 6×，`3`=最多 4×，`2`=3×，`1`=2×，`0`=保留运行库原有上报。**实际生成数量由游戏请求决定**。出厂值是 `3`（4×）：自带 Dynamic MFG 的游戏默认会直接跑到这里给出的上限，`5` 对多数人偏高（公开 issue #497/#499），所以 6× 改成**按需手动开**——在 310.9 构建上把这一行改成 `5` 即可（310.1 构建即使写 5 也会被钳回 3，并记一条 `limit_clamped`）。自带旧版 4X Streamline 插件的游戏无论写几都只有 4×。 |
| `Preset` | `[Compatibility]` | `Auto` | DLSSG 渲染预设（UI 重组），**仅 310.9**。`Auto` 让游戏 / 驱动配置决定（默认）；`A` 强制关闭 UI 重组；`B` 强制开启（生成帧里 HUD/UI 更干净）——**但 B 只在游戏同时向 DLSS-G 交出 HUD-less 图和 UI 平面时才生效**，多数游戏不提供、此时为空操作。310.1 忽略本键。 |
| `Level` | `[Logging]` | `1` | `0` 关闭日志；`1` 只记错误；`2` 增加配置、加载和能力信息；`3` 再增加内核创建、Evaluate 和标记信息。日志文件名为 `loader_<PID>.jsonl` 和 `backend_<PID>.jsonl`。 |
| `Directory` | `[Logging]` | `dlssg_sm86\logs` | 日志目录，相对路径以 INI 目录为基准。 |
| `Mode` | `[Runtime]` | `Bundled` | 运行库来源。`Bundled` 始终使用代理内嵌的运行库和配套后端（普通安装用这个，无须版本匹配）。`Auto`/`Pinned` 是高级用法，见下。 |
| `CacheDirectory` | `[Runtime]` | 空 | 空值使用 `%LOCALAPPDATA%\DlssgSm86\bundles`；非空时作为缓存根目录（相对 INI 目录）。 |

一般安装保留出厂 INI 不动即可。想对比原厂数值时把 `Optimized` 改成 `0`，想再快一点（并接受画面不再逐位一致）
改成 `2`，其它一律不用碰。

## 高级 / 诊断键（不在出厂 INI 里）

下面这些键**仍然被 loader 解析和支持**（开发、验证、监控、抓取、MFG 探针都要用），只是不在出厂 INI 里。**缺失时各自取安全 / 最佳默认值**，所以正常安装不需要写它们。要用时把对应的节和键补进 INI（节不存在就自己加一个 `[节名]`）。

> **注意：以下开关默认已经是最快 / 最安全的一组。** 打开这几项会**降低性能**，只用于诊断或特定硬件复测：
> `HardwareBilinear=1`、`ChainBlock0=1`、`LaunchChains=1`、`PlainVariant>0`、`DisableFusions>0`、`ForceGeneratedFrames>0`。

### [FrameGeneration]

| 配置项 | 值 | 默认 | 说明 |
|---|---|---|---|
| `SkipRepeatedRealCopy` | `0/1` | **`0`**（任何档位都不打开） | `1` 跳过运行库在同一组里**重复**发出的那次全分辨率 `OutputReal` 拷贝（第一帧之后逐字节相同，是死存储）。4K 6× 每真实帧约 −566 µs，1080p 6× 约 −164 µs，2× 为 0。离线重放里它是逐位一致的（两份真实抓取的 `OutputReal` 60/60 与 20/20、输出 180/180 与 140/140），但这个结论的前提是“游戏的插件把同一组的几次 Evaluate 连着发、中间不改颜色输入”，这是插件的性质而不是我们内核的性质，重放和合成场景里输入天然不变、验不出来；0.3.2 曾把它放进档位 1 默认打开，随后收到了闪屏反馈，所以现在**只作为显式选项**：要用就写 `1`。后端有防护，任何疑点都照常转发并记 `real_copy_mismatch`。详见下面小节。两个模型都支持。 |
| `ImageApprox` | `0/1` | **档位决定**（档位 ≥2 为 `1`） | 有损图像内核行的显式开关，覆盖档位的答案：在档位 1 上写 `1` 就只加有损图像行，在档位 2/3 上写 `0` 就只留逐位一致的部分。**仅 310.9**：310.1 没有这些内核，写了会被握手裁掉并记 `kernel_selection_unsupported`。 |
| `ImageApproxMask` | 位掩码 | **档位决定**（档位 2 = 默认集，档位 3 = 全部） | 精确指定启用哪几行有损图像内核，每一位对应一个固定槽位。槽位**一次分配、永不回收**：退役的行留下空位，所以老文档里写下的掩码值永远指同一批行；掩码里出现未分配的位会被忽略并记一条 `image_approx_mask_unassigned`。同一个内核有多行时只启用位号最小的那一行，另一行记 `image_approx_row_shadowed`。armed 的行、槽位与资源号都在后端 `image_approx` 记录里。 |
| `ForceGeneratedFrames` | `0`–`16` | `0` | **诊断实验（会降低体验）**。让 `DLSSG.MultiFrameCount` 及 NVIDIA App 覆盖通道的**每一次 getter** 返回这个数（夹到 `MaxGeneratedFrames` 与后端上限），不动 setter。只有当游戏 Streamline 插件会**读回**帧数时才有作用；否则只会让插值相位错乱、画面发抖。配合 `[Debug] MfgProbe=1` 判定，方法见 `docs/MFG_PROBE.md`。用完改回 `0`。 |
| `ForcePluginFrames` | `0`/`2`–`5` | `0` | **实验性**。给自带**旧版 4X** Streamline 插件的游戏（悟空 `sl.dlss_g.dll` 2.7.4、赛博朋克 2077 2.7.1）在内存里打补丁跑 6X：把插件硬编码的 4X 上限（`min(MultiFrameCountMax,3)` 里的 `3`）与默认初值改成 N，并钩住插件的逐帧常量下发把 `numFramesToGenerate` 强制成 N，使运行库视图、插件循环、资源分配都落在 N（这正是 `ForceGeneratedFrames` 单改 getter 会失步的原因）。仅在按 SHA-256/FileVersion + 唯一字节特征**精确识别**到已知版本时才动，否则不动并记 `plugin_mfg_unrecognized`。须同时设 `MaxGeneratedFrames=N`。补丁在插件验签**之后**、CreateFeature 计算上限**之前**施加。310.1 上会夹到 3（空操作）。**实测结论（悟空 2.7.4，2026-09-14）：补丁机制成功**（日志 `plugin_mfg_install` 上限 3→5、`plugin_mfg_force` 把 `numFramesToGenerate` 强制成 5），**但游戏跑几帧 6X 后整个 DLSS-G 硬失败关闭**（连 4X 也没了）——4X 集成的交换链后备缓冲按 4X 一次性建好，强制第 5 帧无缓冲可呈现，Streamline 不优雅退回而是关掉整个帧生成。**故本键对 4X 游戏不可用、且有害**，默认关。真 6X 需要游戏自带 ≥2.11.1 的 `sl.dlss_g.dll`（如 A Plague Tale 的 2.11.1，实测原生 6X 干净）。见 `docs/MFG_PROBE.md`。 |

### [Logging]

| 配置项 | 值 | 默认 | 说明 |
|---|---|---|---|
| `File` | `0/1` | `1` | `1` 写日志文件；`0` 关闭文件输出（仍受 `Level` 控制）。 |
| `DebugOutput` | `0/1` | `0` | `1` 同时通过 Windows 调试输出发送日志，可由调试器接收；不在游戏画面上显示。 |
| `EvaluateEvery` | `1`–`1000000` | `120` | 正常 Evaluate / 标记日志的采样间隔（按 Evaluate 调用计数）。`1` 记录每次；`0` 按 120 处理。Level 3 下前 12 次总是记录。 |

### [Debug]

| 配置项 | 值 | 默认 | 说明 |
|---|---|---|---|
| `MarkGeneratedFrames` | `0/1` | `0` | `1` 在生成输出上绘制 `FG 1/3` 等实际序号标记，用于区分毛刺出现在生成帧还是真实帧。真实帧和 Reset 输出跳过。标记会改动生成帧像素，做全图数值比较时请关闭。 |
| `MarkerX` / `MarkerY` | `0`–`65535` | `8` / `8` | 标记左上角坐标（输出纹理像素）。 |
| `MarkerScale` | `1`–`8` | `2` | 标记缩放倍率；矩形为 `24×scale` 宽、`9×scale` 高。 |
| `Capture` | `0`–`100000` | `0` | 录多少次 Evaluate 的参数、输入和输出，供离线重放（GCR）。`0` 完全不装任何钩子，性能影响为零。数的是 Evaluate 次数（3× 每真实帧 2 次、6× 每真实帧 5 次）。1080p RGBA8 约 48 MB/次。完整机制见 `docs/CAPTURE.md`。 |
| `CaptureDirectory` | 路径 | `dlssg_sm86\capture` | 抓取目录，相对游戏 EXE 目录。 |
| `CaptureSkip` | `0`–`1e8` | `0` | 先跳过多少次 Evaluate（跳过加载画面 / 过场）。 |
| `CaptureStartOnReset` | `0/1/2` | `1` | 从哪一帧开始录。`2`：只等“刚创建完特性之后的第一次 Evaluate”，是重放能逐位一致的唯一起点；`1`：`DLSSG.Reset` 帧或新建特性的第一帧，先到先算；`0`：立刻开始。`1`/`2` 等满 3 分钟不到就什么都不录。 |
| `CaptureStartKey` | 键名 / VK 码 | `0` | 按键触发起点：`F9`（F1–F24）、单个字母或数字，或虚拟键码（`120` / `0x78`）。设了之后 `CaptureSkip` 满足后继续等，直到在游戏里按下这个键，再从那一刻应用 `CaptureStartOnReset` 的规则（`0` = 按下的那次 Evaluate 立刻开始录，`start_reason=key`；`1`/`2` = 从按键起再等 Reset / 新建特性，3 分钟超时从按键算起）。这是让抓取落在“人物正在运动”那一刻的办法；等待期间记 `capture_waiting_for_key`，按下记 `capture_start_key`。 |
| `CaptureNoStall` | `0/1` | `0` | `1`：游戏线程永远不等。整份抓取缓冲在系统内存里：环和请求的次数一样深、回读缓冲由写盘线程在等待起点期间提前建好、录满之后文件在后台写完（`capture_finishing` → `capture_done`），所以录到的是连续的帧；组的内存按预热时学到的尺寸整组估算（第一组或没有东西在飞时总是放行，预算小于一组时退化成一次只录一组），预算不够时整组跳过（组 = 同一真实帧的全部生成帧，`MultiFrameIndex` 1..count），绝不在组中间丢平面或丢帧。`0`：4 槽环、写盘跟不上时游戏线程最多等 2 秒（4K 下游戏会降到几帧）。事件 `capture_prewarm` / `capture_group_skipped`，manifest `capture.taken_groups` / `skipped_groups` / `skipped_groups_memory`。重放时每个间隙后的第一组只用来暖运行库的上一帧历史，`capture_compare.py` 默认把它排除在汇总外。 |
| `CaptureRingSlots` | `0`、`4`–`1024` | `0` | 在飞的回读槽位数；`0` = 默认（4，`CaptureNoStall=1` 时等于 `Capture` 的次数）。 |
| `CaptureMemoryMB` | `0`–`1048576` | `0` | 回读内存预算（池里的 + 在飞的），MiB；`0` = 安装时可用物理内存的一半。回读堆在系统内存，不占显存。 |
| `CaptureAsyncCopy` | `0/1` | `1` | `1`：把 PCIe 传输从渲染时间线上挪走。游戏自己的命令列表里只做显存→显存的 `CopyResource`（拷进一张暂存纹理），再由一条独立的 `COPY` 队列把暂存纹理搬进回读缓冲，和游戏渲染并行；`COPY` 队列先 `Wait` 游戏那次提交的 fence，`collect()` 等的是 `COPY` 队列自己的 fence。`0`：回读拷贝留在游戏的命令列表里（旧路径，3070 实测 4K 每生成帧 +122 %）。事件 `capture_plane_inline`（某个平面退回旧路径）、`capture_async_copy_unavailable`（这台机器建不出 `COPY` 队列/列表，全部退回旧路径）。 |
| `CaptureVramMB` | `0`–`1048576` | `0` | 暂存纹理的显存预算，MiB；`0` = 1536。暂存纹理按 (设备, 格式, 尺寸) 池化复用，`COPY` 队列读完就还池，所以稳态只需要几组的量；超出预算的平面退回逐平面的内联回读并记 `capture_plane_inline`，抓取不会因此不完整。`CaptureAsyncCopy=0` 时这个键没有作用。 |
| `CaptureLean` | `0/1` | `0` | `1`：只回读不能重建的数据。同一组内 index>1 的 Evaluate 交出的输入资源和 index 1 完全相同（三份悟空抓取 54/54 个纹理平面逐字节一致），所以只记成对 index 1 文件的引用；`DLSSG.OutputReal` 是运行库对 `DLSSG.Backbuffer` 的拷贝（18/18），也记成引用。被引用的平面 json 里 `stored=false`、`source=<相对抓取根目录的文件>`，`sm86_replay` 与 `capture_compare.py` 会跟过去。4X 下每组字节约减半，回读的 PCIe 时间同步减少。16 字节的 buffer 参数总是照录。 |
| `CaptureInputs` | `0/1` | `1` | `0` 不录输入像素（体积大幅下降，但不能重放）。 |
| `CaptureOutputs` | `0/1` | `1` | `0` 不录 `OutputInterpolated` / `OutputReal` 像素。 |
| `MfgProbe` | `0/1` | `0` | `1` 记录每一次对帧数参数的 set 与 get，带调用方模块名、槽位、值和上下文，用来判断“生成几帧”是谁决定的。复用抓取的参数存储钩子，但 `Capture=0` 时也能单独工作。事件与判定见 `docs/MFG_PROBE.md`。 |

### [Compatibility]

| 配置项 | 值 | 默认 | 说明 |
|---|---|---|---|
| `OptimizedKernels` | `0`–`3` | 档位 0（见别名说明） | `[FrameGeneration] Optimized` 的**向后兼容别名**，接受同样的档位范围；两者都在时 `Optimized` 优先。老 INI 里的 `OptimizedKernels=1` 仍然是档位 1。 |
| `KernelImage` | `Auto/PTX/Cubin/Original` | `Auto` | 内核加载格式，见下表。**cubin 被驱动拒绝时自动回退到同一内核的 PTX**（见“驱动版本要求与 cubin → PTX 自动回退”）。`Original` 只装钩子不替换任何内核（本机取原厂数值参考；**310.9 构建在 Ampere 上用不了这一项**——它的原厂图像内核是 sm_89，Ampere 上创建失败，本机参考请用 `Optimized=0`+`KernelImage=PTX`）。 |
| `Router` | `Auto/SM86/SM75` | `Auto` | 内核族。`Auto` 按物理 GPU 选（SM86 及以上走 SM86，Turing 走 SM75）。**SM75 是实验项**，**两种构建都有**。`Optimized=1` 时两边都创建 63 个变体行、跨内核合并与图像补丁全开，日志记一条 `sm75_route_limits`（`variant_rows=63`、`image_patches_partial` 为空）；区别只在补丁的来源与 `HardwareBilinear`：310.1 是两个图像补丁 + 纹理探针（`HardwareBilinear` 可用），310.9 是 3 个按 310.9.1 RVA 重新推导的补丁（63 = 60 + 3），纹理探针的那个内核 310.9.1 已删除，所以 `HardwareBilinear` 在该构建上强制 0。3070 前向 JIT 上两种构建都与各自的 SM86 原厂路线逐位一致（310.9 上含 6×）；**真 Turing（RTX 2080 Ti）上**：sm_75 cubin 与 PTX、档 0 与档 1 都逐位一致，且同一份输入下的输出与 RTX 3080 Ti 的输出逐位一致（rotate 合成场景 32/32；两者与 RTX 5070 上官方输出的差异也逐字节相同，是 Blackwell 与 Turing/Ampere 之间的硬件累加差，见 `docs/evidence/sm75/turing_2026-09-17.md`）。Turing 上的性能尚未测量。SM75 原厂内核族来自 Coldwood1026（见 `THIRD_PARTY_NOTICES.txt`）。 |
| `SM75Family` | `Repaired/Original` | `Repaired` | 仅在 `Router=SM75`（或物理 Turing）时有意义，选哪一份**导入的 sm_75 原厂内核族**。`Repaired` 是本项目修复过的那份：Coldwood1026 的 f16x2 min/max 模拟经一块用通用地址寻址的 `.local` 缓冲交换半字（未定义行为，9 个 DL2 内核 342 处），已按值等价地改写进寄存器，72 个 cubin 也由修复后的 PTX 重新编译——**默认值，本仓库的全部结果都是在它上面测的**。`Original` 是原样导入的那份（cubin 逐字节是 Coldwood 的二进制，PTX 只做了 `.version` 归一化），留着是因为离线 `ptxas` 会给未修复的 PTX 分配真栈帧，**原厂 cubin 路径在真实 Turing 上很可能从来没碰到这个缺陷**，而只有 20 系实机能回答；给真实 Turing 用户做 A/B 用。以 PTX 为输入的 JIT 路径（非 Turing 卡上的前向 JIT，3070 实测）用 `Original` 会重现修复前那张 43–73 dB 的表。只换原厂内核，我们自己重写的变体与图像补丁不受影响；310.9 构建上只覆盖与 310.1 共用的 44 个内核（新增的 26 个没有"原样"版本）。走 SM86 路线时该键完全无效，日志记一条 `sm75_family_ignored`。 |
| `Preset` | `Auto/A/B` | `Auto` | DLSSG 渲染预设（UI 重组），**仅 310.9 构建有效**。`Auto` 不干预；`A` 强制关闭 UI 重组；`B` 强制开启（需要游戏同时提供 HUD-less 与 UI 平面，另占两张全分辨率 FP16 表面）。写在 310.1 上会被拒（`kernel_selection_unsupported`）。详见下面小节。 |
| `SpoofArchToGame` | 缺省 / `0/1` | **缺省 = 自动** | 只管**游戏（以及代它做判断的 Streamline）那道架构闸门**。**这是个三态键**：**不写**（出厂 INI 就没有这一行）= 自动，**写 `1`** = 显式，两者装重定向的时机完全一样——取下面五个时机里最早的那个，与显卡无关；**写 `0`** = 永不装（这一档在完整解析 INI 之前就要知道，所以 loader 在启动时先用 kernel32 的 `GetPrivateProfileStringW` 单独读一次 `[General] Enabled` 与本键，值非法或为空都当自动），Turing 上记一条 `arch_spoof_disabled{redirect_installed}`。自动与显式 `1` 的唯一区别在**日志**：自动档只在确认这是 Turing 主机之后才把 `arch_spoof_*` 记录放出来（第一次真正改写，或后端的 Turing 判决 + `turing_host_defaults`），所以**非 Turing 显卡上出厂 INI 一条 `arch_spoof_*` 都不会有**。为什么要装这么早：**Streamline 2.x 在 `slInit` 里就决定帧生成插件"本硬件不支持"** —— `sl.dlss_g` 默认最低架构 AD100，`sl.common` 拿每个适配器的 `NvAPI_GPU_GetArchInfo` 架构去比它，然后把插件卸掉（日志里是 `Ignoring plugin 'sl.dlss_g' since it is not supported on this platform`），这发生在 D3D 设备存在之前、远早于 NGX 核心向我们索要 DLSS-G snippet，所以任何"等运行库加载之后"的武装都来不及（RTX 2080 Ti + FF7 Rebirth / Streamline 2.8.0.0 实测：写 `1` 与不写这一行的结果一模一样，插件都不在进程里）。机制：把 `nvapi64.dll` 导出表里 `nvapi_QueryInterface` 那一项重定向到该模块自己节尾填充里的 14 字节跳板，使 `NvAPI_GPU_GetArchInfo` 对外报告一个更新的架构（报哪个由下一行的 `SpoofArchValue` 决定：默认对 Turing 和 Ampere 都报 Blackwell `0x1b0`；不改任何 NVIDIA 代码字节；**NVIDIA 自己的组件一律看真实架构**：`_nvngx.dll`、`nvngx_*`、`nvapi*`、我们自己的 `sm86_backend.dll`、`\DriverStore\FileRepository\` 下的驱动组件，以及 **NGX 模型库里的 snippet**——NVIDIA App 的「DLSS override」或 NGX OTA 之后，核心是从 `C:\ProgramData\NVIDIA\NGX\models\dlss\versions\<n>\files\160_E658700.bin` 这样的路径加载超分 / 光线重建 / 帧生成 snippet 的，名字里没有 `nvngx_`，0.3.3 把它们一起骗了，超分 snippet 于是走 `arch>=0x180` 的内核路径把 GPU 挂死（创建 SuperSampling 特性之后立刻 `DXGI_ERROR_DEVICE_HUNG`，帧生成都还没被创建；issue #535，以及启动崩溃 #538 / #540 / #542），所以 0.3.4 起 `\NVIDIA\NGX\models\` 下除 `sl_*` 以外的每个组件、以及任何 `.bin` 模块都看真实架构；`models\sl_*`（Streamline 自己的 OTA 插件）正是要改写的对象，照常改写）。日志里每个**不同的**调用方模块一条 `arch_spoof_applied`（新增 `path` 字段给出完整路径），被排除的调用方每个模块一条 `arch_spoof_excluded`，所以能看出是谁问的、又给了它哪个答复。**只有真实答案是 Turing（`0x160`）或 Ampere（`0x170`）才改写**（Ampere 是 0.3.3 加入的：RTX 3060 Laptop 在 FF7 Rebirth 上与 2080 Ti 现象相同，issue #509）：别的架构上重定向照装但什么都不做，并记一条 `arch_spoof_inert{real_arch,rewritten_archs}`；完整解析 INI 后读到 `0`（启动时那次读不到 INI 的情况）也会让它立刻停止改写。`arch_spoof_installed` 带 `trigger` 与 `default`：**早期三档** `dllmain`（我们被加载时 `nvapi64.dll` 已在进程里）/ `nvapi_load`（我们的 LoadLibrary 钩子接住了 `nvapi64.dll` 自己的加载）/ `dependency_load`（`nvapi64.dll` 作为别的模块的静态导入进来，某次加载之后才发现它在）——只有这三档赶在 `slInit` 之前；**兜底两档** `settings`（读完 INI，即游戏第一次索要 `nvngx_dlssg.dll`）/ `backend`（后端装完）。`default=true` 表示是自动档做的决定，与早晚无关。**它与 NGX 核心的 `0xbad0000b` 无关**：那一条是核心拿显卡架构去比运行库导出的最低架构（见下面 `fg_gate_create_feature` 一行），任何 NVAPI spoof 都改不了。 |
| `SpoofArchValue` | `Auto/Ada/Blackwell` | `Auto` | 改写时对外上报哪个架构。`Auto`（默认）对 **RTX 20（Turing）和 RTX 30（Ampere）都报 Blackwell（`0x1b0`）**：两条内嵌运行库都做多帧生成，后端本来也是以 Blackwell 主机的身份把适配器交给运行库，而游戏和 Streamline 的 3X / 4X / 6X 选项正是按这个架构放开的——报 Ada 能过帧生成那道闸门，但游戏里只剩 2X（真 20 系实测，issue #527 / #528）。`Ada`（`0x190`）留作**诊断覆盖**：0.2.4 与 0.3.0–0.3.2 对外报的就是它，需要对比时可以写回去。`Blackwell` 显式写死同一个值（两者也接受 `0x190` / `0x1b0`）。**这个值从来不是 0.3.3 崩溃的原因**：崩的是 NVIDIA 自己的 DLSS 超分模型也收到了改写后的架构——NVIDIA App 的「DLSS override」或 NGX 在线更新生效时，它是从 NGX 模型目录里按 `160_E658700.bin` 这样的名字加载的，按叶名的排除规则拦不到；3070 上实测报 Ada 与报 Blackwell **都**会挂 GPU，而把这个 snippet 排除之后**两个都不会**，所以修的是「按路径排除 NVIDIA 自己的组件」（`src/spoof_policy.h`，见上一行），不是换一个更小的架构。和 `SpoofArchToGame` 一样在启动时用 kernel32 单独读一次；值非法回到 `Auto` 并记一条 `configuration_warning`。`configuration.spoof_arch_value`、`arch_spoof_installed{spoof_arch_value,reported_arch_turing,reported_arch_ampere}`、`arch_spoof_applied.reported_arch`（432 / 400）说明实际用的是哪个。 |
| `ForceSM86Route` | `0/1` | `0` | `1` 允许在非 SM86 GPU 上强制走 SM86 后端做验证（低于 SM86 仍被拒）。 |
| `SimulateAmpere` | `0/1` | `0` | `1` 为验证调整架构上报；必须同时 `ForceSM86Route=1`。不改物理 GPU。 |
| `LaunchChains` | `0/1` | `0` | **实验项，不要开启（会降低性能甚至有害）**。`1` 把连续内核提交攒成 `NvAPI_D3D12_LaunchCuKernelChain`。5070 上无收益（每次 Evaluate 慢 2–3%），**310.9 上实测有害**（中位 Evaluate 超出最小值 3–120 倍）。保留只为在别的驱动 / GPU 上复测。 |
| `DisableFusions` | `0`–`63` | `0` | **诊断位掩码（打开会变慢）**，关闭 `OptimizedKernels` 的某类跨内核合并：1 解码器 upscale+add+1×1，2 解码器 1×1 对，4 `k_initial_merge`+convPre，8 conv0+池化，16 残差链，32 `k_central_block`+block1 conv0。默认 0 = 全部合并开启。 |
| `PlainVariant` | `0`–`3` | `0` | 同一内核登记了多个替换实现时选第 n 个（越界取最后一个）。**默认 0 是实测最快的一组**；改成 >0 是换备选实现复测，通常更慢。 |
| `ImagePatches` | `0/1` | **档位决定**（档位 ≥1 为 `1`） | `0` 关闭图像处理内核的**精确** PTX 级补丁（需要档位 ≥1，两个模型都支持）。补丁语义不变、逐位一致。注意它只管精确补丁：有损行由 `ImageApprox` 管。 |
| `HardwareBilinear` | `0/1` | **档位决定**（档位 3 为 `1`，其余 `0`；310.9 上恒为 0） | **仅 310.1**（310.9 上强制 0）。`1` 让合成内核用纹理单元双线性采样代替手写 fp32 混合，每个 index 省约 5 µs，但**旋转场景最底一行可差 12 LSB**（原厂在底边混入零纹素）——所以它属于档位 3 的那一类有损加速。 |
| `ChainBlock0` | `0/1` | `0` | `1` 把 DL2 block0 的 8 个残差卷积也换成持久化链内核。5070 上与 8 次原生提交**持平**（无收益），默认关闭；其它 GPU 可复测。 |

### [Runtime]

| 配置项 | 值 | 默认 | 说明 |
|---|---|---|---|
| `Path` | 路径 | 空 | 仅 `Mode=Pinned` 使用，指向目标原版 `nvngx_dlssg.dll`。`Bundled`/`Auto` 不使用。 |

`Mode` 值：`Bundled`（普通安装，内嵌运行库和后端）；`Auto`（加载游戏请求的原运行库，已知哈希自动配套后端）；`Pinned`（使用 `Path` 指定的运行库，需匹配后端）。默认模式下缓存释放 / 加载 / 安装失败会记 `runtime_selection_failed` 并回退原始请求。

### [Backends]

高级外部后端映射，默认无此节。键是目标原运行库的完整 SHA256，值是匹配后端 DLL 路径，仅 `Auto`/`Pinned` 需要：

```ini
[Backends]
; <runtime-sha256>=backends\matching_backend.dll
```

外部后端最好支持内核选择扩展（可选导出 `DlssgMod_SupportedRouteFlags`）。不支持（或只支持一部分）时，loader 把该后端答不了的标志位**去掉之后照常安装**，并记一条 `kernel_selection_unsupported{requested_flags,supported_flags,stripped_flags,effect:"installed without them"}`；那些开关无效，但帧生成本身不会因此没有。注意出厂默认的最佳集本身就带着 `NO_HW_BILINEAR` / `NO_CHAIN_BLOCK0` 两个诊断位，所以不带该导出的旧 ABI 后端一定会出现这条记录（它的 `supported_flags` 为 0，全部被去掉）。

### KernelImage 行为

| KernelImage 值 | 路由启用后的行为 |
|---|---|
| `Auto` | 物理架构与所选内核族相同时用预编译 cubin，否则用 PTX。cubin 被驱动拒绝时自动回退到 PTX。 |
| `PTX` | 总是用所选内核族的 PTX，由驱动 JIT；3080 Ti 也适用。 |
| `Cubin` | **优先**用预编译 cubin，要求物理架构与内核族**完全一致**，否则拒绝安装该路径（默认 Bundled 下回退原始加载）。架构一致但驱动拒绝 cubin 时仍会回退 PTX（宁可 JIT 也不要没有帧生成）。 |
| `Original` | **诊断/参考用**：装钩子但**一个内核都不替换**，也不启用优化 / 合并 / 图像补丁。与 `PTX`/`Cubin` 互斥，与 `Router` 无关。**310.9 构建在 Ampere 上用不了。** |

`KernelImage` × `Router` 的实际选择：

| Router / 物理 GPU | `Auto` | `PTX` | `Cubin` |
|---|---|---|---|
| SM86 族，物理 SM86（3070/3080 Ti） | `cubin_sm86` | `ptx_sm86` | `cubin_sm86` |
| SM86 族，物理 SM89/SM120（强制路由验证） | `ptx_sm86` | `ptx_sm86` | 拒绝 |
| SM75 族，物理 SM75（RTX 20 系） | `cubin_sm75` | `ptx_sm75` | `cubin_sm75` |
| SM75 族，物理 SM86 及以上（前向 JIT 验证） | `ptx_sm75` | `ptx_sm75` | 拒绝 |

SM75 族与 SM86 族在 RTX 3070 的前向 JIT 上**逐位一致**（原厂族与优化集都是，box + rotate 两个场景 7 个分辨率 240/240 张；`m16n8k16` 拆成两条 `m16n8k8` 并没有多出一次与原厂不同的舍入）。优化集在 SM75 上**一行不少**：63 个变体行都有 sm_75 镜像，两个图像补丁与 `HardwareBilinear` 的纹理探针都可用（`HardwareBilinear=1` 时两条路线的输出也逐位相同）。

**310.9 构建上同样成立**：310.9.1 的 70 个内核里，44 个共用 310.1 的 sm_75 族，26 个新内核的 sm_75 镜像由 `scripts/perf/ptx_sm75.py` 从仓库自己的 sm_86 PTX 改写而来（`assets/kernels/310_9_1/sm75/`）。3070 前向 JIT 实测：box + rotate × 7 个分辨率原厂族与优化集各 240/240 张、**`--multi 5`（6×）另外 80/80 张**，全部与同机 SM86 原厂路线逐位一致；MFG 相位（`i/(N+1)` 质心）两条路线完全相同。详见 `docs/evidence/310_9_1_sm75.md`。

数值在 RTX 3070 上以 `.target sm_75` 前向 JIT 验证为与 SM86 路线逐位一致；**真 Turing（20 系实机，2026-09-15/17）**上 `sm_75` cubin 全族创建并运行、0 回退，cubin 与 PTX 逐位一致，档 0 与档 1 逐位一致，游戏内可用。Turing 上的耗时尚未量化。详见 `docs/VALIDATION.md` 与 `docs/evidence/sm75/turing_2026-09-17.md`。

#### 驱动版本要求与 cubin → PTX 自动回退

| 路径 | 最低驱动 | 原因 |
|---|---|---|
| cubin（`Auto` 在物理 SM86 上、`Cubin`） | 约 **R580+**（本机 591.86 实测可用） | 全部 cubin 由 CUDA 13.0.2 的 `ptxas` 生成（ELF ABI 65） |
| PTX（其它架构、`PTX`） | 约 **R555+**（PTX ISA 8.5） | 所有 PTX 声明 `.version 8.5`，由驱动 JIT |

驱动落在两者之间时 cubin 创建会失败：**后端用同一内核的 PTX fatbin 原地重试一次**，之后该镜像家族（基础 / 变体 / 图像补丁三类各自独立）改用 PTX。因此驱动偏旧的唯一后果是首次加载多一次 JIT，帧生成照常、数值逐位一致。日志里找 `kernel_image_fallback`（错误级，每进程一次）。诊断变量 `DLSSG_FORCE_CUBIN_FAIL=1` 可强制触发这条路径（仅用于验证）。

#### `SkipRepeatedRealCopy`：跳过重复的 OutputReal 拷贝

两个运行库的 host graph 都在**每次** Evaluate 末尾发一次全分辨率 `CopyResource(Backbuffer → DLSSG.OutputReal)`。`ext_real` 只有一个写者、没有读者，`ext_color` 只读，所以同一组里第一次之后的拷贝都在写逐字节相同的内容，全是死存储。`1` 让后端在 `MultiFrameIndex > 1` 时吞掉它。收益随倍率增长（4K 6× 每真实帧 −566 µs，2× 为 0），与内核替换正交（`Optimized=0` 下也生效）。任何档位都不默认打开（0.3.2 曾在档位 1 默认打开，之后撤回）；打开它的前提是“游戏不会在同组两次 Evaluate 之间重画真实帧”，这一点只有在真游戏里逐个验证，重放验不出来；后端有防护，任何疑点都照常转发并记 `real_copy_mismatch`。机制见 `docs/ARCHITECTURE.md`。

#### Preset：A / B 预设（UI 重组），仅 310.9 构建

驱动 / NVIDIA App 里的 DLSS 帧生成“渲染预设 A / B”在 310.9.1 内部只对应一个布尔量：UI 重组（`DLSSG.UserInterfaceRecompositionEnabled`）。`A` = UI 被烘进颜色历史一起插值（HUD 会被运动矢量拖影）；`B` = HUD-less 颜色与“UI 颜色+Alpha”分开插值（静止 HUD 不被拖动）。`B` 需要游戏同时提供 `DLSSG.HUDLess` 和 `DLSSG.UI`，否则画面与 A 一致但白占两张全分辨率 FP16 表面（1080p 约 33 MB、4K 约 133 MB）。INI 的 `A`/`B` 会盖掉驱动 / NVIDIA App 的预设覆盖；想让驱动说话就保持 `Auto`。每次创建特性记一条 `preset_pinned`。

## 如何确认配置生效

在 Level 2 或 3 的日志中检查（出厂 `Level=1` 只记错误，验证配置时临时改成 `2` 或 `3`）：

| 事件 / 字段 | 含义 |
|---|---|
| loader：`configuration` | 本次读取的 INI、运行库模式、内核格式请求，以及 `optimized_tier`/`optimized_tier_name`（解析后的一致性档位）、该档位解出的每个旋钮（`optimized_kernels`/`image_patches`/`image_approx`/`image_approx_mask`/`skip_repeated_real_copy`/`hardware_bilinear`/`chain_block0`/`launch_chains`）、`warnings`（本次回退了几个键）、`proxies`（`{active, standby[]}`，哪个代理在干活）。 |
| loader：`configuration_warning`（Level 1） | 某个键的值非法，已回退到该键默认值：`section`/`key`/`value`/`default`/`reason`。mod 仍然启用，只有这个键无效。每个出问题的键一条。 |
| loader：`configuration_error`（Level 1） | 整份配置根本读不了（路径无法构成、内存不足），mod 关闭并保留游戏原始加载。**普通拼写错误不会走到这里**，那是上一行。 |
| loader：`kernel_selection_unsupported`（Level 1） | 后端不支持 INI 要求的某些标志位：`stripped_flags` 是被去掉的位，`effect` 为 `installed without them`——**安装照常进行**，只是这些开关无效。 |
| loader：`proxy_forward_incomplete`（Level 1） | 本机 System32 里的那个系统 DLL 没有导出代理转发表里的全部名字（Windows 版本比打包机器旧，或 Wine/Proton）：`library`/`resolved`/`missing`/`missing_names`。这几个转发器会答 `ERROR_PROC_NOT_FOUND`，**其余转发与 mod 本身不受影响**。以前缺一个名字就让 DllMain 失败、游戏直接 `0xc0000142` 起不来且不产生任何日志。 |
| backend：`install` | `actual_sm` 物理架构；`active` 本次是否启用路由；`image` 选定格式；`target_sm`/`router` 解析后的内核族；`optimized_tier`/`optimized_tier_name` 与 `optimized_knobs`（该档位真正落地的整组旋钮）。 |
| backend：`image_approx` | 仅 310.9：有损图像行的解析结果——`enabled`、`optimized_tier`、`mask`/`mask_from`（INI 指定还是档位默认）、`armed[]`（每一行的入口名、槽位、资源号）、`rows_shadowed`、`exact_patches_replaced`。 |
| loader：`backend_install` | `status=0` 表示后端安装成功；结合 `install.active` 判断是否启用了内核替换。`-102` = 这个运行库 SHA 没有对应后端；`-100` = 后端本身不可用，原因见下一行。 |
| loader：`backend_unusable`（Level 1） | `backend_install` 报 `-100` 时紧挨着的那条，说清是哪一种：`loaded=false` + `load_error`（**最常见的一种：杀毒软件或 Windows「智能应用控制」拦了释放出来的 `sm86_backend.dll`**，游戏侧表现为 `Bad Image 0xc0e90002`）、缺 `DlssgMod_GetInfo`/`DlssgMod_Install`、ABI 不符（bundle 缓存里混了不同版本），或后端与被安装模块的运行库 SHA 不匹配。`remedy` 字段给出处理办法（白名单 + 删掉 `%LOCALAPPDATA%\DlssgSm86\bundles` 重新释放）。 |
| `image=ptx_sm86` / `cubin_sm86` / `original` | 已选择的 SM86 内核格式，或未替换。 |
| backend：`kernel_image_fallback` | 驱动拒绝了某个 cubin，该镜像家族已自动改用 PTX。 |
| backend：`preset_pinned` | `[Compatibility] Preset` 生效（仅 310.9）。 |
| backend：`real_copy_skip` / `real_copy_skipped` / `real_copy_mismatch` | `SkipRepeatedRealCopy` 的钩子、跳过计数与防护触发。 |
| backend：`mfg_probe` / `mfg_summary` | `[Debug] MfgProbe` 与 `[FrameGeneration] ForceGeneratedFrames` 的记录，判定见 `docs/MFG_PROBE.md`。 |
| Level 3：`kernel_create` / `evaluate` | 每次内核创建实际用的格式与状态；实际生成数量、结果、`real_copies_skipped`、`image_fallbacks`。 |

确认路由安装成功时同时检查 `install.active=true` 和对应的 `backend_install.status=0`。仅出现 `image=ptx_sm86` 不代表内核已创建或执行，还要看后续 `kernel_create` / `evaluate`。

### 帧生成为什么打不开：`fg_gate_*`（始终开启，无 INI 键）

装上之后 `install` 一切正常、却一条 `kernel_create` / `evaluate` 都没有，说明游戏根本没有创建 DLSS-G 特性，或者创建在运行库建第一个内核之前就失败了 —— 这两件事我们原来的日志分不出来，更说不出原因。决定权全在运行库之上：Streamline 自己的闸门（**硬件加速 GPU 计划（HAGS）关闭** → `eFeatureNotSupportedHWSchedulingDisabled`、驱动低于插件要求）、NGX 核心对 `GetFeatureRequirements` 的最终答复（核心会在运行库的答复之上再加自己的显卡 / 驱动 / HAGS 判断）、插件读的 NGX 能力参数，以及游戏自己的 UI 开关。`src/fg_gate.cpp` 只观察这四处，五条记录都是 Level 2 可见（只有失败的 CreateFeature 是错误级，Level 1 也会记）：

| 事件（backend） | 内容 / 怎么读 |
|---|---|
| `fg_gate_environment` | 安装时一条。`hags`（`D3DKMTQueryAdapterInfo` 的 `KMTQAITYPE_WDDM_2_7_CAPS`：`supported`/`enabled`/`enabled_by_default`，`state=on\|off\|unsupported`）、`driver`（`NvAPI_SYS_GetDriverAndBranchVersion`，如 `610.74` / `r610_00`，外加已加载 `nvapi64.dll` 的文件版本）、`windows`（`RtlGetVersion`）、`ngx_core`（`_nvngx.dll` 路径+版本）、`streamline`（已加载的 `sl.interposer.dll` / `sl.common.dll` / `sl.dlss_g.dll` / `sl.dlss.dll`，只用 `GetModuleHandle` 看，绝不加载）。**`hags.state=off` 基本就是答案**。每一项都可选，取不到就写 `available=false` + 原因，永远不会让安装失败。 |
| `fg_gate_hags_disabled`（Level 1） | 上一条里唯一**既是用户可改的 Windows 设置、又能单独让帧生成在所有游戏里都打不开**的那一项：HAGS 本机支持但被关掉了。因为出厂 `Level=1` 根本不写 `fg_gate_environment`，这条单独按错误级记录，任何日志等级都能看到；`remedy` 字段写着怎么开。（公开 issue #513：有人为此排查了三个小时。） |
| `fg_gate_hooks` / `fg_gate_hooks_deferred` | 钩住的是 **NGX 核心 `_nvngx.dll` 的公开导出**（`GetFeatureRequirements` / `GetCapabilityParameters` / `GetParameters` / `CreateFeature`），四个导出各自报 `attached`。核心还没加载时先监视 `LoadLibrary`（`fg_gate_hooks_deferred`），等它出现再装；记录里还带一次 `ngx_core` 版本。 |
| `fg_gate_requirements` | 每次核心的 `GetFeatureRequirements`（最多 8 条）。`feature_supported` 是**调用方真正拿到的**那个值，`decoded` 按位解开（0 支持 / 1 显卡不支持 / 2 驱动版本不支持 / 4 系统版本不支持 / 8 硬件计划关闭），另有 `min_hw_architecture`、`min_os_version`、`caller`（调用模块+模块内偏移）与 `adapter_description`。注意与 backend 自己的 `feature_requirements` 区分：那条只是运行库对我们桥接的答复，核心还会在上面再加自己的判断。 |
| `fg_gate_capability` | 每次能力查询后（最多 4 条）读回 `FrameGeneration.Available` / `.NeedsUpdatedDriver` / `.MinDriverVersionMajor` / `.MinDriverVersionMinor` / `.FeatureInitResult`、`DLSSG.MultiFrameCountMax`、`DLSSG.ReflexWarp.Available`，以及作为对照的 `SuperSampling.Available`。`Available=0` 或 `NeedsUpdatedDriver=1` = 插件在我们这边任何东西跑起来之前就拒了。`.FeatureInitResult` 是核心为这个特性记下的 NGX 状态：`3134193675` = `0xBAD0000B`（`NVSDK_NGX_Result_FAIL_OutOfDate`），含义与下一行的 `CreateFeature` 失败完全相同，只是核心早在能力查询阶段就记下了，插件看到 `Available=0` 就再也不会调 `CreateFeature`（RTX 2080 Ti / 巫师 3 实测）。 |
| `fg_gate_create_feature` | 每次核心的 `CreateFeature`（最多 16 条）：`feature_id`（DLSS-G = 11）、`status`、`succeeded`、`handle`、`caller`，DLSS-G 还带 `parameters`（`multi_frame_count` / `multi_frame_count_max` / `width` / `height`）。**失败是错误级**，`0xbad0000b`（`NVSDK_NGX_Result_FAIL_OutOfDate`）= 核心拿它在自己初始化时从 NVAPI 读到的显卡架构，去比 DLSS-G 运行库（snippet）导出的 `NVSDK_NGX_GetGPUArchitecture`（= 运行库声明的最低架构）后拒绝。后端把那个导出报成**物理架构**（`turing_host.gpu_architecture_export`，Turing 上 `0x160`；之前报的是 Ada `0x190`，那正是真 Turing 上帧生成打不开的原因）。`SpoofArchToGame` 管的是另一道闸门（游戏 / Streamline 自己的），对这一条没有帮助：重定向现在在游戏启动时就装好了（`trigger=dllmain`/`nvapi_load`/`dependency_load`），但核心这次比的不是 NVAPI 的答复，而是 snippet 导出的那个最低架构。 |
| `fg_gate_summary` | 一条，第一次 Evaluate 时（`reason=first_evaluate`）或进程退出时（`process_exit`）：requirements / capability / create 的次数与最后一次创建的状态。`create_calls=0` 而 `install` 一切正常 = 游戏从来没要过这个特性。 |

`python scripts\analysis\sm75_report.py <日志目录>` 的 `== fg gate ==` 一节把上面几条拼成一页，VERDICT 里多一行 `feature created`：创建成功 PASS、失败 FAIL（带状态）、从未调用 NOT RUN 并按证据给出最可能的闸门。

## 高级配置示例

以下示例仅在诊断 / 验证时用，正常安装不需要。把对应节和键补进出厂 INI 即可（`[Compatibility]` / `[Debug]` 节不存在就自己加）。

**3080 Ti 用 PTX JIT（等价于自动识别，一般不必写）：**

```ini
[Compatibility]
KernelImage=PTX
```

**RTX 5070 验证 SM86 PTX 路径：**

```ini
[Compatibility]
KernelImage=PTX
ForceSM86Route=1
SimulateAmpere=1

[Logging]
Level=3
```

**取本机原厂数值参考（stock）：** 直接用出厂 INI 把 `Optimized` 改成 `0` 即可（310.9 上不要用 `KernelImage=Original`）。

**开启生成帧标记（区分毛刺在生成帧还是真实帧）：**

```ini
[Debug]
MarkGeneratedFrames=1
```

**310.9 构建 + 6× 省掉重复真实帧拷贝：**

```ini
[FrameGeneration]
MaxGeneratedFrames=5
SkipRepeatedRealCopy=1
```

## 安装注意与卸载

根目录的四个工具类代理（`version.dll`、`winmm.dll`、`dbghelp.dll`、`dinput8.dll`）**全部复制过去即可，不用挑也不用只留一个**：内嵌的运行库和后端完全相同，只有文件名和转发目标不同，进程里第一个被加载的那个成为 active，其余自动待机只做转发（机制见上面“多个代理同时存在”）。

渲染路径代理 `alternatives/dxgi.dll`、`alternatives/d3d12.dll` 不在根目录：它们在 D3D12 热路径上、加载顺序敏感，只在上面四个都没被游戏加载时手动复制其中**一个**过去，说明随包放在 `alternatives/README.md`。

每个代理都把自己的全部导出转发给 `System32` 里的同名真实 DLL，只拦截 `nvngx_dlssg.dll` 的加载。

发布 DLL 的代码签名是可选的：`build.ps1 -SignCert <pfx> -SignPass <pw>`（或 `DLSSG_SIGN_CERT`/`DLSSG_SIGN_PASS`）会对根目录四个代理和 `alternatives\*.dll` 全部做 SHA-256 Authenticode 签名，不提供证书则产物未签名。步骤、如何造一次性测试证书、以及自签名不提供默认信任等见 `docs/SIGNING.md`。

目前针对 Windows x64 / D3D12。本包的构建检查和可选 GPU 验证状态见 `manifest.json` / `validation.json`，架构见 `docs/ARCHITECTURE.md`，抓取与重放见 `docs/CAPTURE.md`，完整记录见 `docs/VALIDATION.md`。

卸载时退出游戏，移走本包添加的**全部**代理（根目录四个，以及手动放过去的 `dxgi.dll`/`d3d12.dll`）和 INI；缓存可保留。原 NVIDIA DLL 及内核资源的归属见 `THIRD_PARTY_NOTICES.txt`；SM75 GPU 资源来自 Coldwood1026 的 RTX 20 系移植工作，在此致谢。
