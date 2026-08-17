import json
import pathlib
import subprocess
import sys
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL_DIR = REPO_ROOT / "tools" / "leyochat_service_pressure"
sys.path.insert(0, str(TOOL_DIR))

import leyochat_service_pressure as pressure  # noqa: E402


class TestLeyoChatServicePressureTool(unittest.TestCase):
    def test_percentile_interpolates_tail_latency(self):
        values = [1.0, 2.0, 3.0, 4.0]

        self.assertEqual(pressure.percentile(values, 0.50), 2.5)
        self.assertAlmostEqual(pressure.percentile(values, 0.95), 3.85)

    def test_message_payload_targets_next_virtual_client(self):
        payload = pressure.build_message_payload(
            "ws-1", "conv-1", virtual_client_index=0, sequence=7, participant_count=5
        )

        self.assertEqual(payload["workspaceId"], "ws-1")
        self.assertEqual(payload["conversationId"], "conv-1")
        self.assertEqual(payload["recipientIds"], ["pressure-client-1"])
        self.assertIn("pressure-0-7", payload["clientMessageId"])

    def test_server_modes_block_automatic_full_mesh_even_with_large_pending_gate(self):
        summary = pressure.simulate_p2p_control_plane(
            client_count=1000,
            mode="ServerPreferred",
            allow_automatic_peer_connections=True,
            pending_connect_limit=999,
        )

        self.assertEqual(summary["rawAutoConnectAttempts"], 0)
        self.assertEqual(summary["pendingGateAcceptedAttempts"], 0)
        self.assertTrue(summary["policyBlocksAutomaticFullMesh"])
        self.assertFalse(summary["pendingGateIsOnlyProtection"])

    def test_p2p_only_model_exposes_quadratic_connection_pressure(self):
        summary = pressure.simulate_p2p_control_plane(
            client_count=1000,
            mode="P2POnly",
            allow_automatic_peer_connections=True,
            pending_connect_limit=64,
        )

        self.assertEqual(summary["directedDiscoveryEdges"], 999000)
        self.assertEqual(summary["uniquePeerPairs"], 499500)
        self.assertEqual(summary["rawAutoConnectAttempts"], 999000)
        self.assertEqual(summary["pendingGateAcceptedAttempts"], 64000)
        self.assertTrue(summary["pendingGateIsOnlyProtection"])

    def test_p2p_model_cli_outputs_json(self):
        result = subprocess.run(
            [
                sys.executable,
                str(TOOL_DIR / "leyochat_service_pressure.py"),
                "p2p-model",
                "--clients",
                "300",
                "--mode",
                "ServerOnly",
                "--allow-automatic-peer-connections",
                "--pending-connect-limit",
                "299",
            ],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )

        payload = json.loads(result.stdout)
        self.assertEqual(payload["clientCount"], 300)
        self.assertEqual(payload["mode"], "ServerOnly")
        self.assertEqual(payload["rawAutoConnectAttempts"], 0)
        self.assertTrue(payload["policyBlocksAutomaticFullMesh"])


if __name__ == "__main__":
    unittest.main()
