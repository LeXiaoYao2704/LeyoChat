# Security policy

## Reporting a vulnerability

Do not include credentials, private messages, databases, logs, or diagnostic
archives in a public issue. After the public repository is enabled, use GitHub
Private Vulnerability Reporting or a private maintainer channel.

Include the affected version, deployment mode, reproduction steps, expected
impact, and whether the issue requires an untrusted network or authenticated
account. Use synthetic data whenever possible.

## Current security boundaries

LeyoChat is not yet hardened for direct exposure to an untrusted public
network. Operators should account for these boundaries:

- A fresh client is `P2POnly` and has no embedded service endpoint or token.
- P2P TLS is unavailable unless a deployment supplies its own credential
  design. Do not assume LAN P2P traffic is confidential against a hostile
  network participant.
- LeyoChatService exposes HTTP endpoints. Put internet-facing deployments
  behind a maintained HTTPS reverse proxy and restrict inbound network access.
- The server installer generates a long random bootstrap credential. Rotate it
  after suspected disclosure and use scoped tokens for routine access.
- Outlook passwords, Azure DevOps PATs, service bearer tokens, and OnlyOffice
  secrets are currently stored in local Qt settings. Protect the Windows user
  profile and do not use high-value credentials on shared machines.
- Diagnostic bundles and logs may contain identifiers, endpoints, file names,
  and message metadata. Review them before sharing.
- OnlyOffice callbacks and external file URLs cross trust boundaries. Use HTTPS,
  a matching JWT secret, and an allowlisted externally reachable URL.

## Secret handling

- Never commit service configuration, `.env` files, certificates, private
  keys, SQLite databases, logs, or DMP files.
- Use deployment-specific credentials; do not publish a universal client token.
- Do not place secrets in command histories or issue screenshots.
- Revoke and replace a secret before reporting a leak.

## Service administration

LeyoChatService bearer tokens carry a client identity, workspace allowlist,
role, and operation scopes. The installer-generated bootstrap credential is an
administrative recovery credential; larger deployments should not use it as
the routine credential for every client.

Common scopes are:

- `admin:read`
- `admin:write`
- `audit:read`
- `token:write`
- `message:read`
- `message:write`
- `message:ack`
- `session:write`
- `events:read`
- `metrics:read`

Create least-privilege tokens through the admin API and restrict both
workspace access and operation scopes.

Administrative endpoints include:

- `POST /api/v1/admin/workspaces`
- `GET /api/v1/admin/workspaces`
- `POST /api/v1/admin/tokens`
- `GET /api/v1/admin/audit?workspaceId=...&limit=...`

Administrative endpoints require an administrative role and the matching
scope. Message, event, session, and metrics endpoints enforce their own
operation scopes.

Audit records contain actor, workspace, action, outcome, timestamp, and compact
metadata. Message body fields such as `body`, `messageBody`, and `payloadJson`
are stripped before audit metadata is stored. Operators must still protect the
audit database because identifiers and operational metadata can be sensitive.

## Supported versions

Until the first public release is tagged, security fixes are applied only to
the current development version. Older pre-release builds are not supported.
