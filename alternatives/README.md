# 渲染路径代理 DLL（alternatives/）

代理 DLL 靠"游戏会加载一个和它同名的系统 DLL"来进入进程。**发布包根目录已经放了四个工具类代理**（`version.dll`、`winmm.dll`、`dbghelp.dll`、`dinput8.dll`），安装时把根目录的文件**全部**复制到渲染 EXE 旁即可，**不需要挑**：

- 哪个先被游戏加载，哪个就是本 mod（日志里 `configuration.proxies.active`）；
- 其余几个自动进入待机（`standby`），只把自己的导出原样转发给 `C:\Windows\System32\` 里的同名真实 DLL，不装任何钩子、不读 INI、不写日志；
- 因此同目录放多个代理是**正常**的，不会互相打架，也不会重复安装帧生成。

只有当**这四个都没有被游戏加载**时，才需要考虑本目录里的两个渲染路径代理。

## 本目录的两个代理（可用，但风险更高）

`dxgi.dll` 和 `d3d12.dll` 是 D3D12 渲染管线本身的入口，游戏每帧都密集调用它们，而且加载顺序敏感（游戏可能在我们的代理就位之前就已按系统路径解析了真实 DLL）。转发是完整的、功能正确，但它们在渲染热路径上，所以不随根目录一起发，需要时再手动复制：

| 名字 | 放置位置 | 说明 |
|---|---|---|
| `alternatives/dxgi.dll` | EXE 旁 | 仅当根目录四个都没被加载时使用。 |
| `alternatives/d3d12.dll` | EXE 旁 | 同上；与 `dxgi.dll` 二选一，不要同时放。 |

用完之后建议删掉，改回根目录那四个。

## 使用步骤

1. 先按发布 `README.md` 的常规步骤，把发布包**根目录的所有文件**（四个代理 + `dlssg_sm86.ini`）复制到渲染 EXE 目录。
2. 启动游戏，看日志 `dlssg_sm86\logs\loader_*.jsonl`：出现 `runtime_redirect` 即代理已生效，`configuration` 记录里的 `proxies` 写明了哪个是 active、哪些在待机（需要 `[Logging] Level=2` 才能看到）。
3. 只有一条 `configuration`/`runtime_redirect` 都没有时，才把本目录的 `dxgi.dll` 或 `d3d12.dll`（**只放一个**）也复制过去再试。

签名与信任见 `docs/SIGNING.md`。完整安装说明见根目录 `README.md`，全部 INI 键见 `docs/INSTALL.md`。
