#!/usr/bin/env python3
"""LeyoChatService lightweight pressure tool.

The tool has two jobs:
1. exercise the HTTP message-service surface with repeatable client loops;
2. model the P2P control-plane blast radius so service-preferred modes can be
   checked without starting thousands of desktop clients.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import math
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from typing import Any, Callable


SERVER_MODES = {"ServerPreferred", "ServerOnly"}
ALL_MODES = {"P2POnly", "ServerPreferred", "ServerOnly"}


def percentile(values: list[float], percentile_value: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    rank = (len(ordered) - 1) * percentile_value
    lower = math.floor(rank)
    upper = math.ceil(rank)
    if lower == upper:
        return ordered[int(rank)]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (rank - lower)


@dataclass
class OperationStats:
    latencies_ms: list[float] = field(default_factory=list)
    failures: int = 0

    def record(self, latency_ms: float, ok: bool) -> None:
        self.latencies_ms.append(latency_ms)
        if not ok:
            self.failures += 1

    def summary(self) -> dict[str, Any]:
        values = self.latencies_ms
        return {
            "count": len(values),
            "failures": self.failures,
            "p50Ms": round(percentile(values, 0.50), 3),
            "p95Ms": round(percentile(values, 0.95), 3),
            "p99Ms": round(percentile(values, 0.99), 3),
            "maxMs": round(max(values), 3) if values else 0.0,
        }


def simulate_p2p_control_plane(
    client_count: int,
    mode: str,
    allow_automatic_peer_connections: bool,
    pending_connect_limit: int,
) -> dict[str, Any]:
    if client_count < 0:
        raise ValueError("client_count must be non-negative")
    if mode not in ALL_MODES:
        raise ValueError(f"unsupported mode: {mode}")
    if pending_connect_limit < 0:
        raise ValueError("pending_connect_limit must be non-negative")

    directed_discovery_edges = client_count * max(0, client_count - 1)
    unique_peer_pairs = client_count * max(0, client_count - 1) // 2
    automatic_p2p_allowed = (
        mode == "P2POnly" and allow_automatic_peer_connections
    )

    raw_auto_connect_attempts = (
        directed_discovery_edges if automatic_p2p_allowed else 0
    )
    pending_gate_accepts = (
        client_count * min(max(0, client_count - 1), pending_connect_limit)
        if automatic_p2p_allowed
        else 0
    )

    return {
        "clientCount": client_count,
        "mode": mode,
        "allowAutomaticPeerConnections": allow_automatic_peer_connections,
        "pendingConnectLimit": pending_connect_limit,
        "directedDiscoveryEdges": directed_discovery_edges,
        "uniquePeerPairs": unique_peer_pairs if automatic_p2p_allowed else 0,
        "rawAutoConnectAttempts": raw_auto_connect_attempts,
        "pendingGateAcceptedAttempts": pending_gate_accepts,
        "policyBlocksAutomaticFullMesh": mode in SERVER_MODES,
        "pendingGateIsOnlyProtection": automatic_p2p_allowed,
    }


def build_message_payload(
    workspace_id: str,
    conversation_id: str,
    virtual_client_index: int,
    sequence: int,
    participant_count: int,
) -> dict[str, Any]:
    recipient_index = (virtual_client_index + 1) % max(2, participant_count)
    client_message_id = (
        f"pressure-{virtual_client_index}-{sequence}-{time.time_ns()}"
    )
    return {
        "workspaceId": workspace_id,
        "conversationId": conversation_id,
        "clientMessageId": client_message_id,
        "type": "chat_text",
        "body": f"pressure message {sequence}",
        "payload": {"source": "leyochat_service_pressure"},
        "contentType": "text/plain",
        "recipientIds": [f"pressure-client-{recipient_index}"],
    }


class ServiceHttpClient:
    def __init__(self, base_url: str, bearer_token: str, timeout_seconds: float):
        self.base_url = base_url.rstrip("/")
        self.bearer_token = bearer_token
        self.timeout_seconds = timeout_seconds

    def request(
        self,
        method: str,
        path: str,
        body: dict[str, Any] | None = None,
    ) -> tuple[int, bytes, float]:
        url = self.base_url + path
        data = None
        headers = {"Authorization": f"Bearer {self.bearer_token}"}
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        request = urllib.request.Request(url, data=data, headers=headers, method=method)
        started = time.perf_counter()
        try:
            with urllib.request.urlopen(
                request, timeout=self.timeout_seconds
            ) as response:
                payload = response.read()
                return response.status, payload, (time.perf_counter() - started) * 1000
        except urllib.error.HTTPError as error:
            payload = error.read()
            return error.code, payload, (time.perf_counter() - started) * 1000


def run_http_pressure(args: argparse.Namespace) -> dict[str, Any]:
    client = ServiceHttpClient(args.base_url, args.token, args.timeout_seconds)
    ack_client = ServiceHttpClient(
        args.base_url, args.ack_token or args.token, args.timeout_seconds
    )
    stats: dict[str, OperationStats] = {
        "heartbeat": OperationStats(),
        "eventFetch": OperationStats(),
        "messageSend": OperationStats(),
        "deliveryAck": OperationStats(),
        "readAck": OperationStats(),
    }
    message_ids: list[str] = []

    def once(index: int) -> None:
        conversation_id = f"{args.conversation_prefix}-{index % args.conversations}"
        device_id = f"pressure-device-{index}"

        status, _, elapsed = client.request(
            "POST",
            "/api/v1/sessions/heartbeat",
            {
                "workspaceId": args.workspace_id,
                "deviceId": device_id,
                "lastEventId": 0,
            },
        )
        stats["heartbeat"].record(elapsed, 200 <= status < 300)

        query = urllib.parse.urlencode(
            {
                "workspaceId": args.workspace_id,
                "deviceId": device_id,
                "afterEventId": 0,
                "limit": args.event_limit,
            }
        )
        status, _, elapsed = client.request("GET", f"/api/v1/events/stream?{query}")
        stats["eventFetch"].record(elapsed, 200 <= status < 300)

        payload = build_message_payload(
            args.workspace_id,
            conversation_id,
            index,
            int(time.time_ns()),
            args.clients,
        )
        status, body, elapsed = client.request("POST", "/api/v1/messages", payload)
        ok = 200 <= status < 300
        stats["messageSend"].record(elapsed, ok)
        if ok:
            try:
                message_id = json.loads(body.decode("utf-8")).get("serverMessageId")
            except json.JSONDecodeError:
                message_id = None
            if message_id:
                message_ids.append(message_id)

        if args.enable_acks and message_ids:
            message_id = message_ids[-1]
            status, _, elapsed = ack_client.request(
                "POST",
                f"/api/v1/messages/{urllib.parse.quote(message_id)}/delivery-ack",
                {"receivedSeq": 1},
            )
            stats["deliveryAck"].record(elapsed, 200 <= status < 300)

            status, _, elapsed = ack_client.request(
                "POST",
                f"/api/v1/messages/{urllib.parse.quote(message_id)}/read-ack",
                {"readSeq": 1},
            )
            stats["readAck"].record(elapsed, 200 <= status < 300)

    deadline = time.monotonic() + args.duration_seconds
    iteration = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        while time.monotonic() < deadline:
            futures = [pool.submit(once, i) for i in range(args.clients)]
            for future in concurrent.futures.as_completed(futures):
                future.result()
            iteration += 1
            if args.sleep_ms:
                time.sleep(args.sleep_ms / 1000.0)

    return {
        "scenario": "http",
        "baseUrl": args.base_url,
        "workspaceId": args.workspace_id,
        "clients": args.clients,
        "iterations": iteration,
        "operations": {name: value.summary() for name, value in stats.items()},
        "messagesAccepted": len(message_ids),
    }


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    http = subparsers.add_parser("http", help="run HTTP pressure loop")
    http.add_argument("--base-url", required=True)
    http.add_argument("--token", required=True)
    http.add_argument("--ack-token", default="")
    http.add_argument("--workspace-id", required=True)
    http.add_argument("--clients", type=positive_int, default=50)
    http.add_argument("--concurrency", type=positive_int, default=20)
    http.add_argument("--duration-seconds", type=positive_int, default=60)
    http.add_argument("--sleep-ms", type=int, default=0)
    http.add_argument("--timeout-seconds", type=float, default=5.0)
    http.add_argument("--conversations", type=positive_int, default=10)
    http.add_argument("--conversation-prefix", default="pressure")
    http.add_argument("--event-limit", type=positive_int, default=100)
    http.add_argument("--enable-acks", action="store_true")

    p2p = subparsers.add_parser("p2p-model", help="model P2P control-plane load")
    p2p.add_argument("--clients", type=int, required=True)
    p2p.add_argument("--mode", choices=sorted(ALL_MODES), required=True)
    p2p.add_argument("--allow-automatic-peer-connections", action="store_true")
    p2p.add_argument("--pending-connect-limit", type=int, default=64)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.command == "http":
        summary = run_http_pressure(args)
    else:
        summary = simulate_p2p_control_plane(
            client_count=args.clients,
            mode=args.mode,
            allow_automatic_peer_connections=args.allow_automatic_peer_connections,
            pending_connect_limit=args.pending_connect_limit,
        )
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
