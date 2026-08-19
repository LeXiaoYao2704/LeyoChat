# LeyoChat

[![Windows CI](https://github.com/LeXiaoYao2704/LeyoChat/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/LeXiaoYao2704/LeyoChat/actions/workflows/windows-ci.yml)
[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

[English](README.md)

LeyoChat 是一个面向 Windows 的桌面通信工具，使用 C++20、Qt 6 和 CMake
开发。它支持局域网点对点通信，也支持可选的自托管消息服务，用于持久化投递、离线同步、群聊和共享文件。

默认配置为 `P2POnly`。新客户端不内置消息服务地址或凭据；只有在用户明确配置后，客户端才会连接 LeyoChat 消息服务。

## 项目状态

- 当前源码版本：`0.4.0`
- 主要平台：Windows 10/11 x64
- 主要工具链：Visual Studio 2022 和 Qt 6.6.3
- 项目阶段：Public Preview（公开预览）

当前仓库以源码为主，尚未发布稳定版二进制安装包。请根据下面的说明从源码构建客户端或服务端。

默认构建会包含客户端、启动器、服务端、安装器目标和全部测试可执行文件，并且使用全新的构建目录验证。维护者工作站目前配置了 134 个测试，其中 133 个通过；剩余的文件传输测试可执行文件会在链接后被本机端点防护软件隔离，因此 Windows CI 会在干净的 GitHub Runner 上执行该测试。

## 功能

- 局域网发现和直接 P2P 文本消息
- 单聊和群聊
- 图片、文件、截图、回复、@ 提及、表情回应和已读回执
- 可选的持久化消息和文件服务
- 带崩溃恢复能力的客户端启动器和 Windows 服务宿主
- 可选的 ONLYOFFICE/WOPI 文档集成
- 可选的 Azure DevOps 和 Outlook/EWS 通知
- 本地 SQLite 持久化和诊断信息导出

## 路由模式

| 模式 | 行为 |
| --- | --- |
| `P2POnly` | 使用局域网发现和直接 P2P 连接，不需要消息服务。 |
| `ServerPreferred` | 服务可用时优先使用已配置的消息服务，并可根据策略回退到 P2P。 |
| `ServerOnly` | 必须使用已配置的消息服务，不使用 P2P 投递回退。 |

大型部署应避免自动建立全互联的 P2P 连接。部署规模限制和容量建议请参阅 [`docs/service-deployment.md`](docs/service-deployment.md)。

## 环境要求

- CMake 3.24 或更高版本
- 安装了 x64 C++ 工作负载的 Visual Studio 2022 Build Tools
- Qt 6.6.3 MSVC x64，包含 `Widgets`、`Network`、`Sql`、`Concurrent`、`Multimedia` 和 `Test`
- 可选 Qt 模块：
  - `HttpServer`：构建 `LeyoChatService`
  - `WebEngineWidgets`：网页文档和文件交换页面
  - `Quick` 和 `QuickControls2`：图形化安装器界面
- Inno Setup 6：构建 Windows 安装包
- Python 3 和 Pillow：仅在重新生成项目自有位图资源时需要

Qt 不包含在本仓库中，请单独安装，并将 Qt 安装前缀传给 CMake。

## 构建

打开 Visual Studio 2022 的 x64 Native Tools 命令环境，然后执行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.6.3\msvc2019_64" `
  -DLEYOCHAT_BUILD_TESTS=ON

cmake --build build --config Release --clean-first --target LeyoChat
```

快速体验局域网 P2P 时，可以在两台连接到同一可信局域网的 Windows 机器上分别启动构建出的 `LeyoChat.exe`。新配置默认使用 `P2POnly`，不需要配置消息服务。首次设置时请给两台客户端使用不同的显示名称，并保留 P2P 监听器所需的 Windows 防火墙规则。P2P 通信适用于可信网络，其他场景请先阅读 [`SECURITY.md`](SECURITY.md)。

当 Qt HttpServer 可用时，可以构建服务端组件：

```powershell
cmake --build build --config Release --clean-first `
  --target LeyoChatService LeyoChatServiceHost
```

首次配置时，CMake 会报告是否找到 `WebEngineWidgets` 和 `HttpServer`。缺少可选模块时，对应的目标或界面功能会被禁用。

## 测试

列出已配置的测试：

```powershell
ctest --test-dir build -C Release -N
```

按名称运行一个聚焦测试：

```powershell
ctest --test-dir build -C Release `
  -R LeyoChatRemoteChatServiceSettingsTests --output-on-failure
```

只有在所有需要的测试可执行文件都构建完成后，才运行完整测试集：

```powershell
cmake --build build --config Release --clean-first
ctest --test-dir build -C Release --output-on-failure
```

每个 Pull Request 都会运行路由和消息投递聚焦测试、打包检查以及文件传输测试。工作流位于 `.github/workflows/windows-ci.yml`，它有意小于完整的 134 测试矩阵；聚焦测试变绿不代表所有可选集成和全部 UI 测试都已覆盖。

## 可选消息服务

客户端只有在以下三项都存在时才会启用服务路由：

- 基础 URL，例如 `https://chat.example.com`
- 由服务端安装器生成的 Bearer 凭据
- 服务端配置允许的 Workspace ID

Windows 服务端安装器会生成随机的 64 字符凭据。不要将生成的服务配置提交到仓库，也不要在预期部署范围之外分享同一凭据。

详细部署说明请参阅 [`docs/service-deployment.md`](docs/service-deployment.md)。

## 可选文件交换页面

WebEngine 文件交换页面没有内置端点。需要使用该集成时，在启动 LeyoChat 前设置环境变量：

```powershell
$env:LEYOCHAT_FILE_EXCHANGE_URL = "https://files.example.com"
```

未设置该变量时，页面保持本地模式，并提示服务尚未配置。

## 打包

构建客户端安装包：

```powershell
.\scripts\package-installer.ps1
```

构建独立服务端安装包：

```powershell
.\scripts\package-server-installer.ps1
```

这两个单文件安装器脚本要求构建目录在配置时启用了图形化安装器：

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.6.3\msvc2019_64" `
  -DLEYOCHAT_BUILD_INSTALLER=ON
```

如果只需要构建内部 Inno Setup 安装器，也可以使用 `package-windows.ps1` 和 `package-server-windows.ps1`。

打包脚本会执行干净的目标构建，并将第三方许可证说明放入安装目录。二进制分发者仍需自行履行其 Qt 构建版本的许可证义务；如果分发 WebEngine，还要包含 Qt WebEngine 的相应声明。

## 仓库结构

```text
src/            客户端、启动器、服务端、网络、存储和 UI 代码
tests/          Qt 测试和脚本测试
third_party/    随仓库提供的第三方源码及其许可证
resources/      项目生成的背景和通话音频
stickers/       项目生成的默认贴纸包
installer/      Qt Quick 图形化安装器界面
windows/        Windows 资源和 Inno Setup 模板
scripts/        构建和打包自动化脚本
tools/          压力模型和可复现资源生成工具
docs/           服务部署和容量说明
.github/        CI、Issue 表单和贡献流程
```

默认媒体资源由 [`tools/generate_open_source_assets.py`](tools/generate_open_source_assets.py) 生成，生成器只使用几何图元和确定性参数。

## 安全

在可信网络之外暴露服务前，请先阅读 [`SECURITY.md`](SECURITY.md)。特别需要注意：当前 P2P 路径不会自动提供部署 TLS 凭据；服务端 HTTP 端点应放在 TLS 反向代理之后；部分可选集成凭据目前存储在本地 Qt 设置中。

## 许可证

LeyoChat 自有代码使用 [Apache License 2.0](LICENSE) 授权，版权归 `LeXiaoYao2704` 所有。

第三方组件继续使用各自的许可证。相关条款和来源记录在 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) 中。
