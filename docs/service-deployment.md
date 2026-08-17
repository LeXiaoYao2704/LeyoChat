# LeyoChatService 部署说明

本文面向 Windows 单独部署的 LeyoChat Server。`LeyoChatService` 是统一后台服务，承载文件服务、OnlyOffice/WOPI、聊天文件中转和可靠消息服务；`LeyoChatServiceHost` 是 Windows 服务宿主，负责用 SCM 启动、停止并监督 `LeyoChatService`。

桌面端仍保留 P2P 能力。三种传输模式的行为不同：

- `P2POnly`：只使用 P2P，不访问消息服务。
- `ServerPreferred`：消息服务可用时优先使用服务端；服务不可用且目标 P2P 可达时回退到 P2P。
- `ServerOnly`：只使用消息服务，服务不可用时不会回退到 P2P。

新安装默认使用 `P2POnly`。没有消息服务或小团队不部署服务端时，不需要修改源码或填写一个虚构的服务地址。

## 当前架构限制

当前 LeyoChatService 是使用 SQLite 的单节点服务，不是集群或高可用服务。服务端目前的持久事件、在线状态和限流边界为：

- `IMessageEventStore`：持久消息和会话事件的追加与回放，当前由 `SqliteMessageEventStore` 实现。
- `ISessionPresenceStore`：在线会话触达和过期，当前由 `MessageSessionRegistry` 在内存中实现。
- `IRateLimitStore`：操作计数与限流决策，当前在内存中实现。

这些接口为未来接入共享 SQL、Redis 或消息队列保留了边界，但不代表当前进程可以安全地多节点运行。让两个服务实例使用各自独立的 SQLite 数据库，会割裂消息历史、回放游标、在线状态和限流状态。

增加节点前，必须为以上三个边界提供共享实现，定义事件顺序和故障恢复语义，并验证断线、重启和故障转移。持久事件 ID 必须继续作为客户端回放游标契约。

## 安装包

服务端安装包由以下脚本生成：

```powershell
.\scripts\package-server-installer.ps1
```

最终产物：

- `build-msvc\package\server-installer-output\LeyoChatServer-{版本}-setup.exe`
- `build-msvc\package\server-installer-output\server-latest.json`

`server-latest.json` 用于服务端安装包版本管理，包含：

```json
{
  "version": "0.4.0",
  "file": "LeyoChatServer-0.4.0-setup.exe",
  "sha256": "...",
  "publishedAtUtc": "2026-06-12T00:00:00Z",
  "minClientVersion": "0.4.0",
  "notes": ""
}
```

测试发布时，应同时分发安装包和 `server-latest.json`。升级器或运维脚本可以用 `sha256` 校验下载到的安装包是否完整。

## 部署方式

生产环境建议把服务端安装在一台稳定的 Windows 服务器或固定工作站上，类似企业 IM 的中心服务部署方式：服务端独立运行，客户端只配置或发现该服务地址，不再依赖某一台桌面客户端顺带提供后台能力。

安装步骤：

1. 以管理员身份运行 `LeyoChatServer-{版本}-setup.exe`。
2. 安装目录默认是 `C:\Program Files\LeyoChat Server`。
3. 安装器创建 Windows 服务 `LeyoChatService`，启动方式为自动。
4. 安装器创建防火墙规则并启动服务。
5. 安装后用健康检查确认服务可用。

健康检查：

```powershell
Invoke-RestMethod http://127.0.0.1:8765/api/v1/health
```

跨机器访问时，把 `127.0.0.1` 替换成服务器 IP 或域名。

## 目录布局

程序文件和业务数据分开存放：

| 路径 | 内容 |
|------|------|
| `C:\Program Files\LeyoChat Server` | `LeyoChatService.exe`、`LeyoChatServiceHost.exe` 和 Qt 运行时 |
| `C:\ProgramData\LeyoChat\Service\leyochat-service.json` | 服务配置文件 |
| `C:\ProgramData\LeyoChat\Service\data` | SQLite 数据库 |
| `C:\ProgramData\LeyoChat\Service\files` | 群文件、聊天附件、WOPI/OnlyOffice 文件存储 |
| `C:\ProgramData\LeyoChat\Service\logs` | 服务 stdout/stderr 日志 |

卸载默认只移除程序、Windows 服务和防火墙规则，不自动删除 `C:\ProgramData\LeyoChat\Service`，避免误删生产数据。

## 配置文件

首次安装时，安装器会要求填写 workspace 和外部访问地址，并生成 64 位随机客户端凭证；随后在缺失时创建 `C:\ProgramData\LeyoChat\Service\leyochat-service.json`。升级安装会保留已有配置，不会重新生成或覆盖凭证。

管理员应在安装页面记录外部访问地址、workspace 和客户端凭证，并将这三项提供给获授权的用户。客户端在设置页的消息服务区域填写相同内容，并选择 `ServerPreferred` 或 `ServerOnly`，即可连接自建服务。服务器地址留空并保持默认 `P2POnly` 时，客户端不会访问远程消息服务。

示例：

