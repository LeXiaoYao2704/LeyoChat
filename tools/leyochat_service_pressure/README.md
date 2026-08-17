# LeyoChatService Pressure Tool

`leyochat_service_pressure.py` 是 Phase 14 的轻量压测入口，用于两类验证：

- HTTP 服务面：heartbeat、event fetch、message send、delivery ACK、read ACK。
- P2P 控制面模型：验证 `ServerPreferred` / `ServerOnly` 下不会因为 UDP discovery 扩散成 TCP 全连接。

## P2P 控制面模型

```powershell
python tools\leyochat_service_pressure\leyochat_service_pressure.py p2p-model `
  --clients 1000 `
  --mode ServerPreferred `
  --allow-automatic-peer-connections `
  --pending-connect-limit 999
```

期望结果：

- `rawAutoConnectAttempts = 0`
- `pendingGateAcceptedAttempts = 0`
- `policyBlocksAutomaticFullMesh = true`
- `pendingGateIsOnlyProtection = false`

对照 P2POnly：

```powershell
python tools\leyochat_service_pressure\leyochat_service_pressure.py p2p-model `
  --clients 1000 `
  --mode P2POnly `
  --allow-automatic-peer-connections `
  --pending-connect-limit 64
```

该模式会暴露 `directedDiscoveryEdges = 999000` 和 `uniquePeerPairs = 499500`，说明风险来自 UDP discovery 后触发的 TCP 自动互连，而不是 UDP 包本身。

## HTTP 压测

先启动服务：

```powershell
.\LeyoChatService.exe --config C:\LeyoChatService\leyochat-service.json
```

再运行：

```powershell
python tools\leyochat_service_pressure\leyochat_service_pressure.py http `
  --base-url http://127.0.0.1:8765 `
  --token replace-with-token `
  --workspace-id team-a `
  --clients 50 `
  --concurrency 20 `
  --duration-seconds 60 `
  --conversations 10
```

输出为 JSON，核心字段：

- `operations.heartbeat.p95Ms`
- `operations.eventFetch.p95Ms`
- `operations.messageSend.p95Ms`
- `operations.deliveryAck.p95Ms`
- `operations.readAck.p95Ms`
- `failures`
- `messagesAccepted`

ACK 压测需要能代表接收方的 token。若只有单个 admin token，默认不要加 `--enable-acks`，否则 delivery/read ACK 会因为 token 不是 recipient 而被服务拒绝。

```powershell
python tools\leyochat_service_pressure\leyochat_service_pressure.py http `
  --base-url http://127.0.0.1:8765 `
  --token sender-token `
  --ack-token recipient-token `
  --workspace-id team-a `
  --clients 50 `
  --enable-acks
```

## 推荐档位

- 50 clients：小团队冒烟。
- 300 clients：部门级容量检查。
- 1000 clients：大站点服务优先模式基线。
- 3000 clients：压力边界探索，不作为单机 SQLite SLA。

每次压测都应同时记录：

- 服务进程 CPU/内存。
- SQLite DB 所在磁盘写入延迟和剩余空间。
- `/api/v1/metrics` 中的 accepted/rejected counters。
- `leyochat.service.rate_limit` 日志数量。
- `leyochat.service.message_store` 是否出现写入失败。
