#!/usr/bin/env python3

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).resolve().parent / "fixtures"
SHA256_ID = re.compile(r"^sha256:[0-9a-f]{64}$")


class AnalyticalRunContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        configured = os.environ.get("SIMAI_ANALYTICAL_BIN")
        cls.binary = (
            Path(configured).resolve()
            if configured
            else (REPO_ROOT / "bin" / "SimAI_analytical").resolve()
        )
        if not cls.binary.is_file():
            raise RuntimeError(
                "Build the real SimAI_analytical binary or set "
                "SIMAI_ANALYTICAL_BIN"
            )

    @staticmethod
    def prepare_run_directory(temp_dir):
        run_directory = Path(temp_dir)
        (run_directory / "results").mkdir()
        (run_directory / "tests").symlink_to(
            REPO_ROOT / "tests", target_is_directory=True
        )
        (run_directory / "astra-sim-alibabacloud").symlink_to(
            REPO_ROOT / "astra-sim-alibabacloud", target_is_directory=True
        )
        return run_directory

    def run_contract(self, manifest_name):
        manifest_path = FIXTURES / manifest_name
        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            result_path = run_directory / "result.json"
            completed = subprocess.run(
                [
                    str(self.binary),
                    "--run-manifest",
                    str(manifest_path),
                    "--result-manifest",
                    str(result_path),
                ],
                cwd=run_directory,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=False,
            )
            result = json.loads(result_path.read_text()) if result_path.exists() else None
        return completed, result, manifest_path

    def test_minimal_legacy_gpu_manifest_runs_real_analytical_process(self):
        completed, result, manifest_path = self.run_contract(
            "minimal_legacy_gpu_run.json"
        )

        self.assertEqual(
            completed.returncode,
            0,
            msg=f"stdout:\n{completed.stdout[-2000:]}\nstderr:\n{completed.stderr[-2000:]}",
        )
        self.assertIsInstance(result, dict)
        self.assertEqual(result["schema_version"], "simai.result/v1")
        self.assertEqual(result["run_schema_version"], "simai.run/v1")
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["reject_code"], "NONE")
        self.assertEqual(result["run_id"], "legacy-gpu-minimal")
        self.assertEqual(result["backend"], "analytical")

        input_summary = result["input_summary"]
        expected_manifest_digest = "sha256:" + hashlib.sha256(
            manifest_path.read_bytes()
        ).hexdigest()
        expected_workload_digest = "sha256:" + hashlib.sha256(
            (FIXTURES / "minimal_workload.txt").read_bytes()
        ).hexdigest()
        self.assertEqual(
            input_summary["run_manifest_sha256"], expected_manifest_digest
        )
        self.assertEqual(input_summary["workload_sha256"], expected_workload_digest)
        self.assertEqual(input_summary["accelerator"], "LEGACY_GPU")
        self.assertEqual(input_summary["gpu_count"], 1)

        provenance = result["provenance"]
        self.assertEqual(provenance["backend_binary"], "SimAI_analytical")
        self.assertEqual(provenance["cost_model"], "LEGACY_CALBUSBW")
        self.assertEqual(
            provenance["binary_sha256"],
            "sha256:" + hashlib.sha256(self.binary.read_bytes()).hexdigest(),
        )
        self.assertRegex(provenance["binary_sha256"], SHA256_ID)
        self.assertEqual(provenance["device_profile_sha256"], "UNKNOWN")

        self.assertEqual(result["evidence"]["workload"]["level"], "USER_PROVIDED")
        self.assertEqual(result["evidence"]["device_profile"]["level"], "UNKNOWN")
        self.assertEqual(result["readiness"]["contract"], "READY")
        self.assertEqual(result["readiness"]["analytical_backend"], "READY")
        self.assertEqual(result["results"]["validity"], "VALID")
        self.assertEqual(result["results"]["hbm_peak_B"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")
        self.assertEqual(result["results"]["fault_goodput_tokens_per_s"], "UNKNOWN")

        serialized = json.dumps(result, sort_keys=True)
        self.assertNotIn(str(REPO_ROOT), serialized)
        self.assertNotIn(str(manifest_path), serialized)
        self.assertNotIn("tests/contract/fixtures/minimal_workload.txt", serialized)
        self.assertIsNone(re.search(r"\b(?:\d{1,3}\.){3}\d{1,3}\b", serialized))

    def test_minimal_ascend_allreduce_uses_profiled_hccl_cost(self):
        completed, result, _ = self.run_contract(
            "minimal_ascend_allreduce_run.json"
        )

        self.assertEqual(
            completed.returncode,
            0,
            msg=f"stdout:\n{completed.stdout[-2000:]}\nstderr:\n{completed.stderr[-2000:]}",
        )
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["reject_code"], "NONE")
        self.assertEqual(result["input_summary"]["accelerator"], "ASCEND_PROFILED")

        # Independent worked example: 20,000 ns startup plus 1 MiB at
        # 25,000,000,000 B/s rounds to 61,943 ns. A four-rank ring moves
        # 2 * (4 - 1) * 1 MiB = 6,291,456 B across the whole group.
        self.assertEqual(result["results"]["timing_ns"], 61943)
        self.assertEqual(result["results"]["traffic_B"], 6291456)
        self.assertEqual(
            result["results"]["collective"],
            {
                "operation": "ALL_REDUCE",
                "message_bytes_per_rank": 1048576,
                "rank_count": 4,
                "group_type": "TP",
                "topology_domain": "HOST",
            },
        )

        provenance = result["provenance"]
        self.assertEqual(provenance["cost_model"], "HCCL_DERIVED")
        self.assertEqual(
            provenance["device_profile_sha256"],
            "sha256:"
            + hashlib.sha256(
                (FIXTURES / "minimal_ascend_profile.json").read_bytes()
            ).hexdigest(),
        )
        self.assertEqual(
            provenance["cost_model_sha256"],
            "sha256:"
            + hashlib.sha256(
                (FIXTURES / "minimal_hccl_allreduce_cost_model.json").read_bytes()
            ).hexdigest(),
        )
        self.assertEqual(
            provenance["raw_observation_sha256"],
            "sha256:"
            + hashlib.sha256(
                (FIXTURES / "minimal_hccl_allreduce_observation.json").read_bytes()
            ).hexdigest(),
        )

        evidence = result["evidence"]
        self.assertEqual(evidence["device_profile"]["level"], "USER_INPUT")
        self.assertEqual(evidence["device_profile"]["readiness"], "FIELD_UNVERIFIED")
        self.assertEqual(evidence["raw_observation"]["level"], "USER_INPUT")
        self.assertEqual(evidence["raw_observation"]["readiness"], "FIELD_UNVERIFIED")
        self.assertEqual(evidence["cost_model"]["level"], "DERIVED")
        self.assertEqual(evidence["cost_model"]["readiness"], "FIELD_UNVERIFIED")
        self.assertNotIn("MEASURED", json.dumps(evidence, sort_keys=True))

        self.assertEqual(result["readiness"]["contract"], "READY")
        self.assertEqual(result["readiness"]["ascend_profile"], "READY")
        self.assertEqual(result["readiness"]["hccl_cost_model"], "READY")
        self.assertEqual(result["readiness"]["traffic"], "READY")

    def test_hccl_model_without_ascend_profile_fails_closed(self):
        completed, result, _ = self.run_contract(
            "missing_ascend_profile_run.json"
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "ASCEND_PROFILE_REQUIRED")
        self.assertEqual(result["input_summary"]["accelerator"], "UNKNOWN")
        self.assertEqual(result["provenance"]["cost_model"], "UNKNOWN")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["ascend_profile"], "UNKNOWN")

    def test_ascend_profile_without_hccl_model_is_unsupported(self):
        completed, result, _ = self.run_contract(
            "missing_hccl_cost_model_run.json"
        )

        self.assertEqual(completed.returncode, 3)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(result["reject_code"], "HCCL_COST_MODEL_REQUIRED")
        self.assertEqual(result["provenance"]["cost_model"], "UNKNOWN")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["ascend_profile"], "BLOCKED")
        self.assertEqual(result["readiness"]["hccl_cost_model"], "BLOCKED")

    def test_ascend_collective_outside_model_domain_never_falls_back(self):
        completed, result, _ = self.run_contract(
            "out_of_domain_ascend_allreduce_run.json"
        )

        self.assertEqual(completed.returncode, 3)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(result["reject_code"], "HCCL_MODEL_DOMAIN_MISS")
        self.assertEqual(result["provenance"]["cost_model"], "UNKNOWN")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["hccl_cost_model"], "BLOCKED")

    def test_minimal_gpu_workload_keeps_legacy_cli_compatible(self):
        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            completed = subprocess.run(
                [
                    str(self.binary),
                    "-w",
                    str(FIXTURES / "minimal_workload.txt"),
                    "-g",
                    "1",
                    "-g_p_s",
                    "1",
                    "-nv",
                    "360",
                    "-nic",
                    "48.5",
                    "-n_p_s",
                    "1",
                    "-g_type",
                    "H100",
                    "-r",
                    "issue16-legacy-cli-",
                ],
                cwd=run_directory,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=False,
            )

        self.assertEqual(
            completed.returncode,
            0,
            msg=f"stdout:\n{completed.stdout[-2000:]}\nstderr:\n{completed.stderr[-2000:]}",
        )

    def test_conflicting_device_selectors_fail_closed(self):
        completed, result, manifest_path = self.run_contract(
            "conflicting_device_run.json"
        )

        self.assertEqual(completed.returncode, 2)
        self.assertIsInstance(result, dict)
        self.assertEqual(result["schema_version"], "simai.result/v1")
        self.assertEqual(result["run_schema_version"], "simai.run/v1")
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "DEVICE_SELECTOR_CONFLICT")
        self.assertEqual(result["run_id"], "device-selector-conflict")
        self.assertEqual(result["provenance"]["cost_model"], "UNKNOWN")
        self.assertEqual(
            result["input_summary"]["run_manifest_sha256"],
            "sha256:" + hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        )
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["results"]["validity"], "UNKNOWN")

        serialized = json.dumps(result, sort_keys=True)
        self.assertNotIn(str(REPO_ROOT), serialized)
        self.assertNotIn(str(manifest_path), serialized)
        self.assertNotIn("tests/contract/fixtures/minimal_workload.txt", serialized)
        self.assertIsNone(re.search(r"\b(?:\d{1,3}\.){3}\d{1,3}\b", serialized))

    def test_mismatched_workload_digest_fails_closed(self):
        completed, result, _ = self.run_contract(
            "mismatched_workload_digest_run.json"
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "WORKLOAD_DIGEST_MISMATCH")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["workload"], "BLOCKED")
        self.assertEqual(result["results"]["validity"], "UNKNOWN")

    def test_rejected_run_returns_output_error_when_result_target_is_unwritable(self):
        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            result_target = run_directory / "result-is-a-directory"
            result_target.mkdir()
            completed = subprocess.run(
                [
                    str(self.binary),
                    "--run-manifest",
                    str(FIXTURES / "conflicting_device_run.json"),
                    "--result-manifest",
                    str(result_target),
                ],
                cwd=run_directory,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=False,
            )

        self.assertEqual(completed.returncode, 4)

    def test_path_launch_hashes_the_resolved_analytical_binary(self):
        environment = os.environ.copy()
        environment["PATH"] = os.pathsep.join(
            [str(self.binary.parent), environment.get("PATH", "")]
        )
        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            result_path = run_directory / "result.json"
            completed = subprocess.run(
                [
                    self.binary.name,
                    "--run-manifest",
                    str(FIXTURES / "minimal_legacy_gpu_run.json"),
                    "--result-manifest",
                    str(result_path),
                ],
                cwd=run_directory,
                env=environment,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=False,
            )
            result = json.loads(result_path.read_text())

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(
            result["provenance"]["binary_sha256"],
            "sha256:" + hashlib.sha256(self.binary.read_bytes()).hexdigest(),
        )

    def test_external_binary_relative_launch_has_resolved_digest(self):
        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            external_binary = run_directory / "SimAI_analytical"
            shutil.copy2(self.binary, external_binary)
            expected_binary_sha256 = (
                "sha256:" + hashlib.sha256(external_binary.read_bytes()).hexdigest()
            )
            relative_binary = os.path.relpath(external_binary, run_directory)
            if os.sep not in relative_binary:
                relative_binary = "." + os.sep + relative_binary
            result_path = run_directory / "result.json"
            completed = subprocess.run(
                [
                    str(relative_binary),
                    "--run-manifest",
                    str(FIXTURES / "minimal_legacy_gpu_run.json"),
                    "--result-manifest",
                    str(result_path),
                ],
                cwd=run_directory,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=False,
            )
            result = json.loads(result_path.read_text())

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(
            result["provenance"]["binary_sha256"],
            expected_binary_sha256,
        )

    def test_same_manifest_has_deterministic_result_fields(self):
        first_completed, first_result, _ = self.run_contract(
            "minimal_legacy_gpu_run.json"
        )
        second_completed, second_result, _ = self.run_contract(
            "minimal_legacy_gpu_run.json"
        )

        self.assertEqual(
            (first_completed.returncode, second_completed.returncode), (0, 0)
        )
        self.assertEqual(first_result, second_result)


if __name__ == "__main__":
    unittest.main()