```json
{
  "configVersion": 1,
  "listen": {
    "port": 8765
  },
  "database": {
    "path": "C:/ProgramData/LeyoChat/Service/data/leyo-chat-service.db"
  },
  "storage": {
    "path": "C:/ProgramData/LeyoChat/Service/files"
  },
  "auth": {
    "token": "replace-with-long-random-token",
    "legacyFileAccess": false,
    "workspaces": ["team-a", "team-b"]
  },
  "sessions": {
    "ttlSeconds": 120
  },
  "rateLimit": {
    "maxRequestsPerWindow": 600,
    "windowMs": 60000
  },
  "onlyOffice": {
    "url": "http://onlyoffice.example.local",
    "jwtSecret": ""
  },
  "externalUrl": "http://leyochat-server.example.local:8765",
  "chatFiles": {
    "ttlDays": 7,
    "quotaMb": 2048
  }
}
```

生产环境建议始终使用明确 workspace allowlist。只有开发或单 workspace 试运行时才使用通配 workspace。

`auth.legacyFileAccess` 用于兼容旧 LeyoChat 客户端的群文件入口，新部署默认关闭。只有确实需要兼容旧客户端时才应显式改为 `true`；开启后，文件服务接口在 Bearer token 缺失或旧 token 失效时会以受限的旧客户端身份继续处理请求，消息服务接口仍然要求有效 Bearer token。

## 自动重启

安装后的服务链路是：

```text
Windows SCM -> LeyoChatServiceHost.exe --service -> LeyoChatService.exe --config ...
```

有两层恢复能力：

- Windows SCM 配置 `sc.exe failure`，宿主进程异常退出时自动重启服务。
- `LeyoChatServiceHost` 监督子进程，`LeyoChatService` 崩溃后按 5 秒、30 秒、60 秒的退避策略重启。

如果 5 分钟内连续崩溃次数达到阈值，宿主会停止继续拉起子进程，避免坏配置或坏数据导致无限重启。此时应查看：

- `C:\ProgramData\LeyoChat\Service\logs\LeyoChatService.stdout.log`
- `C:\ProgramData\LeyoChat\Service\logs\LeyoChatService.stderr.log`

## 升级和卸载

升级时直接运行新版本 `LeyoChatServer-{版本}-setup.exe`：

- 程序文件会覆盖到 `C:\Program Files\LeyoChat Server`。
- 现有 `leyochat-service.json` 不会被覆盖。
- `data`、`files`、`logs` 默认保留。
- 安装器会重新注册服务失败恢复策略和防火墙规则。

卸载时：

- 安装器停止并删除 `LeyoChatService`。
- 删除安装目录中的程序文件。
- 删除服务端防火墙规则。
- 默认保留 `C:\ProgramData\LeyoChat\Service`。

如需彻底清理数据，必须由运维人员在确认备份后手工删除 ProgramData 目录。

## OnlyOffice/WOPI

旧文件服务可执行程序已经不再单独交付，但文件服务逻辑没有删除。OnlyOffice、WOPI、群文件、聊天附件 TTL/配额清理等能力仍由 `LeyoChatService` 承载。

部署 OnlyOffice 时注意：

- `onlyOffice.url` 指向 Document Server 或内网代理地址。
- 如启用 JWT，`onlyOffice.jwtSecret` 必须与 Document Server 配置一致。
- `externalUrl` 必须是客户端和 OnlyOffice 都能访问到的 LeyoChat Server 地址。

## 容量与 P2P 拓扑

以下内容是容量规划指导和确定性的拓扑模型，不是公开性能基准或服务等级保证。

UDP 发现不是主要扩展风险。发现后自动建立 TCP 连接会放大握手、目录快照和在线状态流量，最终可能形成具有 `O(N^2)` 对等关系的全网状拓扑。

较大规模部署应遵循：

- `ServerPreferred` 可以更新发现状态，但不应自动连接所有发现的对端。
- `ServerOnly` 禁用 P2P 投递回退。
- 待连接数量限制只是最后一道保护，不能让全网状策略具备可扩展性。

无需网络流量即可运行拓扑模型：

```powershell
python tools\leyochat_service_pressure\leyochat_service_pressure.py p2p-model `
  --clients 1000 `
  --mode ServerPreferred `
  --allow-automatic-peer-connections `
  --pending-connect-limit 64
```

服务端感知策略应报告零次自动全网状连接尝试。`P2POnly` 只适合规模相对较小且可信的局域网。

在一次性测试服务器上运行 HTTP 压力工具：

```powershell
python tools\leyochat_service_pressure\leyochat_service_pressure.py http `
  --base-url http://127.0.0.1:8765 `
  --token replace-with-test-token `
  --workspace-id test-workspace `
  --clients 50 `
  --concurrency 20 `
  --duration-seconds 60 `
  --conversations 10
```

只使用测试凭证和合成消息。记录请求百分位、失败率、SQLite busy/写入失败、限流拒绝、进程内存、磁盘增长和活动会话数。逐步增加负载，不要仅根据拓扑模型推断生产容量。

## 运维检查

上线后至少监控：

- `/api/v1/health` 是否返回 HTTP 200。
- `LeyoChatService` Windows 服务是否处于 Running。
- `C:\ProgramData\LeyoChat\Service\data` 所在磁盘剩余空间。
- `C:\ProgramData\LeyoChat\Service\logs` 中是否出现持续启动失败、数据库打开失败或消息写入失败。
- `leyochat.service.rate_limit` 是否持续大量拒绝请求。
- OnlyOffice 在线编辑是否能打开、保存并回调。
