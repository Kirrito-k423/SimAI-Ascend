#!/usr/bin/env python3

import hashlib
import copy
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import threading
import time
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).resolve().parent / "fixtures"
SHA256_ID = re.compile(r"^sha256:[0-9a-f]{64}$")
TARGET_RESOURCE_MAX_BYTES = {
    "model": 256 * 1024,
    "step": 64 * 1024,
    "routing": 64 * 1024,
    "memory_event_plan": 128 * 1024,
}


def a2_membership_digest(group):
    canonical = "{}|{}|{}|{}".format(
        group["id"],
        group["groupType"],
        ",".join(str(rank) for rank in group["members"]),
        group["topologyDigest"],
    )
    return "sha256:" + hashlib.sha256(canonical.encode()).hexdigest()


def set_all_readiness(value, readiness):
    if isinstance(value, dict):
        for key, child in value.items():
            if key == "readiness":
                value[key] = readiness
            else:
                set_all_readiness(child, readiness)
    elif isinstance(value, list):
        for child in value:
            set_all_readiness(child, readiness)


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

    def run_a2_ground_truth_contract(self, mutate_manifest=None):
        """Attach the frozen A2 GroundTruth Run/Result to a real process run."""
        return self.run_mutated_a2_ground_truth_contract(
            mutate_manifest=mutate_manifest
        )

    def run_mutated_a2_ground_truth_contract(
        self,
        *,
        mutate_profile=None,
        mutate_raw=None,
        mutate_model=None,
        mutate_run=None,
        mutate_result=None,
        mutate_manifest=None,
    ):
        """Rebind GroundTruth Run/Result digests before a real process run."""
        manifest = json.loads(
            (FIXTURES / "minimal_ascend_allreduce_run.json").read_text()
        )
        profile = json.loads(
            (FIXTURES / "a2_ascend_profile_synthetic.json").read_text()
        )
        raw = json.loads(
            (FIXTURES / "a2_hccl_ep4_observation_synthetic.json").read_text()
        )
        model = json.loads(
            (FIXTURES / "a2_hccl_ep4_cost_model_synthetic.json").read_text()
        )
        ground_truth_run = json.loads(
            (FIXTURES / "a2_ground_truth_run_synthetic.json").read_text()
        )
        ground_truth_result = json.loads(
            (FIXTURES / "a2_ground_truth_result_synthetic.json").read_text()
        )
        if mutate_profile is not None:
            mutate_profile(profile)
        with tempfile.TemporaryDirectory(prefix="simai-a2-ground-truth-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)

            profile_path = run_directory / "profile.json"
            profile_path.write_text(json.dumps(profile, indent=2) + "\n")
            profile_digest = "sha256:" + hashlib.sha256(
                profile_path.read_bytes()
            ).hexdigest()

            raw["spec"]["profileRef"] = profile["metadata"]["id"]
            raw["spec"]["profileDigest"] = profile_digest
            if mutate_raw is not None:
                mutate_raw(raw)
            raw_path = run_directory / "raw.json"
            raw_path.write_text(json.dumps(raw, indent=2) + "\n")
            raw_digest = "sha256:" + hashlib.sha256(raw_path.read_bytes()).hexdigest()

            model["spec"]["profileDigest"] = profile_digest
            model["spec"]["inputSamples"][0]["path"] = str(raw_path)
            model["spec"]["inputSamples"][0]["sha256"] = raw_digest
            if mutate_model is not None:
                mutate_model(model)
            model_path = run_directory / "model.json"
            model_path.write_text(json.dumps(model, indent=2) + "\n")
            model_digest = "sha256:" + hashlib.sha256(
                model_path.read_bytes()
            ).hexdigest()

            profile_evidence = profile["spec"]["evidence"][0]
            profile_binding = ground_truth_run["spec"]["bindings"]["profile"]
            profile_binding.update(
                {
                    "id": profile["metadata"]["id"],
                    "sha256": profile_digest,
                    "evidenceClass": profile_evidence["class"],
                    "readiness": profile["spec"]["identity"][
                        "physicalChipCount"
                    ]["readiness"],
                    "evidenceRef": profile_evidence["id"],
                }
            )
            if mutate_run is not None:
                mutate_run(ground_truth_run)
            ground_truth_run_path = run_directory / "ground-truth-run.json"
            ground_truth_run_path.write_text(
                json.dumps(ground_truth_run, indent=2) + "\n"
            )
            ground_truth_run_digest = "sha256:" + hashlib.sha256(
                ground_truth_run_path.read_bytes()
            ).hexdigest()
            ground_truth_result["spec"][
                "groundTruthRunDigest"
            ] = ground_truth_run_digest
            for scenario in ground_truth_result["spec"]["scenarios"]:
                scenario["provenanceDigest"] = ground_truth_run_digest
            ground_truth_result["spec"]["rawObservations"] = [
                {"path": str(raw_path), "sha256": raw_digest}
            ]
            ground_truth_result["spec"]["derivedCostModel"] = {
                "path": str(model_path),
                "sha256": model_digest,
            }
            ground_truth_result["spec"]["bindings"] = copy.deepcopy(
                ground_truth_run["spec"]["bindings"]
            )
            if mutate_result is not None:
                mutate_result(ground_truth_result, ground_truth_run_digest)
            ground_truth_result_path = run_directory / "ground-truth-result.json"
            ground_truth_result_path.write_text(
                json.dumps(ground_truth_result, indent=2) + "\n"
            )
            ground_truth_result_digest = "sha256:" + hashlib.sha256(
                ground_truth_result_path.read_bytes()
            ).hexdigest()
            manifest["a2_ground_truth"] = {
                "schema_version": "simai.a2.calibration/v1",
                "run": {
                    "path": str(ground_truth_run_path),
                    "sha256": ground_truth_run_digest,
                },
                "result": {
                    "path": str(ground_truth_result_path),
                    "sha256": ground_truth_result_digest,
                },
            }
            manifest["workload"]["path"] = str(
                FIXTURES / "a2_ascend_ep4_allreduce_workload.txt"
            )
            manifest["workload"]["sha256"] = (
                "sha256:2116c57e51c2a1bf286f13a738c027e4e07f76a3b8b38d884792fbcc0f31f7ec"
            )
            manifest["device_profile"] = {
                "path": str(profile_path),
                "sha256": profile_digest,
            }
            manifest["collective_cost_model"] = {
                "path": str(model_path),
                "sha256": model_digest,
            }
            if mutate_manifest is not None:
                mutate_manifest(manifest)
            manifest_path = run_directory / "run.json"
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
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
            result = json.loads(result_path.read_text())
        return completed, result

    def run_mutated_ascend_contract(
        self, *, mutate_profile=None, mutate_model=None, mutate_raw=None
    ):
        """Run a content-addressed mutation through the public process seam."""
        profile = json.loads(
            (FIXTURES / "minimal_ascend_profile.json").read_text()
        )
        model = json.loads(
            (FIXTURES / "minimal_hccl_allreduce_cost_model.json").read_text()
        )
        raw = json.loads(
            (FIXTURES / "minimal_hccl_allreduce_observation.json").read_text()
        )
        manifest = json.loads(
            (FIXTURES / "minimal_ascend_allreduce_run.json").read_text()
        )

        if mutate_profile is not None:
            mutate_profile(profile)

        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            profile_path = run_directory / "profile.json"
            profile_path.write_text(json.dumps(profile, indent=2) + "\n")
            profile_digest = "sha256:" + hashlib.sha256(
                profile_path.read_bytes()
            ).hexdigest()

            raw["spec"]["profileDigest"] = profile_digest
            if mutate_raw is not None:
                mutate_raw(raw)
            raw_path = run_directory / "raw.json"
            raw_path.write_text(json.dumps(raw, indent=2) + "\n")
            raw_digest = "sha256:" + hashlib.sha256(raw_path.read_bytes()).hexdigest()

            model["spec"]["profileDigest"] = profile_digest
            model["spec"]["inputSamples"][0]["path"] = str(raw_path)
            model["spec"]["inputSamples"][0]["sha256"] = raw_digest
            if mutate_model is not None:
                mutate_model(model)
            model_path = run_directory / "model.json"
            model_path.write_text(json.dumps(model, indent=2) + "\n")
            model_digest = "sha256:" + hashlib.sha256(
                model_path.read_bytes()
            ).hexdigest()

            manifest["workload"]["path"] = str(
                FIXTURES / "ascend_allreduce_workload.txt"
            )
            manifest["device_profile"] = {
                "path": str(profile_path),
                "sha256": profile_digest,
            }
            manifest["collective_cost_model"] = {
                "path": str(model_path),
                "sha256": model_digest,
            }
            manifest_path = run_directory / "run.json"
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
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
            result = json.loads(result_path.read_text())
        return completed, result

    def run_a2_typed_test(self, source_name):
        """Compile and execute the typed A2 validator seam."""
        source_path = Path(__file__).resolve().parent / source_name
        implementation = (
            REPO_ROOT
            / "astra-sim-alibabacloud/astra-sim/network_frontend/analytical/A2GroundTruth.cc"
        )
        with tempfile.TemporaryDirectory(prefix="simai-a2-typed-") as temp_dir:
            binary = Path(temp_dir) / "typed-test"
            compiled = subprocess.run(
                [
                    os.environ.get("CXX", "c++"),
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Wpedantic",
                    "-Werror",
                    "-I",
                    str(REPO_ROOT / "astra-sim-alibabacloud"),
                    str(source_path),
                    str(implementation),
                    "-o",
                    str(binary),
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            executed = None
            if compiled.returncode == 0:
                executed = subprocess.run(
                    [str(binary)],
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    check=False,
                )
        return compiled, executed

    def test_a2_ground_truth_statistics_and_model_are_consumed_by_real_process(self):
        completed, result = self.run_a2_ground_truth_contract()

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(result["status"], "VALID")
        calibration = result["results"]["a2_ground_truth"]
        self.assertEqual(calibration["status"], "VALID")
        self.assertFalse(calibration["calibration_eligible"])
        self.assertEqual(calibration["evidence"], "USER_INPUT")
        self.assertEqual(calibration["raw_observation_count"], 1)
        self.assertRegex(calibration["derived_cost_model_sha256"], SHA256_ID)
        scenarios = {scenario["id"]: scenario for scenario in calibration["scenarios"]}
        self.assertEqual(scenarios["A2-CAL-BALANCED"]["sample_count"], 5)
        self.assertEqual(
            scenarios["A2-CAL-BALANCED"]["representative_statistic"],
            "MEDIAN",
        )
        self.assertEqual(
            scenarios["A2-CAL-BALANCED"]["representative_step_time_ns"],
            100000000,
        )
        self.assertEqual(scenarios["A2-CAL-COMM"]["sample_count"], 10)
        self.assertEqual(
            scenarios["A2-CAL-COMM"]["representative_statistic"],
            "LINEAR_TYPE7_P90",
        )
        self.assertEqual(
            scenarios["A2-CAL-COMM"]["representative_step_time_ns"],
            141000000,
        )
        self.assertEqual(result["results"]["timing_ns"], 61943)

    def test_a2_ground_truth_cv_rule_requires_exactly_five_or_ten_samples(self):
        def mutate_short_result(document, _run_digest):
            scenario = document["spec"]["scenarios"][0]
            scenario["stepTimeNs"] = scenario["stepTimeNs"][:4]
            scenario["peakHbmB"] = scenario["peakHbmB"][:4]

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=mutate_short_result
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"], "A2_GROUND_TRUTH_SAMPLE_COUNT_INVALID"
        )

        def mutate_result(document, _run_digest):
            document["spec"]["scenarios"][1]["stepTimeNs"] = [
                100000000,
                101000000,
                99000000,
                100500000,
                99500000,
                100000000,
                101000000,
                99000000,
                100500000,
                99500000,
            ]

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=mutate_result
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"], "A2_GROUND_TRUTH_SAMPLE_RULE_VIOLATION"
        )

    def test_a2_invalid_accuracy_executions_never_enter_calibration(self):
        def scenario(document):
            return document["spec"]["scenarios"][0]

        mutations = {
            "oom": (
                lambda document, _digest: scenario(document).__setitem__(
                    "oom", True
                ),
                "A2_OOM",
            ),
            "hbm_at_85_percent": (
                lambda document, _digest: scenario(document)[
                    "peakHbmB"
                ].__setitem__(0, 58411555226),
                "A2_HBM_LIMIT_REACHED",
            ),
            "rank_loss": (
                lambda document, _digest: scenario(document).__setitem__(
                    "completedRanks", 7
                ),
                "A2_RANK_LOSS",
            ),
            "non_finite": (
                lambda document, _digest: scenario(document).__setitem__(
                    "lossFinite", False
                ),
                "A2_NON_FINITE",
            ),
            "token_loss": (
                lambda document, _digest: scenario(document).__setitem__(
                    "droppedTokens", 1
                ),
                "A2_TOKEN_LOSS",
            ),
            "token_replay": (
                lambda document, _digest: scenario(document).__setitem__(
                    "replayedTokens", 1
                ),
                "A2_TOKEN_REPLAY",
            ),
            "provenance_drift": (
                lambda document, _digest: scenario(document).__setitem__(
                    "provenanceDigest",
                    "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
                ),
                "A2_PROVENANCE_DRIFT",
            ),
        }
        for name, (mutation, expected_code) in mutations.items():
            with self.subTest(name=name):
                completed, result = self.run_mutated_a2_ground_truth_contract(
                    mutate_result=mutation
                )
                self.assertEqual(completed.returncode, 5)
                self.assertEqual(result["status"], "INVALID_ACCURACY_EXECUTION")
                self.assertEqual(result["reject_code"], expected_code)
                self.assertNotEqual(
                    result["readiness"]["a2_ground_truth"], "READY"
                )

    def test_a2_blocked_environment_has_actionable_remediation_and_no_fit(self):
        def mutate_result(document, _run_digest):
            document["spec"]["status"] = "BLOCKED_ENV"
            document["spec"]["block"] = {
                "reason": "A2_HCCL_ABI_UNAVAILABLE",
                "remediation": "Install the pinned CANN 8.5 HCCL runtime in the isolated lane.",
            }
            document["spec"]["rawObservations"] = []
            document["spec"]["derivedCostModel"] = None
            document["spec"]["scenarios"] = []

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=mutate_result
        )

        self.assertEqual(completed.returncode, 6)
        self.assertEqual(result["status"], "BLOCKED_ENV")
        self.assertEqual(result["reject_code"], "A2_HCCL_ABI_UNAVAILABLE")
        self.assertIn("CANN 8.5 HCCL", result["remediation"])
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["readiness"]["a2_ground_truth"], "BLOCKED")

    def test_a2_source_identity_and_model_raw_digest_closure_fail_closed(self):
        def drift_source(document):
            document["spec"]["sourceIdentity"]["mindSpeedLlmCommit"] = "0" * 40

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_run=drift_source
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "A2_GROUND_TRUTH_RUN_INVALID")

        def drift_raw(document, _run_digest):
            document["spec"]["rawObservations"][0]["sha256"] = (
                "sha256:" + "d" * 64
            )

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=drift_raw
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "A2_RAW_OBSERVATION_DIGEST_MISMATCH"
        )

        def drift_model(document, _run_digest):
            document["spec"]["derivedCostModel"]["sha256"] = (
                "sha256:" + "e" * 64
            )

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=drift_model
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "A2_DERIVED_MODEL_BINDING_MISMATCH"
        )

    def test_a2_measured_claim_cannot_upgrade_unverified_raw_or_model_evidence(self):
        def claim_measured(document, _run_digest):
            evidence = document["spec"]["evidence"]
            evidence["class"] = "MEASURED"
            evidence["readiness"] = "FIELD_VERIFIED"
            evidence["conditions"]["hardwareAvailable"] = True

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=claim_measured
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        calibration = result["results"]["a2_ground_truth"]
        self.assertFalse(calibration["calibration_eligible"])
        self.assertEqual(
            result["evidence"]["raw_observation"]["readiness"],
            "FIELD_UNVERIFIED",
        )
        self.assertEqual(
            result["evidence"]["cost_model"]["readiness"],
            "FIELD_UNVERIFIED",
        )

    def test_a2_verified_evidence_chain_can_enter_calibration(self):
        def verify_profile(document):
            set_all_readiness(document, "FIELD_VERIFIED")
            evidence = document["spec"]["evidence"][0]
            evidence["class"] = "MEASURED"
            evidence["conditions"]["hardwareAvailable"] = True

        def verify_raw(document):
            set_all_readiness(document, "FIELD_VERIFIED")
            document["spec"]["evidenceClass"] = "MEASURED"
            evidence = document["spec"]["evidence"][0]
            evidence["class"] = "MEASURED"
            evidence["conditions"]["hardwareAvailable"] = True

        def verify_model(document):
            set_all_readiness(document, "FIELD_VERIFIED")
            evidence = document["spec"]["evidence"][0]
            evidence["conditions"]["hardwareAvailable"] = True

        def verify_run(document):
            evidence = document["spec"]["evidence"]
            evidence["readiness"] = "FIELD_VERIFIED"
            evidence["conditions"]["hardwareAvailable"] = True

        def verify_result(document, _run_digest):
            evidence = document["spec"]["evidence"]
            evidence["class"] = "MEASURED"
            evidence["readiness"] = "FIELD_VERIFIED"
            evidence["conditions"]["hardwareAvailable"] = True

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_profile=verify_profile,
            mutate_raw=verify_raw,
            mutate_model=verify_model,
            mutate_run=verify_run,
            mutate_result=verify_result,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertTrue(result["results"]["a2_ground_truth"]["calibration_eligible"])
        self.assertEqual(
            result["evidence"]["device_profile"]["readiness"],
            "FIELD_VERIFIED",
        )
        self.assertEqual(
            result["evidence"]["raw_observation"]["level"], "MEASURED"
        )
        self.assertEqual(
            result["evidence"]["cost_model"]["readiness"], "FIELD_VERIFIED"
        )

    def test_a2_profile_uses_consumed_evidence_not_unreferenced_first_record(self):
        def poison_profile_evidence_order(document):
            set_all_readiness(document, "FIELD_VERIFIED")
            first = document["spec"]["evidence"][0]
            first["class"] = "MEASURED"
            first["readiness"] = "FIELD_VERIFIED"
            first["conditions"]["hardwareAvailable"] = True
            referenced = copy.deepcopy(first)
            referenced["id"] = "a2-profile-referenced-user-input"
            referenced["class"] = "USER_INPUT"
            referenced["readiness"] = "FIELD_UNVERIFIED"
            referenced["source"]["ref"] = "test-only-referenced-user-input"
            referenced["conditions"]["hardwareAvailable"] = False
            document["spec"]["evidence"].append(referenced)

            def bind_consumed_fields(value):
                if isinstance(value, dict):
                    for key, child in value.items():
                        if key == "evidenceRef":
                            value[key] = referenced["id"]
                        else:
                            bind_consumed_fields(child)
                elif isinstance(value, list):
                    for child in value:
                        bind_consumed_fields(child)

            for key in (
                "identity",
                "rankGranularity",
                "compute",
                "memory",
                "topology",
            ):
                bind_consumed_fields(document["spec"][key])

        def verify_raw(document):
            set_all_readiness(document, "FIELD_VERIFIED")
            document["spec"]["evidenceClass"] = "MEASURED"
            evidence = document["spec"]["evidence"][0]
            evidence["class"] = "MEASURED"
            evidence["conditions"]["hardwareAvailable"] = True

        def verify_model(document):
            set_all_readiness(document, "FIELD_VERIFIED")
            document["spec"]["evidence"][0]["conditions"][
                "hardwareAvailable"
            ] = True

        def verify_run(document):
            evidence = document["spec"]["evidence"]
            evidence["readiness"] = "FIELD_VERIFIED"
            evidence["conditions"]["hardwareAvailable"] = True

        def verify_result(document, _run_digest):
            evidence = document["spec"]["evidence"]
            evidence["class"] = "MEASURED"
            evidence["readiness"] = "FIELD_VERIFIED"
            evidence["conditions"]["hardwareAvailable"] = True

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_profile=poison_profile_evidence_order,
            mutate_raw=verify_raw,
            mutate_model=verify_model,
            mutate_run=verify_run,
            mutate_result=verify_result,
        )

        self.assertEqual(completed.returncode, 2, completed.stderr)
        self.assertFalse(
            result.get("results", {})
            .get("a2_ground_truth", {})
            .get("calibration_eligible", False)
        )
        self.assertEqual(
            result["reject_code"], "DEVICE_PROFILE_FIELD_EVIDENCE_INVALID"
        )

    def test_a2_profile_v02_evidence_index_rejects_unknown_keys(self):
        def add_unknown_evidence_key(document):
            document["spec"]["evidence"][0]["unexpectedRawHostLog"] = True

        def add_unknown_source_key(document):
            document["spec"]["evidence"][0]["source"]["unexpectedHost"] = True

        for mutation in (add_unknown_evidence_key, add_unknown_source_key):
            with self.subTest(mutation=mutation.__name__):
                completed, result = self.run_mutated_a2_ground_truth_contract(
                    mutate_profile=mutation
                )
                self.assertEqual(completed.returncode, 2, completed.stderr)
                self.assertEqual(
                    result["reject_code"],
                    "DEVICE_PROFILE_FIELD_EVIDENCE_INVALID",
                )

    def test_a2_raw_group_identity_must_match_model_and_run(self):
        def drift_raw(document):
            document["spec"]["group"]["id"] = "wrong-ep-group"

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_raw=drift_raw
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "RAW_OBSERVATION_GROUP_INVALID")

    def test_a2_raw_group_rank_count_must_match_membership(self):
        def drift_raw(document):
            document["spec"]["group"]["rankCount"] = 3

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_raw=drift_raw
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "RAW_OBSERVATION_GROUP_INVALID")

    def test_a2_raw_group_members_must_be_legal_world8_ranks(self):
        def drift_raw(document):
            group = document["spec"]["group"]
            group["members"] = [0, 1, 2, 8]
            group["membershipDigest"] = a2_membership_digest(group)

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_raw=drift_raw
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "RAW_OBSERVATION_GROUP_INVALID")

    def test_a2_model_group_membership_must_match_run(self):
        def drift_model(document):
            group = document["spec"]["groupDomain"]
            group["memberRanks"] = [[0, 1, 2, 8]]
            canonical = {
                "id": group["groupIds"][0],
                "groupType": group["groupTypes"][0],
                "members": group["memberRanks"][0],
                "topologyDigest": group["topologyDigests"][0],
            }
            group["membershipDigests"] = [a2_membership_digest(canonical)]

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_model=drift_model
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "HCCL_COST_MODEL_GROUP_INVALID")

    def test_a2_profile_binding_must_match_selected_profile(self):
        def drift_run(document):
            document["spec"]["bindings"]["profile"]["sha256"] = (
                "sha256:" + "c" * 64
            )

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_run=drift_run
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "A2_PROFILE_BINDING_MISMATCH")

    def test_a2_workload_binding_must_match_selected_workload(self):
        def drift_run(document):
            document["spec"]["bindings"]["workload"]["sha256"] = (
                "sha256:" + "d" * 64
            )

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_run=drift_run
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "A2_WORKLOAD_BINDING_MISMATCH")

    def test_a2_declared_invalid_accuracy_preserves_subtype_and_has_no_fit(self):
        def declare_oom(document, _run_digest):
            document["spec"]["status"] = "INVALID_ACCURACY_EXECUTION"
            document["spec"]["block"] = {
                "reason": "A2_OOM",
                "remediation": "Correct the execution and repeat the unchanged frozen scenario.",
            }
            document["spec"]["rawObservations"] = []
            document["spec"]["derivedCostModel"] = None
            document["spec"]["scenarios"] = []

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=declare_oom
        )
        self.assertEqual(completed.returncode, 5)
        self.assertEqual(result["status"], "INVALID_ACCURACY_EXECUTION")
        self.assertEqual(result["reject_code"], "A2_OOM")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_a2_malformed_scenario_numeric_is_invalid_input(self):
        def malformed(document, _run_digest):
            document["spec"]["scenarios"][0]["stepTimeNs"][0] = 1.5

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=malformed
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "A2_GROUND_TRUTH_SCENARIO_INVALID")

    def test_a2_envelope_reference_rejects_unknown_keys(self):
        def add_unknown(document):
            document["a2_ground_truth"]["run"]["unexpectedRawHostLog"] = "forbidden"

        completed, result = self.run_a2_ground_truth_contract(
            mutate_manifest=add_unknown
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "A2_GROUND_TRUTH_RUN_REFERENCE_INVALID"
        )

    def test_a2_result_evidence_rejects_unknown_raw_host_log_key(self):
        def add_unknown(document, _run_digest):
            document["spec"]["evidence"]["source"][
                "unexpectedRawHostLog"
            ] = "forbidden"

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_result=add_unknown
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "A2_GROUND_TRUTH_RESULT_INVALID")

    def test_a2_verified_raw_schema_rejects_unknown_nested_keys(self):
        def add_unknown(document):
            document["spec"]["evidence"][0]["source"][
                "unexpectedRawHostLog"
            ] = "forbidden"

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_raw=add_unknown
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "RAW_OBSERVATION_SCHEMA_INVALID")

    def test_a2_verified_model_schema_rejects_unknown_nested_keys(self):
        def add_unknown(document):
            document["spec"]["groupDomain"]["unexpectedRawHostLog"] = "forbidden"

        completed, result = self.run_mutated_a2_ground_truth_contract(
            mutate_model=add_unknown
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "HCCL_COST_MODEL_SCHEMA_INVALID")

    def test_a2_typed_validator_rejects_zero_and_uint64_hbm_overflow(self):
        compiled, executed = self.run_a2_typed_test(
            "a2_ground_truth_typed_numeric_test.cc"
        )
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        self.assertIsNotNone(executed)
        self.assertEqual(executed.returncode, 0, executed.stderr)

    def test_a2_typed_validator_rejects_nonfinite_step_time(self):
        compiled, executed = self.run_a2_typed_test(
            "a2_ground_truth_typed_nonfinite_test.cc"
        )
        self.assertNotEqual(compiled.returncode, 0)
        self.assertIn("narrow", compiled.stderr.lower())
        self.assertIsNone(executed)

    def test_a2_type7_p90_preserves_uint64_maximum(self):
        compiled, executed = self.run_a2_typed_test(
            "a2_ground_truth_type7_uint64_test.cc"
        )
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        self.assertIsNotNone(executed)
        self.assertEqual(executed.returncode, 0, executed.stderr)

    def run_mutated_target_contract(
        self,
        *,
        mutate_model=None,
        mutate_step=None,
        mutate_routing=None,
        mutate_memory=None,
        mutate_manifest=None,
        mutate_workload=None,
        workload_layer_count=1,
        target_workload_format="STANDARD",
        target_specific_parallelism="MODEL",
        target_forward_collective="NONE",
        target_forward_bytes=0,
        target_weight_collective="NONE",
        target_weight_bytes=0,
        ascend_profiled=False,
        artifact_sizes=None,
        raw_replacements=None,
        stdin_resource=None,
        atomic_workload_replacement=None,
    ):
        """Rebind all Target Workload digests, then run the real process."""
        model = json.loads(
            (FIXTURES / "target_10t_model_manifest.json").read_text()
        )
        step = json.loads(
            (FIXTURES / "target_500m_step_manifest.json").read_text()
        )
        routing = json.loads(
            (FIXTURES / "target_hash_routing_artifact.json").read_text()
        )
        memory = json.loads(
            (FIXTURES / "target_symbolic_memory_event_plan.json").read_text()
        )
        manifest = json.loads(
            (FIXTURES / "target_10t_symbolic_run.json").read_text()
        )
        if mutate_model is not None:
            mutate_model(model)

        with tempfile.TemporaryDirectory(prefix="simai-target-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            artifact_sizes = artifact_sizes or {}
            raw_replacements = raw_replacements or {}
            artifact_contents = {}

            def write_artifact(resource_name, name, document):
                path = run_directory / name
                content = json.dumps(document, indent=2) + "\n"
                for original, replacement in raw_replacements.get(
                    resource_name, ()
                ):
                    if content.count(original) != 1:
                        raise AssertionError(
                            f"expected one {resource_name} raw token {original!r}"
                        )
                    content = content.replace(original, replacement, 1)
                requested_size = artifact_sizes.get(resource_name)
                if requested_size is not None:
                    padding = requested_size - len(content.encode())
                    if padding < 0:
                        raise AssertionError(
                            f"{resource_name} fixture exceeds requested size"
                        )
                    content = " " * padding + content
                path.write_text(content)
                artifact_contents[resource_name] = content
                digest = "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
                return path, digest

            model_path, model_digest = write_artifact("model", "model.json", model)
            step["spec"]["modelDigest"] = model_digest
            if mutate_step is not None:
                mutate_step(step)
            step_path, step_digest = write_artifact("step", "step.json", step)

            routing["spec"]["modelDigest"] = model_digest
            routing["spec"]["stepDigest"] = step_digest
            if mutate_routing is not None:
                mutate_routing(routing)
            routing_path, routing_digest = write_artifact(
                "routing", "routing.json", routing
            )

            memory["spec"]["modelDigest"] = model_digest
            memory["spec"]["stepDigest"] = step_digest
            memory["spec"]["routingDigest"] = routing_digest
            if mutate_memory is not None:
                mutate_memory(memory)
            memory_path, memory_digest = write_artifact(
                "memory_event_plan", "memory.json", memory
            )

            resource_digests = (
                model_digest,
                step_digest,
                routing_digest,
                memory_digest,
            )
            composite_digest = "sha256:" + hashlib.sha256(
                "\n".join(resource_digests).encode()
            ).hexdigest()
            target = manifest["target_workload"]
            target["sha256"] = composite_digest
            for key, path, digest in (
                ("model", model_path, model_digest),
                ("step", step_path, step_digest),
                ("routing", routing_path, routing_digest),
                ("memory_event_plan", memory_path, memory_digest),
            ):
                target[key] = {
                    "path": "/dev/stdin" if key == stdin_resource else str(path),
                    "sha256": digest,
                }
            binding_values = (
                model_digest,
                step_digest,
                routing_digest,
                memory_digest,
                composite_digest,
            )
            workload_document = {
                "header": (
                    ("HYBRID_CUSTOMIZED" if target_workload_format == "CUSTOMIZED"
                     else "HYBRID_TRANSFORMER")
                    + " model_parallel_NPU_group: "
                    + ("4" if ascend_profiled else "1")
                    + " ep: 1 pp: 1 vpp: 1 ga: 1 all_gpus: "
                    + ("4" if ascend_profiled else "1")
                    + " checkpoints: 0 "
                    "checkpoint_initiates: 0 pp_comm 0 "
                    "target_model_sha256: " + model_digest + " "
                    "target_step_sha256: " + step_digest + " "
                    "target_routing_sha256: " + routing_digest + " "
                    "target_memory_event_plan_sha256: " + memory_digest + " "
                    "target_workload_sha256: " + composite_digest
                ),
                "layers": [
                    (
                        f"target_layer_{layer}\t-1\t10"
                        f"\t{target_forward_collective}"
                        f"\t{target_forward_bytes}\t10\tNONE\t0"
                        f"\t10\t{target_weight_collective}"
                        f"\t{target_weight_bytes}\t10\t"
                        + (
                            target_specific_parallelism + "\t"
                            if target_workload_format == "CUSTOMIZED"
                            else ""
                        )
                        + "\t".join(binding_values)
                    )
                    for layer in range(workload_layer_count)
                ],
            }
            if mutate_workload is not None:
                mutate_workload(workload_document)
            workload_path = run_directory / "target-workload.txt"
            workload_path.write_text(
                workload_document["header"]
                + "\n"
                + str(len(workload_document["layers"]))
                + "\n"
                + "\n".join(workload_document["layers"])
                + "\n"
            )
            manifest["workload"]["path"] = str(workload_path)
            manifest["workload"]["sha256"] = "sha256:" + hashlib.sha256(
                workload_path.read_bytes()
            ).hexdigest()
            manifest["workload"]["target_workload_sha256"] = composite_digest
            if ascend_profiled:
                manifest.pop("legacy_gpu", None)
                for key, fixture_name in (
                    ("device_profile", "minimal_ascend_profile.json"),
                    (
                        "collective_cost_model",
                        "minimal_hccl_allreduce_cost_model.json",
                    ),
                ):
                    fixture_path = FIXTURES / fixture_name
                    manifest[key] = {
                        "path": str(fixture_path),
                        "sha256": "sha256:" + hashlib.sha256(
                            fixture_path.read_bytes()
                        ).hexdigest(),
                    }
            if mutate_manifest is not None:
                mutate_manifest(manifest)

            replacement_thread = None
            replacement_errors = []
            if atomic_workload_replacement is not None:
                replacement_document = copy.deepcopy(workload_document)
                atomic_workload_replacement(replacement_document)
                replacement_content = (
                    replacement_document["header"]
                    + "\n"
                    + str(len(replacement_document["layers"]))
                    + "\n"
                    + "\n".join(replacement_document["layers"])
                    + "\n"
                )
                replacement_path = run_directory / "replacement-workload.txt"
                replacement_path.write_text(replacement_content)
                model_fifo = run_directory / "model-read-barrier.fifo"
                os.mkfifo(model_fifo)
                target["model"]["path"] = str(model_fifo)

                def replace_after_model_reader_opens():
                    try:
                        with model_fifo.open("w") as pipe:
                            os.replace(replacement_path, workload_path)
                            pipe.write(artifact_contents["model"])
                    except BaseException as error:
                        replacement_errors.append(error)

                replacement_thread = threading.Thread(
                    target=replace_after_model_reader_opens,
                    daemon=True,
                )
                replacement_thread.start()
            manifest_path, _ = write_artifact("run", "run.json", manifest)
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
                input=(
                    artifact_contents[stdin_resource]
                    if stdin_resource is not None
                    else None
                ),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=False,
            )
            if replacement_thread is not None:
                replacement_thread.join(timeout=5)
                if replacement_thread.is_alive():
                    self.fail("model-read barrier writer did not finish")
                if replacement_errors:
                    raise replacement_errors[0]
            result = json.loads(result_path.read_text())
        return completed, result

    @staticmethod
    def materialize_memory_plan(
        memory,
        *,
        base_hbm_B,
        reserve_hbm_B,
        component_values,
        observed_execution_peak_B="UNKNOWN",
    ):
        for index, binding_name in enumerate(
            ("precision", "optimizer", "placement", "recomputation", "runtime"),
            start=1,
        ):
            memory["spec"]["bindings"][binding_name] = {
                "state": "BOUND",
                "sha256": "sha256:" + str(index) * 64,
            }
        for component in memory["spec"]["components"]:
            component["peakBytes"] = component_values[component["category"]]
        planned_peak = sum(component_values.values())
        memory["spec"]["capacity"] = {
            "baseHbmB": base_hbm_B,
            "reserveHbmB": reserve_hbm_B,
            "scenarioUsableHbmB": base_hbm_B - reserve_hbm_B,
            "plannedPeakHbmB": planned_peak,
            "observedExecutionPeakHbmB": observed_execution_peak_B,
        }

    def run_generated_legacy_contract(
        self,
        workload_token,
        *,
        layer_id="legacy_layer",
        input_gradient_token="NONE",
        weight_gradient_token="NONE",
    ):
        return self.run_generated_legacy_records(
            "HYBRID_TRANSFORMER",
            [
                {
                    "layer_id": layer_id,
                    "forward": workload_token,
                    "input_gradient": input_gradient_token,
                    "weight_gradient": weight_gradient_token,
                }
            ],
        )

    def run_generated_legacy_records(self, header_policy, records):
        manifest = json.loads(
            (FIXTURES / "minimal_legacy_gpu_run.json").read_text()
        )
        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            workload_path = run_directory / "legacy-workload.txt"
            layer_lines = ""
            for record in records:
                layer_lines += (
                    f"{record['layer_id']}\t-1\t10\t{record['forward']}"
                    f"\t1048576\t10\t{record['input_gradient']}\t0"
                    f"\t10\t{record['weight_gradient']}\t0\t10"
                )
                if header_policy == "HYBRID_CUSTOMIZED":
                    layer_lines += "\tDATA"
                layer_lines += "\n"
            workload_path.write_text(
                f"{header_policy} model_parallel_NPU_group: 1 ep: 1 pp: 1 "
                "vpp: 1 ga: 1 all_gpus: 1 checkpoints: 0 "
                "checkpoint_initiates: 0 pp_comm 0\n"
                f"{len(records)}\n"
                f"{layer_lines}"
            )
            manifest["workload"] = {
                "path": str(workload_path),
                "sha256": "sha256:"
                + hashlib.sha256(workload_path.read_bytes()).hexdigest(),
            }
            manifest_path = run_directory / "run.json"
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
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
            result = json.loads(result_path.read_text())
        return completed, result

    def run_generated_hccl_contract(
        self,
        *,
        collective,
        workload_token,
        payload_semantics,
        reduction,
        traffic_algorithm,
        message_bytes=1048576,
        routing_matrix=None,
        include_routing=True,
        layer_count=1,
        routing_padding_bytes=0,
        routing_rank_override=None,
        routing_total_bytes=None,
        routing_via_stdin=False,
        projected_routing=None,
        mutate_projected=None,
        projected_content_override=None,
        projected_padding_bytes=0,
        projected_via_stdin=False,
        projected_digest_override=None,
        rank_count=4,
        timeout=30,
    ):
        """Build immutable synthetic artifacts, then observe the real process."""
        profile = json.loads((FIXTURES / "minimal_ascend_profile.json").read_text())
        profile["metadata"]["id"] = f"synthetic-a2-r{rank_count}-analytical"
        profile["spec"]["identity"]["physicalChipCount"]["value"] = rank_count
        profile["spec"]["identity"]["managementDeviceCount"]["value"] = rank_count
        profile["spec"]["topology"]["levels"][0]["rankCount"] = rank_count
        profile_content = json.dumps(profile, indent=2) + "\n"
        profile_digest = "sha256:" + hashlib.sha256(
            profile_content.encode()
        ).hexdigest()
        duration_ns = round(20000 + message_bytes / 25000000000 * 1000000000)
        raw = {
            "apiVersion": "simai.ascend.observation/v1alpha1",
            "kind": "HcclRawSample",
            "schemaSemver": "0.1.0",
            "metadata": {"id": f"synthetic-{collective.lower()}-point"},
            "spec": {
                "profileRef": profile["metadata"]["id"],
                "profileDigest": profile_digest,
                "collective": collective,
                "group": {
                    "rankCount": rank_count,
                    "scope": "HOST",
                    "groupType": "TP",
                    "topologyDigest": "sha256:" + "1" * 64,
                },
                "algorithm": {
                    "name": traffic_algorithm,
                    "version": "synthetic-v1",
                },
                "payload": {
                    "bytesPerRank": {
                        "semantics": (
                            "API_INPUT_BYTES"
                            if collective == "ALL_REDUCE"
                            else payload_semantics
                        ),
                        "uniformValue": message_bytes,
                        "unit": "B",
                    },
                    "dtype": "BF16",
                    "reduction": reduction,
                },
                "statistics": {
                    "timingStatistic": "ARITHMETIC_MEAN",
                    "sampleCount": 5,
                    "warmupExcluded": True,
                },
                "normalized": {
                    "averageTime": {"value": duration_ns, "unit": "ns"},
                    "algBandwidth": {
                        "value": message_bytes * 1000000000 / duration_ns,
                        "unit": "B/s",
                    },
                },
                "correctness": {"status": "PASS"},
                "eligibility": {"fit": True, "reasons": []},
                "evidence": [
                    {
                        "id": "worked-example-input",
                        "class": "USER_INPUT",
                        "readiness": "FIELD_UNVERIFIED",
                        "source": {
                            "uri": f"fixture://issue-18/{collective.lower()}",
                            "ref": "worked-example-v1",
                        },
                        "method": {"name": "synthetic-point", "version": "1"},
                        "asOf": "2026-08-18T00:00:00+08:00",
                        "conditions": {"hardwareAvailable": False},
                        "sanitization": "synthetic-no-host-data",
                    }
                ],
            },
        }

        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            profile_path = run_directory / "profile.json"
            profile_path.write_text(profile_content)
            projected_reference = None
            projected_content = None
            if projected_routing is not None:
                projected_routing = copy.deepcopy(projected_routing)
                if mutate_projected is not None:
                    mutate_projected(projected_routing)
                if projected_padding_bytes:
                    projected_routing["padding"] = "x" * projected_padding_bytes
                projected_path = run_directory / "projected-routing.json"
                projected_content = (
                    projected_content_override
                    if projected_content_override is not None
                    else json.dumps(projected_routing, indent=2) + "\n"
                )
                if not projected_via_stdin:
                    projected_path.write_text(projected_content)
                projected_reference = {
                    "path": "/dev/stdin" if projected_via_stdin else str(projected_path),
                    "sha256": projected_digest_override
                    or "sha256:" + hashlib.sha256(projected_content.encode()).hexdigest(),
                }
            routing_reference = None
            if routing_matrix is not None:
                routing = {
                    "apiVersion": "simai.ascend.routing/v1alpha1",
                    "kind": "HcclAllToAllVRouting",
                    "schemaSemver": "0.1.0",
                    "metadata": {"id": "synthetic-a2av-routing-r4-host"},
                    "spec": {
                        "profileDigest": profile_digest,
                        "topologyDigest": "sha256:" + "1" * 64,
                        "rankCount": 4,
                        "countSemantics": "HCCL_SEND_COUNTS_BYTES",
                        "unit": "B",
                        "sendCounts": routing_matrix,
                        "evidence": [
                            {
                                "id": "routing-input",
                                "class": "USER_INPUT",
                                "readiness": "FIELD_UNVERIFIED",
                                "source": {
                                    "uri": "fixture://issue-18/a2av-routing",
                                    "ref": "worked-example-v1",
                                },
                                "method": {
                                    "name": "synthetic-routing-matrix",
                                    "version": "1",
                                },
                                "asOf": "2026-08-18T00:00:00+08:00",
                                "conditions": {"hardwareAvailable": False},
                                "sanitization": "synthetic-no-host-data",
                            }
                        ],
                    },
                }
                if routing_padding_bytes or routing_total_bytes is not None:
                    routing["spec"]["padding"] = "x" * routing_padding_bytes
                if routing_rank_override is not None:
                    routing["spec"]["rankCount"] = routing_rank_override
                if routing_total_bytes is not None:
                    empty_padding = json.dumps(routing, indent=2) + "\n"
                    padding_bytes = routing_total_bytes - len(
                        empty_padding.encode()
                    )
                    self.assertGreaterEqual(padding_bytes, 0)
                    routing["spec"]["padding"] = "x" * padding_bytes
                routing_content = json.dumps(routing, indent=2) + "\n"
                if routing_total_bytes is not None:
                    self.assertEqual(
                        len(routing_content.encode()), routing_total_bytes
                    )
                routing_path = run_directory / "routing.json"
                if not routing_via_stdin:
                    routing_path.write_text(routing_content)
                routing_digest = "sha256:" + hashlib.sha256(
                    routing_content.encode()
                ).hexdigest()
                routing_reference = {
                    "path": "/dev/stdin" if routing_via_stdin else str(routing_path),
                    "sha256": routing_digest,
                }
                raw["spec"]["payload"]["bytesPerRank"].pop("uniformValue")
                raw["spec"]["payload"]["bytesPerRank"][
                    "maximumValue"
                ] = message_bytes
                raw["spec"]["payload"]["routingDigest"] = routing_digest
            raw_path = run_directory / "raw.json"
            raw_path.write_text(json.dumps(raw, indent=2) + "\n")
            raw_digest = "sha256:" + hashlib.sha256(raw_path.read_bytes()).hexdigest()
            model = {
                "apiVersion": "simai.ascend.costmodel/v1alpha1",
                "kind": "HcclCostModel",
                "schemaSemver": "0.1.0",
                "metadata": {"id": f"synthetic-{collective.lower()}-model"},
                "spec": {
                    "profileDigest": profile_digest,
                    "collective": collective,
                    "dtype": "BF16",
                    "reduction": reduction,
                    "timingScope": "DEVICE_ONLY",
                    "payloadSemantics": payload_semantics,
                    "groupDomain": {
                        "rankCounts": [rank_count],
                        "groupTypes": ["TP"],
                        "scopes": ["HOST"],
                        "topologyDigests": ["sha256:" + "1" * 64],
                    },
                    "messageDomainBytes": {
                        "min": message_bytes,
                        "max": message_bytes,
                        "unit": "B",
                    },
                    "inputSamples": [
                        {
                            "id": raw["metadata"]["id"],
                            "path": str(raw_path),
                            "sha256": raw_digest,
                        }
                    ],
                    "fit": {
                        "family": "ALPHA_BETA",
                        "formula": "round(startup_ns + message_B / bandwidth_Bps * 1000000000)",
                        "startup": {"value": 20000, "unit": "ns"},
                        "bandwidth": {"value": 25000000000, "unit": "B/s"},
                        "interpolation": "NONE",
                    },
                    "traffic": {
                        "algorithm": traffic_algorithm,
                        "semantics": "ALGORITHM_TOTAL_GROUP_BYTES",
                    },
                    "evidenceClass": "DERIVED",
                    "readiness": "FIELD_UNVERIFIED",
                    "extrapolation": {"allowed": False, "policy": "FAIL"},
                },
            }
            if routing_reference is not None:
                model["spec"]["routingDigest"] = routing_reference["sha256"]
            model_path = run_directory / "model.json"
            model_path.write_text(json.dumps(model, indent=2) + "\n")
            model_digest = "sha256:" + hashlib.sha256(
                model_path.read_bytes()
            ).hexdigest()
            workload_path = run_directory / "workload.txt"
            layer_lines = "".join(
                f"generated_layer_{index}\t-1\t10\t{workload_token}\t"
                f"{message_bytes}\t10\tNONE\t0\t10\tNONE\t0\t10\n"
                for index in range(layer_count)
            )
            workload_path.write_text(
                f"HYBRID_TRANSFORMER model_parallel_NPU_group: {rank_count} ep: 1 pp: 1 "
                f"vpp: 1 ga: 1 all_gpus: {rank_count} checkpoints: 0 "
                "checkpoint_initiates: 0 pp_comm 0\n"
                f"{layer_count}\n"
                f"{layer_lines}"
            )
            workload_digest = "sha256:" + hashlib.sha256(
                workload_path.read_bytes()
            ).hexdigest()
            manifest = {
                "schema_version": "simai.run/v1",
                "run_id": f"generated-{collective.lower().replace('_', '-')}",
                "backend": "analytical",
                "workload": {
                    "path": str(workload_path),
                    "sha256": workload_digest,
                },
                "device_profile": {
                    "path": str(profile_path),
                    "sha256": profile_digest,
                },
                "collective_cost_model": {
                    "path": str(model_path),
                    "sha256": model_digest,
                },
            }
            if routing_reference is not None and include_routing:
                manifest["routing"] = routing_reference
            if projected_reference is not None:
                manifest["projected_a2a"] = {
                    "schema_version": "simai.projected-a2a.request/v1alpha1",
                    "routing": projected_reference,
                }
            manifest_path = run_directory / "run.json"
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
            result_path = run_directory / "result.json"
            started = time.monotonic()
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
                input=routing_content if routing_via_stdin else None,
                timeout=timeout,
                check=False,
            )
            completed.wall_seconds = time.monotonic() - started
            completed.result_bytes = result_path.stat().st_size
            result = json.loads(result_path.read_text())
        return completed, result

    @staticmethod
    def projected_routing(policy, *, rank_count=4, domains=None):
        document = json.loads(
            (FIXTURES / "projected_a2a_uniform_r4_d2.json").read_text()
        )
        if domains is None:
            domains = [
                {"id": "domain-0", "firstRank": 0, "rankCount": 2},
                {"id": "domain-1", "firstRank": 2, "rankCount": 2},
            ]
        document["metadata"]["id"] = (
            f"synthetic-projected-a2a-r{rank_count}-d{len(domains)}"
        )
        document["spec"]["rankCount"] = rank_count
        document["spec"]["domains"] = domains
        document["spec"]["policy"] = policy
        return document

    def run_segmented_allgather_contract(
        self, runtime_message_bytes, *, mutate_model=None
    ):
        """Run one point against a two-segment, three-observation model."""
        profile_path = FIXTURES / "minimal_ascend_profile.json"
        profile_digest = "sha256:" + hashlib.sha256(
            profile_path.read_bytes()
        ).hexdigest()
        model = json.loads(
            (FIXTURES / "minimal_hccl_allgather_cost_model.json").read_text()
        )
        raw_template = json.loads(
            (FIXTURES / "minimal_hccl_allgather_observation.json").read_text()
        )
        observations = {
            4096: 10205,
            65536: 13277,
            1048576: 37853,
        }

        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            input_samples = []
            for message_bytes, duration_ns in observations.items():
                raw = json.loads(json.dumps(raw_template))
                raw_id = f"synthetic-hccl-ag-{message_bytes}-r4-host"
                raw["metadata"]["id"] = raw_id
                raw["spec"]["payload"]["bytesPerRank"][
                    "uniformValue"
                ] = message_bytes
                raw["spec"]["normalized"]["averageTime"][
                    "value"
                ] = duration_ns
                raw["spec"]["normalized"]["algBandwidth"][
                    "value"
                ] = message_bytes * 1000000000 / duration_ns
                raw_path = run_directory / f"raw-{message_bytes}.json"
                raw_path.write_text(json.dumps(raw, indent=2) + "\n")
                input_samples.append(
                    {
                        "id": raw_id,
                        "path": str(raw_path),
                        "sha256": "sha256:"
                        + hashlib.sha256(raw_path.read_bytes()).hexdigest(),
                    }
                )

            model["spec"]["profileDigest"] = profile_digest
            model["spec"]["messageDomainBytes"] = {
                "min": 4096,
                "max": 1048576,
                "unit": "B",
            }
            model["spec"]["inputSamples"] = input_samples
            model["spec"]["fit"] = {
                "family": "PIECEWISE_ALPHA_BETA",
                "formula": "round(startup_ns + message_B / bandwidth_Bps * 1000000000)",
                "interpolation": "SEGMENT_LOCAL",
                "segments": [
                    {
                        "min": 4096,
                        "max": 65536,
                        "upperBound": "EXCLUSIVE",
                        "startup": {"value": 10000, "unit": "ns"},
                        "bandwidth": {"value": 20000000000, "unit": "B/s"},
                    },
                    {
                        "min": 65536,
                        "max": 1048576,
                        "upperBound": "INCLUSIVE",
                        "startup": {"value": 11639, "unit": "ns"},
                        "bandwidth": {"value": 40000000000, "unit": "B/s"},
                    },
                ],
            }
            if mutate_model is not None:
                mutate_model(model)
            model_path = run_directory / "model.json"
            model_path.write_text(json.dumps(model, indent=2) + "\n")
            model_digest = "sha256:" + hashlib.sha256(
                model_path.read_bytes()
            ).hexdigest()
            workload_path = run_directory / "workload.txt"
            workload_path.write_text(
                "HYBRID_TRANSFORMER model_parallel_NPU_group: 4 ep: 1 pp: 1 "
                "vpp: 1 ga: 1 all_gpus: 4 checkpoints: 0 "
                "checkpoint_initiates: 0 pp_comm 0\n"
                "1\n"
                f"segmented_ag\t-1\t10\tALLGATHER\t{runtime_message_bytes}"
                "\t10\tNONE\t0\t10\tNONE\t0\t10\n"
            )
            manifest = {
                "schema_version": "simai.run/v1",
                "run_id": f"segmented-ag-{runtime_message_bytes}",
                "backend": "analytical",
                "workload": {
                    "path": str(workload_path),
                    "sha256": "sha256:"
                    + hashlib.sha256(workload_path.read_bytes()).hexdigest(),
                },
                "device_profile": {
                    "path": str(profile_path),
                    "sha256": profile_digest,
                },
                "collective_cost_model": {
                    "path": str(model_path),
                    "sha256": model_digest,
                },
            }
            manifest_path = run_directory / "run.json"
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
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
            result = json.loads(result_path.read_text())
        return completed, result

    @staticmethod
    def use_explicit_legacy_busbw_adapter(model):
        """Replace direct algbw with an explicit, typed legacy busbw adapter."""
        fit = model["spec"]["fit"]
        fit["family"] = "LEGACY_BUSBW_ADAPTER"
        fit.pop("bandwidth")
        fit["adapter"] = {
            "schema": "simai.legacy.busbw/v1",
            "columns": [
                "collective",
                "message_B",
                "rank_count",
                "bus_bandwidth_Bps",
            ],
            "collective": "ALL_REDUCE",
            "messageDomainBytes": {
                "min": 1048576,
                "max": 1048576,
                "unit": "B",
            },
            "rankCount": 4,
            "busBandwidth": {"value": 37500000000, "unit": "B/s"},
            "conversion": "HCCL_RING_BUSBW_TO_ALGBW",
        }

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

    def test_target_10t_model_manifest_reports_frozen_logical_parameters(self):
        completed, result, _ = self.run_contract("target_10t_symbolic_run.json")

        self.assertEqual(
            completed.returncode,
            0,
            msg=f"stdout:\n{completed.stdout[-2000:]}\nstderr:\n{completed.stderr[-2000:]}",
        )
        # Independent oracle: the production validator totals frozen tensor
        # groups. This test instead applies the accepted architecture transform
        # to the independently known official baseline.
        official_baseline = 1_598_837_347_742
        added_experts = 2048 - 384
        moe_layers = 61 + 1
        added_expert_weights = added_experts * moe_layers * (3 * 7168 * 3072)
        added_router_weights = added_experts * moe_layers * 7168
        added_router_biases = added_experts * (61 - 3 + 1)
        independent_parameter_oracle = (
            official_baseline
            + added_expert_weights
            + added_router_weights
            + added_router_biases
        )
        self.assertEqual(independent_parameter_oracle, 8_414_884_746_526)

        model = result["results"]["target_workload"]["model"]
        self.assertEqual(
            model["logical_trainable_parameters"], independent_parameter_oracle
        )
        self.assertEqual(model["routed_experts"], 2048)
        self.assertEqual(model["top_k"], 16)
        self.assertEqual(model["expert_intermediate_size"], 3072)
        self.assertEqual(model["shared_experts"], 1)
        self.assertEqual(model["parameter_unit"], "count")
        self.assertRegex(result["provenance"]["target_model_sha256"], SHA256_ID)
        self.assertEqual(result["readiness"]["target_model"], "READY")

    def test_target_model_identity_and_tensor_counting_are_frozen(self):
        mutations = (
            lambda model: model["spec"]["architecture"].update(
                {"routedExperts": 2047}
            ),
            lambda model: model["spec"]["architecture"].update({"topK": 15}),
            lambda model: model["spec"]["architecture"].update(
                {"expertIntermediateSize": 3071}
            ),
            lambda model: model["spec"]["architecture"].update(
                {"sharedExperts": 2}
            ),
            lambda model: model["spec"].update(
                {"logicalTrainableParameters": 8_414_884_746_525}
            ),
            lambda model: model["spec"]["tensorRegistry"]["entries"][35].update(
                {"instances": 249_855}
            ),
            lambda model: model["spec"]["checkpointStorage"].update(
                {"value": 8_414_884_746_526}
            ),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index):
                completed, result = self.run_mutated_target_contract(
                    mutate_model=mutation
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(
                    result["reject_code"],
                    (
                        "TARGET_MODEL_REGISTRY_INVALID"
                        if index == 5
                        else "TARGET_MODEL_IDENTITY_MISMATCH"
                    ),
                )
                self.assertEqual(result["readiness"]["target_model"], "BLOCKED")

    def test_target_tensor_registry_reports_checkpoint_auxiliary_elements(self):
        completed, result = self.run_mutated_target_contract()

        self.assertEqual(completed.returncode, 0)
        registry = json.loads(
            (FIXTURES / "target_10t_model_manifest.json").read_text()
        )["spec"]["tensorRegistry"]["entries"]
        dtype_bytes = {
            "BF16": 2,
            "F32": 4,
            "F8_E4M3": 1,
            "F8_E8M0": 1,
            "PACKED_FP4_I8": 1,
            "I64": 8,
        }

        def independent_product(shape):
            value = 1
            for dimension in shape:
                value *= dimension
            return value

        independent_logical = sum(
            entry["instances"] * independent_product(entry["logicalShape"])
            for entry in registry
            if entry["trainableRole"] == "LOGICAL_TRAINABLE"
        )
        independent_auxiliary = sum(
            entry["instances"]
            * independent_product(entry["checkpointStorageShape"])
            for entry in registry
            if entry["trainableRole"] == "CHECKPOINT_AUXILIARY"
        )
        independent_storage = sum(
            entry["instances"]
            * independent_product(entry["checkpointStorageShape"])
            * dtype_bytes[entry["checkpointStorageDtype"]]
            for entry in registry
        )
        self.assertEqual(independent_logical, 8_414_884_746_526)
        self.assertEqual(independent_auxiliary, 262_134_842_368)
        self.assertEqual(independent_storage, 4_486_847_493_752)
        model = result["results"]["target_workload"]["model"]
        self.assertEqual(model["logical_trainable_parameters"], independent_logical)
        self.assertEqual(
            model["checkpoint_auxiliary_elements"],
            {
                "quant_scale": 262_128_636_928,
                "routing_table": 6_205_440,
                "total": 262_134_842_368,
                "unit": "count",
            },
        )
        self.assertEqual(
            model["checkpoint_storage"]["value"], independent_storage
        )

    def test_target_tensor_registry_rejects_incomplete_or_ambiguous_types(self):
        def remove_type(model):
            model["spec"]["tensorRegistry"]["entries"].pop()

        def duplicate_type_id(model):
            entries = model["spec"]["tensorRegistry"]["entries"]
            entries[1]["id"] = entries[0]["id"]

        def unknown_role(model):
            model["spec"]["tensorRegistry"]["entries"][0][
                "trainableRole"
            ] = "UNKNOWN_ROLE"

        def unknown_scope(model):
            model["spec"]["tensorRegistry"]["entries"][0][
                "blockScope"
            ] = "UNKNOWN_SCOPE"

        def unknown_dtype(model):
            model["spec"]["tensorRegistry"]["entries"][0][
                "checkpointStorageDtype"
            ] = "UNKNOWN_DTYPE"

        def unclosed_fp4_shape(model):
            model["spec"]["tensorRegistry"]["entries"][35][
                "checkpointStorageShape"
            ][1] += 1

        for mutation in (
            remove_type,
            duplicate_type_id,
            unknown_role,
            unknown_scope,
            unknown_dtype,
            unclosed_fp4_shape,
        ):
            with self.subTest(mutation=mutation.__name__):
                completed, result = self.run_mutated_target_contract(
                    mutate_model=mutation
                )
                self.assertEqual(completed.returncode, 2)
                self.assertEqual(
                    result["reject_code"], "TARGET_MODEL_REGISTRY_INVALID"
                )

    def test_gts_exact_limit_is_accepted_and_reported(self):
        completed, result = self.run_mutated_target_contract()

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        step = result["results"]["target_workload"]["step"]
        # Independent oracle: production uses checked uint64 multiplication
        # while this assertion uses Python's arbitrary-precision integers.
        independent_gts_oracle = 15625 * 1 * 32 * 1000
        independent_assignment_oracle = independent_gts_oracle * 16 * 62
        self.assertEqual(independent_gts_oracle, 500_000_000)
        self.assertEqual(independent_assignment_oracle, 496_000_000_000)
        self.assertEqual(step["formula"], "sequence * MBS * DP * GA")
        self.assertEqual(step["sequence_tokens"], 15625)
        self.assertEqual(step["micro_batch_sequences"], 1)
        self.assertEqual(step["data_parallel_replicas"], 32)
        self.assertEqual(step["gradient_accumulation"], 1000)
        self.assertEqual(step["configured_gts"], independent_gts_oracle)
        self.assertEqual(step["gts_limit"], 500_000_000)
        self.assertEqual(
            step["configured_routed_assignment_slots_upper_bound"],
            independent_assignment_oracle,
        )
        self.assertRegex(result["provenance"]["target_step_sha256"], SHA256_ID)
        self.assertEqual(result["readiness"]["target_step"], "READY")

    def test_gts_above_limit_has_stable_rejection_without_erasing_model(self):
        def exceed_limit(step):
            step["spec"]["sequenceTokens"] = 15626
            step["spec"]["configuredGlobalTokens"] = 500_032_000
            step["spec"]["configuredRoutedAssignmentSlotsUpperBound"] = (
                500_032_000 * 16 * 62
            )

        completed, result = self.run_mutated_target_contract(
            mutate_step=exceed_limit
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"], "TARGET_STEP_GTS_LIMIT_EXCEEDED"
        )
        self.assertEqual(result["readiness"]["target_model"], "READY")
        self.assertEqual(result["readiness"]["target_step"], "BLOCKED")
        self.assertEqual(
            result["results"]["target_workload"]["model"][
                "logical_trainable_parameters"
            ],
            8_414_884_746_526,
        )

    def test_gts_factors_reject_zero_negative_noninteger_and_unsafe_integer(self):
        invalid_values = (0, -1, 1.5, "1", 9_007_199_254_740_992)
        for invalid_value in invalid_values:
            with self.subTest(invalid_value=invalid_value):
                completed, result = self.run_mutated_target_contract(
                    mutate_step=lambda step, value=invalid_value: step["spec"].update(
                        {"microBatchSequences": value}
                    )
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(
                    result["reject_code"], "TARGET_STEP_FACTOR_INVALID"
                )
                self.assertEqual(result["readiness"]["target_model"], "READY")
                self.assertEqual(result["readiness"]["target_step"], "BLOCKED")

    def test_target_integer_lexemes_reject_fraction_and_exponent_smuggling(self):
        for replacement in ("1.00000000000000001", "1e0"):
            with self.subTest(replacement=replacement):
                completed, result = self.run_mutated_target_contract(
                    raw_replacements={
                        "step": (
                            ('"microBatchSequences": 1',
                             f'"microBatchSequences": {replacement}'),
                        )
                    }
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(
                    result["reject_code"], "TARGET_STEP_FACTOR_INVALID"
                )

    def test_target_integer_lexemes_reject_unsafe_and_uint64_boundaries(self):
        cases = (
            ("01", "TARGET_STEP_INVALID_JSON"),
            ("9007199254740992", "TARGET_STEP_FACTOR_INVALID"),
            ("18446744073709551615", "TARGET_STEP_FACTOR_INVALID"),
            ("18446744073709551616", "TARGET_STEP_FACTOR_INVALID"),
        )
        for replacement, expected_code in cases:
            with self.subTest(replacement=replacement):
                completed, result = self.run_mutated_target_contract(
                    raw_replacements={
                        "step": (
                            ('"microBatchSequences": 1',
                             f'"microBatchSequences": {replacement}'),
                        )
                    }
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["reject_code"], expected_code)

    def test_target_memory_rejects_negative_underflow_integer_lexeme(self):
        component_values = {
            "PARAMETERS": 0,
            "GRADIENTS": 10,
            "OPTIMIZER_STATES": 10,
            "ACTIVATIONS": 10,
            "COMMUNICATION_BUFFERS": 10,
            "EXPERT_PLACEMENT": 10,
            "RECOMPUTATION": 10,
        }
        completed, result = self.run_mutated_target_contract(
            mutate_memory=lambda memory: self.materialize_memory_plan(
                memory,
                base_hbm_B=2000,
                reserve_hbm_B=1000,
                component_values=component_values,
            ),
            raw_replacements={
                "memory_event_plan": (
                    ('"peakBytes": 0', '"peakBytes": -1e-400'),
                )
            },
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_MEMORY_COMPONENTS_INVALID"
        )

    def test_gts_multiplication_overflow_is_rejected_before_limit_check(self):
        def overflow_uint64(step):
            step["spec"].update(
                {
                    "sequenceTokens": 500_000_000,
                    "microBatchSequences": 500_000_000,
                    "dataParallelReplicas": 500_000_000,
                    "gradientAccumulation": 500_000_000,
                }
            )

        completed, result = self.run_mutated_target_contract(
            mutate_step=overflow_uint64
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "TARGET_STEP_GTS_OVERFLOW")
        self.assertEqual(result["readiness"]["target_step"], "BLOCKED")

    def test_declared_gts_must_match_the_four_canonical_factors(self):
        completed, result = self.run_mutated_target_contract(
            mutate_step=lambda step: step["spec"].update(
                {"configuredGlobalTokens": 499_999_999}
            )
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "TARGET_STEP_GTS_MISMATCH")
        self.assertEqual(result["readiness"]["target_step"], "BLOCKED")

    def test_target_workload_closes_four_resource_digests_and_aicb_binding(self):
        completed, result = self.run_mutated_target_contract()

        self.assertEqual(completed.returncode, 0)
        provenance = result["provenance"]
        resource_digests = (
            provenance["target_model_sha256"],
            provenance["target_step_sha256"],
            provenance["target_routing_sha256"],
            provenance["target_memory_event_plan_sha256"],
        )
        expected_composite = "sha256:" + hashlib.sha256(
            "\n".join(resource_digests).encode()
        ).hexdigest()
        self.assertEqual(provenance["target_workload_sha256"], expected_composite)
        self.assertEqual(
            result["input_summary"]["target_workload_sha256"], expected_composite
        )
        self.assertEqual(
            result["results"]["target_workload"]["aicb_execution_binding"],
            {
                "workload_sha256": provenance["workload_sha256"],
                "model_sha256": resource_digests[0],
                "step_sha256": resource_digests[1],
                "routing_sha256": resource_digests[2],
                "memory_event_plan_sha256": resource_digests[3],
                "target_workload_sha256": expected_composite,
                "runtime_record_format": "STANDARD",
                "runtime_specific_parallelism": ["NONE"],
            },
        )
        for resource in (
            "target_model",
            "target_step",
            "target_routing",
            "target_memory_event_plan",
            "target_workload",
        ):
            self.assertEqual(result["readiness"][resource], "READY")
        for resource in (
            "target_model",
            "target_step",
            "target_routing",
            "target_memory_event_plan",
        ):
            self.assertRegex(result["evidence"][resource]["digest"], SHA256_ID)
            self.assertEqual(
                result["evidence"][resource]["readiness"], "FIELD_UNVERIFIED"
            )

    def test_target_envelope_is_rejected_before_artifact_io(self):
        def invalidate_envelope_and_references(manifest):
            manifest["target_workload"]["schema_version"] = "invalid/v0"
            for resource in ("model", "step", "routing", "memory_event_plan"):
                manifest["target_workload"][resource]["path"] = (
                    "/definitely-not-present/target-resource.json"
                )

        completed, result = self.run_mutated_target_contract(
            mutate_manifest=invalidate_envelope_and_references
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "TARGET_WORKLOAD_SCHEMA_INVALID")
        self.assertEqual(result["readiness"]["target_model"], "BLOCKED")

    def test_target_resources_enforce_exact_bounded_stream_limits(self):
        reject_codes = {
            "model": "TARGET_MODEL_ARTIFACT_TOO_LARGE",
            "step": "TARGET_STEP_ARTIFACT_TOO_LARGE",
            "routing": "TARGET_ROUTING_ARTIFACT_TOO_LARGE",
            "memory_event_plan": "TARGET_MEMORY_ARTIFACT_TOO_LARGE",
        }
        for resource, maximum_bytes in TARGET_RESOURCE_MAX_BYTES.items():
            with self.subTest(resource=resource, boundary="exact"):
                completed, result = self.run_mutated_target_contract(
                    artifact_sizes={resource: maximum_bytes}
                )
                self.assertEqual(completed.returncode, 0)
                self.assertEqual(result["status"], "VALID")

            with self.subTest(resource=resource, boundary="plus_one"):
                completed, result = self.run_mutated_target_contract(
                    artifact_sizes={resource: maximum_bytes + 1}
                )
                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["reject_code"], reject_codes[resource])

    def test_target_routing_bounded_loader_supports_nonseekable_stdin(self):
        maximum_bytes = TARGET_RESOURCE_MAX_BYTES["routing"]
        completed, result = self.run_mutated_target_contract(
            artifact_sizes={"routing": maximum_bytes},
            stdin_resource="routing",
        )
        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")

        completed, result = self.run_mutated_target_contract(
            artifact_sizes={"routing": maximum_bytes + 1},
            stdin_resource="routing",
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_ROUTING_ARTIFACT_TOO_LARGE"
        )

    def test_target_resource_schemas_reject_unknown_keys_and_identity_changes(self):
        cases = (
            (
                "model-metadata-type",
                {"mutate_model": lambda model: model.update({"metadata": 0})},
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "model-metadata-id",
                {
                    "mutate_model": lambda model: model["metadata"].update(
                        {"id": "different-model"}
                    )
                },
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "model-root-extra",
                {"mutate_model": lambda model: model.update({"extra": True})},
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "model-source-extra",
                {
                    "mutate_model": lambda model: model["spec"]["source"].update(
                        {"branch": "main"}
                    )
                },
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "model-architecture-extra",
                {
                    "mutate_model": lambda model: model["spec"][
                        "architecture"
                    ].update({"denseLayers": 3})
                },
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "model-registry-extra",
                {
                    "mutate_model": lambda model: model["spec"][
                        "tensorRegistry"
                    ].update({"groups": []})
                },
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "model-registry-entry-extra",
                {
                    "mutate_model": lambda model: model["spec"][
                        "tensorRegistry"
                    ]["entries"][0].update({"declaredElements": 1})
                },
                "TARGET_MODEL_REGISTRY_INVALID",
            ),
            (
                "model-evidence-extra",
                {
                    "mutate_model": lambda model: model["spec"]["evidence"][
                        0
                    ].update({"note": "not-controlled"})
                },
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "model-evidence-source-extra",
                {
                    "mutate_model": lambda model: model["spec"]["evidence"][
                        0
                    ]["source"].update({"mirror": "none"})
                },
                "TARGET_MODEL_SCHEMA_INVALID",
            ),
            (
                "step-spec-extra",
                {
                    "mutate_step": lambda step: step["spec"].update(
                        {"extra": True}
                    )
                },
                "TARGET_STEP_SCHEMA_INVALID",
            ),
            (
                "step-metadata-extra",
                {
                    "mutate_step": lambda step: step["metadata"].update(
                        {"name": "unexpected"}
                    )
                },
                "TARGET_STEP_SCHEMA_INVALID",
            ),
            (
                "routing-root-projection",
                {
                    "mutate_routing": lambda routing: routing.update(
                        {"projectedA2ATraffic": {"totalBytes": 1}}
                    )
                },
                "TARGET_ROUTING_SCHEMA_INVALID",
            ),
            (
                "routing-policy-extension",
                {
                    "mutate_routing": lambda routing: routing["spec"][
                        "policy"
                    ].update(
                        {
                            "extensions": {
                                "projected_a2a_traffic": {"total_bytes": 1}
                            }
                        }
                    )
                },
                "TARGET_ROUTING_SCHEMA_INVALID",
            ),
            (
                "routing-unknown-algorithm",
                {
                    "mutate_routing": lambda routing: routing["spec"][
                        "policy"
                    ].update({"algorithm": "UNKNOWN_PROJECTOR"})
                },
                "TARGET_ROUTING_SCHEMA_INVALID",
            ),
            (
                "memory-component-extra",
                {
                    "mutate_memory": lambda memory: memory["spec"][
                        "components"
                    ][0].update({"checkpointBytes": 1})
                },
                "TARGET_MEMORY_SCHEMA_INVALID",
            ),
            (
                "memory-bindings-extra",
                {
                    "mutate_memory": lambda memory: memory["spec"][
                        "bindings"
                    ].update({"allocator": "UNBOUND"})
                },
                "TARGET_MEMORY_SCHEMA_INVALID",
            ),
            (
                "memory-capacity-extra",
                {
                    "mutate_memory": lambda memory: memory["spec"][
                        "capacity"
                    ].update({"runtimeFreeHbmB": 1})
                },
                "TARGET_MEMORY_SCHEMA_INVALID",
            ),
            (
                "target-reference-extra",
                {
                    "mutate_manifest": lambda manifest: manifest[
                        "target_workload"
                    ]["model"].update({"mediaType": "application/json"})
                },
                "TARGET_WORKLOAD_SCHEMA_INVALID",
            ),
        )
        for name, kwargs, expected_code in cases:
            with self.subTest(name=name):
                completed, result = self.run_mutated_target_contract(**kwargs)
                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["reject_code"], expected_code)

    def test_target_workload_composite_digest_mismatch_fails_closed(self):
        wrong_digest = "sha256:" + "0" * 64

        def corrupt_composite(manifest):
            manifest["target_workload"]["sha256"] = wrong_digest
            manifest["workload"]["target_workload_sha256"] = wrong_digest

        completed, result = self.run_mutated_target_contract(
            mutate_manifest=corrupt_composite
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_WORKLOAD_DIGEST_MISMATCH"
        )
        self.assertEqual(result["readiness"]["target_memory_event_plan"], "READY")
        self.assertEqual(result["readiness"]["target_workload"], "BLOCKED")

    def test_aicb_workload_binding_must_match_target_composite(self):
        completed, result = self.run_mutated_target_contract(
            mutate_workload=lambda workload: workload.update(
                {
                    "header": workload["header"].replace(
                        workload["header"].split()[-1],
                        "sha256:" + "0" * 64,
                    )
                }
            )
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_AICB_BINDING_MISMATCH"
        )
        self.assertEqual(result["readiness"]["target_workload"], "BLOCKED")

    def test_target_aicb_binding_is_consumed_from_header_and_event(self):
        completed, result = self.run_mutated_target_contract()

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["readiness"]["target_workload"], "READY")

    def test_target_bound_customized_event_applies_specific_parallelism(self):
        completed, result = self.run_mutated_target_contract(
            target_workload_format="CUSTOMIZED",
            target_specific_parallelism="DATA",
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(
            result["results"]["target_workload"]["aicb_execution_binding"]
            ["runtime_record_format"],
            "CUSTOMIZED",
        )
        self.assertEqual(
            result["results"]["target_workload"]["aicb_execution_binding"]
            ["runtime_specific_parallelism"],
            ["DATA"],
        )

    def test_target_bound_customized_event_requires_specific_parallelism(self):
        def remove_specific_parallelism(workload):
            fields = workload["layers"][0].split("\t")
            self.assertEqual(fields[12], "DATA")
            del fields[12]
            workload["layers"][0] = "\t".join(fields)

        completed, result = self.run_mutated_target_contract(
            target_workload_format="CUSTOMIZED",
            target_specific_parallelism="DATA",
            mutate_workload=remove_specific_parallelism,
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_AICB_EVENT_BINDING_MISSING"
        )

    def test_target_bound_customized_event_rejects_unknown_specific_parallelism(self):
        completed, result = self.run_mutated_target_contract(
            target_workload_format="CUSTOMIZED",
            target_specific_parallelism="DATTA",
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"],
            "TARGET_AICB_SPECIFIC_PARALLELISM_INVALID",
        )
        self.assertEqual(result["readiness"]["target_workload"], "BLOCKED")

    def test_target_workload_execution_uses_the_verified_byte_snapshot(self):
        def replace_one_mib_with_two_mib(workload):
            fields = workload["layers"][0].split("\t")
            self.assertEqual(fields[4], "1048576")
            fields[4] = "2097152"
            workload["layers"][0] = "\t".join(fields)

        completed, result = self.run_mutated_target_contract(
            target_forward_collective="ALLREDUCE",
            target_forward_bytes=1_048_576,
            ascend_profiled=True,
            atomic_workload_replacement=replace_one_mib_with_two_mib,
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["results"]["timing_ns"], 61_943)
        self.assertEqual(
            result["results"]["collective_payload"]["input_B_per_rank"],
            1_048_576,
        )

    def test_arbitrary_legacy_workload_cannot_claim_the_target_contract(self):
        def replace_with_legacy(workload):
            workload["header"] = (
                "HYBRID_TRANSFORMER model_parallel_NPU_group: 1 ep: 1 pp: 1 "
                "vpp: 1 ga: 1 all_gpus: 1 checkpoints: 0 "
                "checkpoint_initiates: 0 pp_comm 0"
            )
            workload["layers"] = [
                "legacy_layer\t-1\t10\tNONE\t0\t10\tNONE\t0\t10\tNONE\t0\t10"
            ]

        completed, result = self.run_mutated_target_contract(
            mutate_workload=replace_with_legacy
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "TARGET_AICB_BINDING_MISSING")
        self.assertEqual(result["readiness"]["target_workload"], "BLOCKED")

    def test_target_aicb_event_requires_all_resource_bindings(self):
        def remove_model_binding(workload):
            fields = workload["layers"][0].split("\t")
            del fields[12]
            workload["layers"][0] = "\t".join(fields)

        completed, result = self.run_mutated_target_contract(
            mutate_workload=remove_model_binding
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_AICB_EVENT_BINDING_MISSING"
        )

    def test_target_aicb_event_single_hash_tamper_fails_closed(self):
        def tamper_routing_binding(workload):
            fields = workload["layers"][0].split("\t")
            fields[14] = "sha256:" + "0" * 64
            workload["layers"][0] = "\t".join(fields)

        completed, result = self.run_mutated_target_contract(
            mutate_workload=tamper_routing_binding
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_AICB_EVENT_BINDING_MISMATCH"
        )

    def test_target_aicb_binding_propagates_to_every_layer_event(self):
        def tamper_middle_layer(workload):
            fields = workload["layers"][1].split("\t")
            fields[13] = "sha256:" + "f" * 64
            workload["layers"][1] = "\t".join(fields)

        completed, result = self.run_mutated_target_contract(
            mutate_workload=tamper_middle_layer,
            workload_layer_count=3,
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(
            result["reject_code"], "TARGET_AICB_EVENT_BINDING_MISMATCH"
        )

    def test_target_resource_dependency_digests_are_layered(self):
        cases = (
            (
                lambda routing: routing["spec"].update(
                    {"modelDigest": "sha256:" + "0" * 64}
                ),
                None,
                "TARGET_ROUTING_MODEL_DIGEST_MISMATCH",
            ),
            (
                None,
                lambda memory: memory["spec"].update(
                    {"stepDigest": "sha256:" + "0" * 64}
                ),
                "TARGET_MEMORY_RESOURCE_DIGEST_MISMATCH",
            ),
        )
        for mutate_routing, mutate_memory, expected_code in cases:
            with self.subTest(expected_code=expected_code):
                completed, result = self.run_mutated_target_contract(
                    mutate_routing=mutate_routing,
                    mutate_memory=mutate_memory,
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["reject_code"], expected_code)
                self.assertEqual(result["readiness"]["target_model"], "READY")
                self.assertEqual(result["readiness"]["target_step"], "READY")

    def test_target_routing_does_not_reuse_projected_a2a(self):
        mutations = (
            lambda routing: routing["spec"].update(
                {"projectedA2ATraffic": {"totalBytes": 1}}
            ),
            lambda routing: routing["spec"]["policy"].update(
                {"projectedA2ATraffic": {"totalBytes": 1}}
            ),
            lambda routing: routing["spec"]["policy"].update(
                {"domainPairBytes": [[1]]}
            ),
            lambda routing: routing["spec"]["policy"].update(
                {"topologyResourceLoads": {"link": 1}}
            ),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                completed, result = self.run_mutated_target_contract(
                    mutate_routing=mutation
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(
                    result["reject_code"],
                    "TARGET_ROUTING_SCHEMA_INVALID",
                )
                self.assertEqual(
                    result["readiness"]["target_routing"], "BLOCKED"
                )

    def test_target_workload_missing_routing_resource_is_unsupported(self):
        completed, result = self.run_mutated_target_contract(
            mutate_manifest=lambda manifest: manifest["target_workload"].pop(
                "routing"
            )
        )

        self.assertEqual(completed.returncode, 3)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(result["reject_code"], "TARGET_ROUTING_REQUIRED")
        self.assertEqual(result["readiness"]["target_routing"], "BLOCKED")

    def test_target_resource_evidence_is_required_for_readiness(self):
        completed, result = self.run_mutated_target_contract(
            mutate_memory=lambda memory: memory["spec"].update({"evidence": []})
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["reject_code"], "TARGET_MEMORY_SCHEMA_INVALID")
        self.assertEqual(
            result["readiness"]["target_memory_event_plan"], "BLOCKED"
        )

    def test_each_target_resource_requires_one_resolved_consistent_evidence_ref(self):
        resources = (
            ("model", "mutate_model", "TARGET_MODEL_SCHEMA_INVALID"),
            ("step", "mutate_step", "TARGET_STEP_SCHEMA_INVALID"),
            ("routing", "mutate_routing", "TARGET_ROUTING_SCHEMA_INVALID"),
            ("memory", "mutate_memory", "TARGET_MEMORY_SCHEMA_INVALID"),
        )

        def missing_ref(document):
            document["spec"].pop("evidenceRef", None)

        def unresolved_ref(document):
            document["spec"]["evidenceRef"] = "does-not-exist"

        def conflicting_readiness(document):
            document["spec"]["readiness"] = "FIELD_VERIFIED"

        def invalid_record_readiness(document):
            document["spec"]["evidence"][0]["readiness"] = "GARBAGE"

        def ambiguous_records(document):
            second = json.loads(json.dumps(document["spec"]["evidence"][0]))
            second["id"] = "second-evidence-record"
            document["spec"]["evidence"].append(second)

        for resource, keyword, expected_code in resources:
            for mutation in (
                missing_ref,
                unresolved_ref,
                conflicting_readiness,
                invalid_record_readiness,
                ambiguous_records,
            ):
                with self.subTest(resource=resource, mutation=mutation.__name__):
                    completed, result = self.run_mutated_target_contract(
                        **{keyword: mutation}
                    )
                    self.assertEqual(completed.returncode, 2)
                    self.assertEqual(result["reject_code"], expected_code)
                    self.assertEqual(
                        result["readiness"][
                            "target_memory_event_plan"
                            if resource == "memory"
                            else f"target_{resource}"
                        ],
                        "BLOCKED",
                    )

    def test_unbound_memory_policies_remain_symbolic_and_checkpoint_is_not_hbm(self):
        completed, result = self.run_mutated_target_contract()

        self.assertEqual(completed.returncode, 0)
        memory = result["results"]["memory"]
        self.assertEqual(memory["unit"], "B")
        self.assertEqual(
            memory["aggregation"], "CONSERVATIVE_COMPONENT_PEAK_SUM"
        )
        self.assertEqual(
            memory["bindings"],
            {
                "precision": "UNBOUND",
                "optimizer": "UNBOUND",
                "placement": "UNBOUND",
                "recomputation": "UNBOUND",
                "runtime": "UNBOUND",
            },
        )
        expected_categories = {
            "parameters",
            "gradients",
            "optimizer_states",
            "activations",
            "communication_buffers",
            "expert_placement",
            "recomputation",
        }
        self.assertEqual(set(memory["components"]), expected_categories)
        for component in memory["components"].values():
            self.assertEqual(component["state"], "SYMBOLIC")
            self.assertEqual(component["unit"], "B")
            self.assertEqual(component["value"], "UNKNOWN")
            self.assertIsInstance(component["expression"], str)
            self.assertTrue(component["expression"])
        self.assertEqual(memory["peak_per_rank_B"], "UNKNOWN")
        self.assertEqual(memory["search_95_percent_gate"], "UNKNOWN")
        self.assertEqual(memory["a2_a3_execution_85_percent_gate"], "UNKNOWN")
        self.assertEqual(result["readiness"]["hbm"], "UNKNOWN")
        self.assertEqual(result["results"]["hbm_peak_B"], "UNKNOWN")

        model = result["results"]["target_workload"]["model"]
        self.assertEqual(
            model["checkpoint_storage"],
            {
                "value": 4_486_847_493_752,
                "unit": "B",
                "semantics": "FIXED_QUANTIZED_CHECKPOINT_ONLY_NOT_TRAINING_HBM",
                "used_as_training_hbm": False,
            },
        )
        self.assertEqual(
            model["active_logical_parameters"],
            {
                "main_blocks_only": 88_950_053_982,
                "main_forward_including_io": 90_803_533_923,
                "training_graph_including_mtp": 92_345_423_134,
                "unit": "count",
            },
        )

    def test_search_hbm_exact_95_percent_floor_passes_and_one_byte_over_fails(self):
        component_values = {
            "PARAMETERS": 200,
            "GRADIENTS": 100,
            "OPTIMIZER_STATES": 150,
            "ACTIVATIONS": 200,
            "COMMUNICATION_BUFFERS": 100,
            "EXPERT_PLACEMENT": 100,
            "RECOMPUTATION": 100,
        }

        def exact_boundary(memory):
            self.materialize_memory_plan(
                memory,
                base_hbm_B=2001,
                reserve_hbm_B=1000,
                component_values=component_values,
            )

        exact_completed, exact_result = self.run_mutated_target_contract(
            mutate_memory=exact_boundary
        )
        self.assertEqual(exact_completed.returncode, 0)
        exact = exact_result["results"]["memory"]
        # Independent oracle: do not reuse the production threshold helper.
        independent_peak_oracle = sum(component_values.values())
        independent_usable_oracle = 2001 - 1000
        independent_search_limit_oracle = independent_usable_oracle * 95 // 100
        self.assertEqual(independent_peak_oracle, 950)
        self.assertEqual(independent_search_limit_oracle, 950)
        self.assertEqual(exact["peak_per_rank_B"], independent_peak_oracle)
        self.assertEqual(exact["capacity"]["base_hbm_B"], 2001)
        self.assertEqual(
            exact["capacity"]["scenario_usable_hbm_B"],
            independent_usable_oracle,
        )
        self.assertEqual(exact["search_limit"]["denominator"], "SCENARIO_USABLE_HBM_B")
        self.assertEqual(exact["search_limit"]["rounding"], "FLOOR_INTEGER_BYTES")
        self.assertEqual(
            exact["search_limit"]["maximum_allowed_B"],
            independent_search_limit_oracle,
        )
        self.assertEqual(exact["search_95_percent_gate"], "PASS")
        self.assertEqual(exact_result["readiness"]["hbm"], "READY")
        self.assertEqual(exact_result["results"]["hbm_peak_B"], 950)

        over_values = dict(component_values)
        over_values["PARAMETERS"] += 1

        def one_byte_over(memory):
            self.materialize_memory_plan(
                memory,
                base_hbm_B=2001,
                reserve_hbm_B=1000,
                component_values=over_values,
            )

        over_completed, over_result = self.run_mutated_target_contract(
            mutate_memory=one_byte_over
        )
        self.assertEqual(over_completed.returncode, 2)
        self.assertEqual(over_result["reject_code"], "HBM_SEARCH_LIMIT_EXCEEDED")
        self.assertEqual(over_result["results"]["memory"]["peak_per_rank_B"], 951)
        self.assertEqual(
            over_result["results"]["memory"]["search_95_percent_gate"], "FAIL"
        )
        self.assertEqual(over_result["readiness"]["hbm"], "BLOCKED")

    def test_a2_a3_execution_requires_strictly_below_85_percent_base_hbm(self):
        component_values = {
            "PARAMETERS": 200,
            "GRADIENTS": 100,
            "OPTIMIZER_STATES": 150,
            "ACTIVATIONS": 200,
            "COMMUNICATION_BUFFERS": 100,
            "EXPERT_PLACEMENT": 100,
            "RECOMPUTATION": 100,
        }

        def run_observed(observed_peak):
            return self.run_mutated_target_contract(
                mutate_memory=lambda memory: self.materialize_memory_plan(
                    memory,
                    base_hbm_B=2000,
                    reserve_hbm_B=1000,
                    component_values=component_values,
                    observed_execution_peak_B=observed_peak,
                )
            )

        below_completed, below_result = run_observed(1699)
        self.assertEqual(below_completed.returncode, 0)
        below = below_result["results"]["memory"]
        # Independent oracle: exact rational comparison, not the production
        # quotient/remainder implementation.
        independent_boundary_oracle = 2000 * 85 // 100
        independent_maximum_accepted_oracle = (2000 * 85 - 1) // 100
        self.assertEqual(independent_boundary_oracle, 1700)
        self.assertEqual(independent_maximum_accepted_oracle, 1699)
        self.assertEqual(below["execution_limit"]["denominator"], "BASE_HBM_B")
        self.assertEqual(below["execution_limit"]["comparison"], "STRICTLY_LESS_THAN_85_PERCENT")
        self.assertEqual(
            below["execution_limit"]["boundary_B"], independent_boundary_oracle
        )
        self.assertEqual(
            below["execution_limit"]["maximum_accepted_B"],
            independent_maximum_accepted_oracle,
        )
        self.assertEqual(below["a2_a3_execution_85_percent_gate"], "PASS")

        for observed_peak in (1700, 1701):
            with self.subTest(observed_peak=observed_peak):
                completed, result = run_observed(observed_peak)
                self.assertEqual(completed.returncode, 5)
                self.assertEqual(result["status"], "INVALID_ACCURACY_EXECUTION")
                self.assertEqual(
                    result["reject_code"], "HBM_EXECUTION_LIMIT_REACHED"
                )
                memory = result["results"]["memory"]
                self.assertEqual(
                    memory["a2_a3_execution_85_percent_gate"],
                    "INVALID_ACCURACY_EXECUTION",
                )
                self.assertEqual(memory["observed_execution_peak_B"], observed_peak)
                self.assertEqual(result["readiness"]["hbm"], "BLOCKED")

    def test_memory_materialization_requires_complete_bindings_and_conserved_peak(self):
        component_values = {
            "PARAMETERS": 200,
            "GRADIENTS": 100,
            "OPTIMIZER_STATES": 150,
            "ACTIVATIONS": 200,
            "COMMUNICATION_BUFFERS": 100,
            "EXPERT_PLACEMENT": 100,
            "RECOMPUTATION": 100,
        }

        def partial_binding(memory):
            self.materialize_memory_plan(
                memory,
                base_hbm_B=2000,
                reserve_hbm_B=1000,
                component_values=component_values,
            )
            memory["spec"]["bindings"]["runtime"] = "UNBOUND"

        def inconsistent_peak(memory):
            self.materialize_memory_plan(
                memory,
                base_hbm_B=2000,
                reserve_hbm_B=1000,
                component_values=component_values,
            )
            memory["spec"]["capacity"]["plannedPeakHbmB"] = 949

        for mutation, expected_code in (
            (partial_binding, "TARGET_MEMORY_BINDINGS_INCOMPLETE"),
            (inconsistent_peak, "TARGET_MEMORY_CAPACITY_MISMATCH"),
        ):
            with self.subTest(expected_code=expected_code):
                completed, result = self.run_mutated_target_contract(
                    mutate_memory=mutation
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["reject_code"], expected_code)
                self.assertEqual(result["readiness"]["target_model"], "READY")
                self.assertEqual(result["readiness"]["target_step"], "READY")

    def test_memory_component_expression_and_dependencies_are_canonical(self):
        def change_expression(memory):
            memory["spec"]["components"][0]["expression"] = "checkpoint bytes"

        def drop_dependency(memory):
            memory["spec"]["components"][4]["requires"] = ["runtime"]

        for mutation in (change_expression, drop_dependency):
            with self.subTest(mutation=mutation.__name__):
                completed, result = self.run_mutated_target_contract(
                    mutate_memory=mutation
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(
                    result["reject_code"], "TARGET_MEMORY_COMPONENTS_INVALID"
                )

    def test_legacy_gpu_alltoallv_is_explicitly_unsupported(self):
        completed, result = self.run_generated_legacy_contract("ALLTOALLV")

        self.assertEqual(completed.returncode, 3)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(result["reject_code"], "LEGACY_ALLTOALLV_UNSUPPORTED")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

    def test_legacy_layer_identifier_is_not_decoded_as_a_collective(self):
        for layer_id in ("ALLTOALLV", "ALLTOALLV_debug"):
            with self.subTest(layer_id=layer_id):
                completed, result = self.run_generated_legacy_contract(
                    "NONE", layer_id=layer_id
                )

                self.assertEqual(completed.returncode, 0)
                self.assertEqual(result["status"], "VALID")
                self.assertEqual(result["reject_code"], "NONE")

    def test_legacy_unknown_alltoallv_variant_is_invalid_input(self):
        cases = (
            {"workload_token": "ALLTOALLVXYZ"},
            {
                "workload_token": "NONE",
                "input_gradient_token": "ALLTOALLVXYZ",
            },
            {
                "workload_token": "NONE",
                "weight_gradient_token": "ALLTOALLVXYZ",
            },
        )
        for case in cases:
            with self.subTest(case=case):
                completed, result = self.run_generated_legacy_contract(**case)

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["status"], "INVALID_INPUT")
                self.assertEqual(
                    result["reject_code"],
                    "LEGACY_COLLECTIVE_TOKEN_INVALID",
                )
                self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
                self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

    def test_customized_second_layer_alltoallv_is_unsupported(self):
        for phase in ("forward", "input_gradient", "weight_gradient"):
            records = [
                {
                    "layer_id": "customized_first",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
                {
                    "layer_id": "customized_second",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
            ]
            records[1][phase] = "ALLTOALLV"
            with self.subTest(phase=phase):
                completed, result = self.run_generated_legacy_records(
                    "HYBRID_CUSTOMIZED", records
                )

                self.assertEqual(completed.returncode, 3)
                self.assertEqual(result["status"], "UNSUPPORTED")
                self.assertEqual(
                    result["reject_code"], "LEGACY_ALLTOALLV_UNSUPPORTED"
                )

    def test_customized_second_layer_unknown_alltoallv_is_invalid(self):
        for phase in ("forward", "input_gradient", "weight_gradient"):
            records = [
                {
                    "layer_id": "customized_first",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
                {
                    "layer_id": "customized_second",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
            ]
            records[1][phase] = "ALLTOALLVXYZ"
            with self.subTest(phase=phase):
                completed, result = self.run_generated_legacy_records(
                    "HYBRID_CUSTOMIZED", records
                )

                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["status"], "INVALID_INPUT")
                self.assertEqual(
                    result["reject_code"],
                    "LEGACY_COLLECTIVE_TOKEN_INVALID",
                )

    def test_customized_second_layer_identifier_is_not_a_collective(self):
        completed, result = self.run_generated_legacy_records(
            "HYBRID_CUSTOMIZED",
            [
                {
                    "layer_id": "customized_first",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
                {
                    "layer_id": "ALLTOALLV_debug",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
            ],
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["reject_code"], "NONE")

    def test_standard_multi_layer_legacy_workload_remains_valid(self):
        completed, result = self.run_generated_legacy_records(
            "HYBRID_TRANSFORMER",
            [
                {
                    "layer_id": "standard_first",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
                {
                    "layer_id": "standard_second",
                    "forward": "NONE",
                    "input_gradient": "NONE",
                    "weight_gradient": "NONE",
                },
            ],
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["reject_code"], "NONE")

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
        self.assertEqual(result["readiness"]["topology"], "READY")
        self.assertEqual(result["readiness"]["routing"], "NOT_REQUIRED")
        self.assertEqual(result["readiness"]["traffic"], "READY")

    def test_minimal_ascend_allgather_returns_canonical_payload_and_cost(self):
        completed, result, _ = self.run_contract(
            "minimal_ascend_allgather_run.json"
        )

        self.assertEqual(
            completed.returncode,
            0,
            msg=f"stdout:\n{completed.stdout[-2000:]}\nstderr:\n{completed.stderr[-2000:]}",
        )
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["reject_code"], "NONE")
        self.assertEqual(result["results"]["timing_ns"], 61943)
        self.assertEqual(result["results"]["traffic_B"], 12582912)
        self.assertEqual(
            result["results"]["collective"],
            {
                "operation": "ALL_GATHER",
                "message_bytes_per_rank": 1048576,
                "rank_count": 4,
                "group_type": "TP",
                "topology_domain": "HOST",
            },
        )
        self.assertEqual(
            result["results"]["collective_payload"],
            {
                "semantics": "HCCL_ALLGATHER_SEND_BYTES",
                "input_B_per_rank": 1048576,
                "output_B_per_rank": 4194304,
                "routing_sha256": "NOT_REQUIRED",
            },
        )

    def test_reduce_scatter_returns_canonical_payload_and_cost(self):
        completed, result = self.run_generated_hccl_contract(
            collective="REDUCE_SCATTER",
            workload_token="REDUCESCATTER",
            payload_semantics="HCCL_REDUCESCATTER_INPUT_BYTES",
            reduction="SUM",
            traffic_algorithm="RING_REDUCE_SCATTER",
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["results"]["timing_ns"], 61943)
        self.assertEqual(result["results"]["traffic_B"], 3145728)
        self.assertEqual(result["results"]["collective"]["operation"], "REDUCE_SCATTER")
        self.assertEqual(
            result["results"]["collective_payload"],
            {
                "semantics": "HCCL_REDUCESCATTER_INPUT_BYTES",
                "input_B_per_rank": 1048576,
                "output_B_per_rank": 262144,
                "routing_sha256": "NOT_REQUIRED",
            },
        )

    def test_uniform_alltoall_returns_canonical_payload_and_cost(self):
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL",
            workload_token="ALLTOALL",
            payload_semantics="HCCL_ALLTOALL_TOTAL_SEND_BYTES",
            reduction="NONE",
            traffic_algorithm="UNIFORM_DIRECT_EXCHANGE",
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["results"]["timing_ns"], 61943)
        self.assertEqual(result["results"]["traffic_B"], 3145728)
        self.assertEqual(result["results"]["collective"]["operation"], "ALL_TO_ALL")
        self.assertEqual(
            result["results"]["collective_payload"],
            {
                "semantics": "HCCL_ALLTOALL_TOTAL_SEND_BYTES",
                "input_B_per_rank": 1048576,
                "output_B_per_rank": 1048576,
                "routing_sha256": "NOT_REQUIRED",
            },
        )

    def test_projected_uniform_alltoall_matches_enumerated_ground_truth(self):
        routing = self.projected_routing(
            {"kind": "UNIFORM", "messageBytesPerRank": 400}
        )
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL",
            workload_token="ALLTOALL",
            payload_semantics="HCCL_ALLTOALL_TOTAL_SEND_BYTES",
            reduction="NONE",
            traffic_algorithm="UNIFORM_DIRECT_EXCHANGE",
            message_bytes=400,
            projected_routing=routing,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(result["status"], "VALID")
        projected = result["results"]["projected_a2a_traffic"]
        self.assertEqual(projected["schema_version"], "simai.projected-a2a/v1alpha1")
        self.assertEqual(
            projected["capability"],
            {
                "backend": "ANALYTICAL",
                "endpoint_flows_materialized": False,
                "simulation_flow_support": "NOT_PROVIDED",
            },
        )
        self.assertEqual(projected["readiness"], "READY")
        self.assertEqual(projected["unit"], "B")
        self.assertEqual(projected["global_bytes"], 1200)
        self.assertEqual(
            projected["per_rank"],
            [
                {"rank": 0, "domain": "domain-0", "send_B": 300, "receive_B": 300},
                {"rank": 1, "domain": "domain-0", "send_B": 300, "receive_B": 300},
                {"rank": 2, "domain": "domain-1", "send_B": 300, "receive_B": 300},
                {"rank": 3, "domain": "domain-1", "send_B": 300, "receive_B": 300},
            ],
        )
        self.assertEqual(
            projected["per_domain"],
            [
                {"domain": "domain-0", "send_B": 600, "receive_B": 600},
                {"domain": "domain-1", "send_B": 600, "receive_B": 600},
            ],
        )
        self.assertEqual(
            projected["domain_matrix_B"], [[200, 400], [400, 200]]
        )
        self.assertEqual(
            projected["resource_loads"],
            [
                {"id": "intra-domain-fabric", "scope": "INTRA_DOMAIN", "offered_load_B": 400},
                {"id": "inter-domain-fabric", "scope": "INTER_DOMAIN", "offered_load_B": 800},
            ],
        )
        self.assertEqual(
            projected["conservation"],
            {
                "global_equals_rank_send": True,
                "global_equals_rank_receive": True,
                "global_equals_domain_matrix": True,
                "global_equals_resource_loads": True,
                "domain_rows_equal_send": True,
                "domain_columns_equal_receive": True,
                "status": "PASS",
            },
        )
        self.assertEqual(
            projected["resident_state"],
            {
                "rank_summaries": 4,
                "domain_matrix_cells": 4,
                "resource_summaries": 2,
                "resident_dense_routing_cells": 0,
                "resident_endpoint_flows": 0,
                "resident_state_units": 10,
                "complexity": "O(P + D^2 + R)",
            },
        )
        self.assertRegex(projected["provenance"]["routing_sha256"], SHA256_ID)
        self.assertEqual(projected["provenance"]["policy"], "UNIFORM")
        self.assertEqual(
            projected["input_cost"]["artifact_bytes_read"],
            len((json.dumps(routing, indent=2) + "\n").encode()),
        )
        self.assertEqual(projected["input_cost"]["routing_records_read"], 0)
        self.assertGreaterEqual(projected["input_cost"]["parse_projection_time_ns"], 0)
        self.assertEqual(
            projected["determinism"],
            {
                "ordering": "RANK_ASCENDING_DOMAIN_DECLARATION_RESOURCE_DECLARATION",
                "semantic_inputs": "CONTENT_ADDRESSED",
                "observed_timing_fields_excluded_from_semantic_identity": True,
            },
        )
        self.assertEqual(result["readiness"]["projected_a2a"], "READY")
        self.assertEqual(result["evidence"]["projected_a2a"]["level"], "USER_INPUT")
        self.assertEqual(
            result["evidence"]["projected_a2a"]["readiness"],
            "FIELD_UNVERIFIED",
        )

    def test_projected_a2a_is_unknown_when_second_hccl_request_invalidates_run(self):
        routing = self.projected_routing(
            {"kind": "UNIFORM", "messageBytesPerRank": 400}
        )
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL",
            workload_token="ALLTOALL",
            payload_semantics="HCCL_ALLTOALL_TOTAL_SEND_BYTES",
            reduction="NONE",
            traffic_algorithm="UNIFORM_DIRECT_EXCHANGE",
            message_bytes=400,
            projected_routing=routing,
            layer_count=2,
        )

        self.assertEqual(completed.returncode, 3, completed.stderr)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(
            result["reject_code"], "HCCL_MULTIPLE_REQUESTS_UNSUPPORTED"
        )
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["projected_a2a"], "BLOCKED")
        self.assertEqual(result["results"]["projected_a2a_traffic"], "UNKNOWN")

    def test_projected_locality_dense_a2av_matches_enumerated_ground_truth(self):
        counts = [
            [0, 80, 10, 10],
            [70, 0, 20, 10],
            [10, 10, 0, 80],
            [20, 10, 70, 0],
        ]
        routing = self.projected_routing(
            {"kind": "DENSE_COUNTS", "scenario": "LOCALITY", "sendCounts": counts}
        )
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=100,
            routing_matrix=counts,
            projected_routing=routing,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        projected = result["results"]["projected_a2a_traffic"]
        self.assertEqual(projected["global_bytes"], 400)
        self.assertEqual(
            [(rank["send_B"], rank["receive_B"]) for rank in projected["per_rank"]],
            [(100, 100), (100, 100), (100, 100), (100, 100)],
        )
        self.assertEqual(projected["domain_matrix_B"], [[150, 50], [50, 150]])
        self.assertEqual(
            [resource["offered_load_B"] for resource in projected["resource_loads"]],
            [300, 100],
        )
        self.assertEqual(projected["conservation"]["status"], "PASS")
        self.assertEqual(
            projected["input_cost"]["artifact_format"], "IMMUTABLE_DENSE_JSON"
        )
        self.assertEqual(projected["input_cost"]["routing_records_read"], 16)
        self.assertEqual(projected["provenance"]["scenario"], "LOCALITY")
        self.assertEqual(projected["resident_state"]["resident_dense_routing_cells"], 16)
        self.assertEqual(
            projected["resident_state"]["complexity"], "O(P^2 + D^2 + R)"
        )

    def test_projected_hotspot_dense_a2av_matches_enumerated_ground_truth(self):
        counts = [
            [0, 70, 20, 10],
            [0, 0, 90, 10],
            [0, 80, 0, 20],
            [0, 70, 30, 0],
        ]
        routing = self.projected_routing(
            {"kind": "DENSE_COUNTS", "scenario": "HOTSPOT", "sendCounts": counts}
        )
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=100,
            routing_matrix=counts,
            projected_routing=routing,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        projected = result["results"]["projected_a2a_traffic"]
        self.assertEqual(projected["global_bytes"], 400)
        self.assertEqual(
            [rank["send_B"] for rank in projected["per_rank"]], [100, 100, 100, 100]
        )
        self.assertEqual(
            [rank["receive_B"] for rank in projected["per_rank"]], [0, 220, 140, 40]
        )
        self.assertEqual(projected["domain_matrix_B"], [[70, 130], [150, 50]])
        self.assertEqual(
            [(domain["send_B"], domain["receive_B"]) for domain in projected["per_domain"]],
            [(200, 220), (200, 180)],
        )
        self.assertEqual(
            [resource["offered_load_B"] for resource in projected["resource_loads"]],
            [120, 280],
        )
        self.assertEqual(
            projected["imbalance"],
            {
                "hottest_receive_rank": 1,
                "maximum_rank_receive_B": 220,
                "mean_rank_receive_B": 100,
                "maximum_to_mean_receive_ratio": 2.2,
            },
        )
        self.assertEqual(projected["conservation"]["status"], "PASS")

    def test_projected_uniform_100000_rank_real_process_has_bounded_state(self):
        rank_count = 100_000
        domains = [
            {"id": f"domain-{index:03d}", "firstRank": index * 1000, "rankCount": 1000}
            for index in range(100)
        ]
        routing = self.projected_routing(
            {"kind": "UNIFORM", "messageBytesPerRank": rank_count},
            rank_count=rank_count,
            domains=domains,
        )
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL",
            workload_token="ALLTOALL",
            payload_semantics="HCCL_ALLTOALL_TOTAL_SEND_BYTES",
            reduction="NONE",
            traffic_algorithm="UNIFORM_DIRECT_EXCHANGE",
            message_bytes=rank_count,
            projected_routing=routing,
            rank_count=rank_count,
            timeout=60,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr[-2000:])
        self.assertLess(completed.wall_seconds, 60)
        self.assertLess(completed.result_bytes, 32 * 1024 * 1024)
        projected = result["results"]["projected_a2a_traffic"]
        self.assertEqual(projected["global_bytes"], 9_999_900_000)
        self.assertEqual(len(projected["per_rank"]), rank_count)
        self.assertEqual(projected["per_rank"][0]["send_B"], 99_999)
        self.assertEqual(projected["per_rank"][-1]["receive_B"], 99_999)
        self.assertEqual(len(projected["domain_matrix_B"]), 100)
        self.assertEqual(projected["domain_matrix_B"][0][0], 999_000)
        self.assertEqual(projected["domain_matrix_B"][0][1], 1_000_000)
        self.assertEqual(
            [resource["offered_load_B"] for resource in projected["resource_loads"]],
            [99_900_000, 9_900_000_000],
        )
        self.assertEqual(
            projected["resident_state"],
            {
                "rank_summaries": 100_000,
                "domain_matrix_cells": 10_000,
                "resource_summaries": 2,
                "resident_dense_routing_cells": 0,
                "resident_endpoint_flows": 0,
                "resident_state_units": 110_002,
                "complexity": "O(P + D^2 + R)",
            },
        )
        self.assertEqual(
            projected["uniform_closed_form"],
            {
                "directed_pairs_represented": 9_999_900_000,
                "directed_pairs_materialized": 0,
            },
        )
        self.assertEqual(projected["input_cost"]["routing_records_read"], 0)
        self.assertEqual(projected["conservation"]["status"], "PASS")

    def test_projected_a2a_artifacts_fail_closed_at_public_process_boundary(self):
        uniform = self.projected_routing(
            {"kind": "UNIFORM", "messageBytesPerRank": 400}
        )
        cases = [
            (
                "non-regular",
                {"projected_routing": uniform, "projected_via_stdin": True},
                "PROJECTED_A2A_ROUTING_NOT_REGULAR",
            ),
            (
                "digest-mismatch",
                {"projected_routing": uniform, "projected_digest_override": "sha256:" + "0" * 64},
                "PROJECTED_A2A_ROUTING_DIGEST_MISMATCH",
            ),
            (
                "malformed",
                {"projected_routing": uniform, "projected_content_override": "{"},
                "PROJECTED_A2A_ROUTING_INVALID_JSON",
            ),
            (
                "over-limit",
                {"projected_routing": uniform, "projected_padding_bytes": 8 * 1024 * 1024},
                "PROJECTED_A2A_ROUTING_ARTIFACT_TOO_LARGE",
            ),
            (
                "unknown-key",
                {
                    "projected_routing": uniform,
                    "mutate_projected": lambda document: document["spec"].update({"flows": []}),
                },
                "PROJECTED_A2A_ROUTING_SCHEMA_INVALID",
            ),
            (
                "membership-gap",
                {
                    "projected_routing": uniform,
                    "mutate_projected": lambda document: document["spec"]["domains"][0].update({"firstRank": 1}),
                },
                "PROJECTED_A2A_MEMBERSHIP_INVALID",
            ),
            (
                "evidence-unresolved",
                {
                    "projected_routing": uniform,
                    "mutate_projected": lambda document: document["spec"].update({"evidenceRef": "missing"}),
                },
                "PROJECTED_A2A_EVIDENCE_INVALID",
            ),
        ]
        for name, kwargs, expected_code in cases:
            with self.subTest(name=name):
                completed, result = self.run_generated_hccl_contract(
                    collective="ALL_TO_ALL",
                    workload_token="ALLTOALL",
                    payload_semantics="HCCL_ALLTOALL_TOTAL_SEND_BYTES",
                    reduction="NONE",
                    traffic_algorithm="UNIFORM_DIRECT_EXCHANGE",
                    message_bytes=400,
                    **kwargs,
                )
                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["status"], "INVALID_INPUT")
                self.assertEqual(result["reject_code"], expected_code)
                self.assertEqual(result["results"]["projected_a2a_traffic"], "UNKNOWN")

        counts = [
            [0, 80, 10, 10],
            [70, 0, 20, 10],
            [10, 10, 0, 80],
            [20, 10, 70, 0],
        ]
        divergent = copy.deepcopy(counts)
        divergent[0][1], divergent[0][2] = divergent[0][2], divergent[0][1]
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=100,
            routing_matrix=counts,
            projected_routing=self.projected_routing(
                {"kind": "DENSE_COUNTS", "scenario": "ARBITRARY", "sendCounts": divergent}
            ),
        )
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"], "PROJECTED_A2A_DENSE_BINDING_MISMATCH"
        )

    def test_alltoallv_uses_routing_counts_for_payload_and_traffic(self):
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=[
                [0, 100000, 200000, 300000],
                [150000, 0, 250000, 350000],
                [175000, 225000, 0, 275000],
                [325000, 125000, 225000, 0],
            ],
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["results"]["timing_ns"], 50000)
        self.assertEqual(result["results"]["traffic_B"], 2700000)
        self.assertEqual(result["results"]["collective"]["operation"], "ALL_TO_ALL_V")
        payload = result["results"]["collective_payload"]
        self.assertEqual(payload["semantics"], "HCCL_ALLTOALLV_COUNTS_MATRIX")
        self.assertEqual(payload["input_B_per_rank"], 750000)
        self.assertEqual(payload["output_B_per_rank"], 925000)
        self.assertRegex(payload["routing_sha256"], SHA256_ID)
        self.assertEqual(
            payload["routing_sha256"], result["provenance"]["routing_sha256"]
        )
        self.assertEqual(result["readiness"]["routing"], "READY")

    def test_alltoallv_without_routing_is_unknown_and_never_falls_back(self):
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=[
                [0, 100000, 200000, 300000],
                [150000, 0, 250000, 350000],
                [175000, 225000, 0, 275000],
                [325000, 125000, 225000, 0],
            ],
            include_routing=False,
        )

        self.assertEqual(completed.returncode, 3)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(result["reject_code"], "HCCL_ROUTING_REQUIRED")
        self.assertEqual(result["readiness"]["routing"], "UNKNOWN")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

    def test_alltoallv_routing_artifact_has_preparse_byte_limit(self):
        routing_matrix = [
            [0, 100000, 200000, 300000],
            [150000, 0, 250000, 350000],
            [175000, 225000, 0, 275000],
            [325000, 125000, 225000, 0],
        ]
        within_completed, within_result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=routing_matrix,
            routing_padding_bytes=900000,
        )
        self.assertEqual(within_completed.returncode, 0)
        self.assertEqual(within_result["status"], "VALID")

        over_completed, over_result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=routing_matrix,
            routing_padding_bytes=1100000,
        )
        self.assertEqual(over_completed.returncode, 2)
        self.assertEqual(over_result["status"], "INVALID_INPUT")
        self.assertEqual(
            over_result["reject_code"], "HCCL_ROUTING_ARTIFACT_TOO_LARGE"
        )
        self.assertEqual(over_result["results"]["timing_ns"], "UNKNOWN")

    def test_alltoallv_routing_artifact_exact_byte_limit(self):
        routing_matrix = [
            [0, 100000, 200000, 300000],
            [150000, 0, 250000, 350000],
            [175000, 225000, 0, 275000],
            [325000, 125000, 225000, 0],
        ]
        at_limit_completed, at_limit_result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=routing_matrix,
            routing_total_bytes=1024 * 1024,
        )
        self.assertEqual(at_limit_completed.returncode, 0)
        self.assertEqual(at_limit_result["status"], "VALID")

        over_limit_completed, over_limit_result = (
            self.run_generated_hccl_contract(
                collective="ALL_TO_ALL_V",
                workload_token="ALLTOALLV",
                payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
                reduction="NONE",
                traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
                message_bytes=750000,
                routing_matrix=routing_matrix,
                routing_total_bytes=1024 * 1024 + 1,
            )
        )
        self.assertEqual(over_limit_completed.returncode, 2)
        self.assertEqual(over_limit_result["status"], "INVALID_INPUT")
        self.assertEqual(
            over_limit_result["reject_code"],
            "HCCL_ROUTING_ARTIFACT_TOO_LARGE",
        )

    def test_alltoallv_nonseekable_routing_cannot_bypass_byte_limit(self):
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=[
                [0, 100000, 200000, 300000],
                [150000, 0, 250000, 350000],
                [175000, 225000, 0, 275000],
                [325000, 125000, 225000, 0],
            ],
            routing_total_bytes=1101348,
            routing_via_stdin=True,
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"], "HCCL_ROUTING_ARTIFACT_TOO_LARGE"
        )
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_alltoallv_routing_rank_above_dense_limit_is_rejected(self):
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=[
                [0, 100000, 200000, 300000],
                [150000, 0, 250000, 350000],
                [175000, 225000, 0, 275000],
                [325000, 125000, 225000, 0],
            ],
            routing_rank_override=257,
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"], "HCCL_ROUTING_CAPACITY_EXCEEDED"
        )
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

    def test_alltoallv_routing_cells_above_dense_limit_is_rejected(self):
        cells_completed, cells_result = self.run_generated_hccl_contract(
            collective="ALL_TO_ALL_V",
            workload_token="ALLTOALLV",
            payload_semantics="HCCL_ALLTOALLV_COUNTS_MATRIX",
            reduction="NONE",
            traffic_algorithm="VARIABLE_DIRECT_EXCHANGE",
            message_bytes=750000,
            routing_matrix=[[0] * 257 for _ in range(256)],
        )
        self.assertEqual(cells_completed.returncode, 2)
        self.assertEqual(cells_result["status"], "INVALID_INPUT")
        self.assertEqual(
            cells_result["reject_code"], "HCCL_ROUTING_CAPACITY_EXCEEDED"
        )
        self.assertEqual(cells_result["results"]["traffic_B"], "UNKNOWN")

    def test_segmented_known_points_boundaries_and_monotonicity(self):
        expected = {
            4096: 10205,
            65535: 13277,
            65536: 13277,
            1048576: 37853,
        }
        observed = []
        for message_bytes, expected_duration in expected.items():
            with self.subTest(message_bytes=message_bytes):
                completed, result = self.run_segmented_allgather_contract(
                    message_bytes
                )
                self.assertEqual(completed.returncode, 0)
                self.assertEqual(result["status"], "VALID")
                self.assertEqual(
                    result["results"]["timing_ns"], expected_duration
                )
                observed.append(result["results"]["timing_ns"])
        self.assertEqual(observed, sorted(observed))

    def test_segmented_model_with_one_ns_discrete_drop_is_rejected(self):
        def lower_second_segment(model):
            model["spec"]["fit"]["segments"][1]["startup"]["value"] = 11638

        completed, result = self.run_segmented_allgather_contract(
            65536, mutate_model=lower_second_segment
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(
            result["reject_code"], "HCCL_COST_MODEL_NON_MONOTONIC"
        )
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_all_collectives_share_fixed_profile_message_matrix(self):
        cases = [
            (
                "ALL_REDUCE",
                "ALLREDUCE",
                "HCCL_ALLREDUCE_IN_PLACE_BUFFER_BYTES",
                "SUM",
                "RING",
                6,
                lambda size: size,
            ),
            (
                "ALL_GATHER",
                "ALLGATHER",
                "HCCL_ALLGATHER_SEND_BYTES",
                "NONE",
                "RING_ALL_GATHER",
                12,
                lambda size: 4 * size,
            ),
            (
                "REDUCE_SCATTER",
                "REDUCESCATTER",
                "HCCL_REDUCESCATTER_INPUT_BYTES",
                "SUM",
                "RING_REDUCE_SCATTER",
                3,
                lambda size: size // 4,
            ),
            (
                "ALL_TO_ALL",
                "ALLTOALL",
                "HCCL_ALLTOALL_TOTAL_SEND_BYTES",
                "NONE",
                "UNIFORM_DIRECT_EXCHANGE",
                3,
                lambda size: size,
            ),
            (
                "ALL_TO_ALL_V",
                "ALLTOALLV",
                "HCCL_ALLTOALLV_COUNTS_MATRIX",
                "NONE",
                "VARIABLE_DIRECT_EXCHANGE",
                4,
                lambda size: size,
            ),
        ]
        for (
            collective,
            workload_token,
            payload_semantics,
            reduction,
            traffic_algorithm,
            traffic_multiplier,
            expected_output,
        ) in cases:
            durations = []
            for message_bytes in (4096, 1048576):
                routing_matrix = None
                if collective == "ALL_TO_ALL_V":
                    routing_matrix = [
                        [0, message_bytes, 0, 0],
                        [0, 0, message_bytes, 0],
                        [0, 0, 0, message_bytes],
                        [message_bytes, 0, 0, 0],
                    ]
                with self.subTest(
                    collective=collective, message_bytes=message_bytes
                ):
                    completed, result = self.run_generated_hccl_contract(
                        collective=collective,
                        workload_token=workload_token,
                        payload_semantics=payload_semantics,
                        reduction=reduction,
                        traffic_algorithm=traffic_algorithm,
                        message_bytes=message_bytes,
                        routing_matrix=routing_matrix,
                    )
                    self.assertEqual(completed.returncode, 0)
                    self.assertEqual(result["status"], "VALID")
                    self.assertEqual(
                        result["results"]["collective"]["operation"],
                        collective,
                    )
                    self.assertEqual(
                        result["results"]["collective_payload"]["semantics"],
                        payload_semantics,
                    )
                    self.assertEqual(
                        result["results"]["collective_payload"][
                            "output_B_per_rank"
                        ],
                        expected_output(message_bytes),
                    )
                    self.assertEqual(
                        result["results"]["traffic_B"],
                        traffic_multiplier * message_bytes,
                    )
                    self.assertEqual(
                        result["results"]["timing_ns"],
                        round(
                            20000
                            + message_bytes / 25000000000 * 1000000000
                        ),
                    )
                    durations.append(result["results"]["timing_ns"])
            self.assertEqual(durations, sorted(durations))

    def test_second_hccl_request_fails_closed_in_single_request_result_schema(self):
        completed, result = self.run_generated_hccl_contract(
            collective="ALL_GATHER",
            workload_token="ALLGATHER",
            payload_semantics="HCCL_ALLGATHER_SEND_BYTES",
            reduction="NONE",
            traffic_algorithm="RING_ALL_GATHER",
            layer_count=2,
        )

        self.assertEqual(completed.returncode, 3)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(
            result["reject_code"], "HCCL_MULTIPLE_REQUESTS_UNSUPPORTED"
        )
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")
        self.assertEqual(result["results"]["collective_payload"], "UNKNOWN")

    def test_explicit_legacy_busbw_adapter_converts_to_canonical_algbw(self):
        completed, result = self.run_mutated_ascend_contract(
            mutate_model=self.use_explicit_legacy_busbw_adapter
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["reject_code"], "NONE")
        self.assertEqual(result["results"]["timing_ns"], 61943)
        self.assertEqual(result["results"]["traffic_B"], 6291456)
        self.assertEqual(result["provenance"]["cost_model"], "HCCL_DERIVED")
        self.assertEqual(
            result["provenance"]["cost_model_adapter"],
            "EXPLICIT_LEGACY_BUSBW",
        )

    def test_legacy_busbw_without_explicit_adapter_is_rejected(self):
        def remove_adapter(model):
            self.use_explicit_legacy_busbw_adapter(model)
            model["spec"]["fit"].pop("adapter")

        completed, result = self.run_mutated_ascend_contract(
            mutate_model=remove_adapter
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "LEGACY_BUSBW_ADAPTER_REQUIRED")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_legacy_busbw_missing_column_is_rejected(self):
        def remove_bandwidth_column(model):
            self.use_explicit_legacy_busbw_adapter(model)
            model["spec"]["fit"]["adapter"]["columns"].remove(
                "bus_bandwidth_Bps"
            )

        completed, result = self.run_mutated_ascend_contract(
            mutate_model=remove_bandwidth_column
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "LEGACY_BUSBW_COLUMN_MISSING")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

    def test_legacy_busbw_ambiguous_unit_is_rejected(self):
        def use_ambiguous_bandwidth_unit(model):
            self.use_explicit_legacy_busbw_adapter(model)
            model["spec"]["fit"]["adapter"]["busBandwidth"]["unit"] = "GB/s"

        completed, result = self.run_mutated_ascend_contract(
            mutate_model=use_ambiguous_bandwidth_unit
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "LEGACY_BUSBW_UNIT_AMBIGUOUS")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_legacy_busbw_out_of_domain_row_is_rejected(self):
        def use_mismatched_message_domain(model):
            self.use_explicit_legacy_busbw_adapter(model)
            model["spec"]["fit"]["adapter"]["messageDomainBytes"][
                "min"
            ] = 524288

        completed, result = self.run_mutated_ascend_contract(
            mutate_model=use_mismatched_message_domain
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "LEGACY_BUSBW_DOMAIN_MISMATCH")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

    def test_missing_topology_is_distinct_unknown_and_never_falls_back(self):
        completed, result = self.run_mutated_ascend_contract(
            mutate_profile=lambda profile: profile["spec"].pop("topology")
        )

        self.assertEqual(completed.returncode, 3)
        self.assertEqual(result["status"], "UNSUPPORTED")
        self.assertEqual(result["reject_code"], "HCCL_TOPOLOGY_REQUIRED")
        self.assertEqual(result["readiness"]["topology"], "UNKNOWN")
        self.assertEqual(result["readiness"]["routing"], "NOT_REQUIRED")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

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
        self.assertEqual(result["readiness"]["topology"], "READY")
        self.assertEqual(result["readiness"]["routing"], "NOT_REQUIRED")

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

    def test_hccl_duration_outside_signed_rounding_range_fails_closed(self):
        completed, result = self.run_mutated_ascend_contract(
            mutate_model=lambda model: model["spec"]["fit"]["bandwidth"].update(
                {"value": 0.0001}
            )
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "HCCL_COST_MODEL_NUMERIC_OVERFLOW")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")
        self.assertEqual(result["results"]["traffic_B"], "UNKNOWN")

    def test_profile_noncanonical_consumed_unit_is_rejected(self):
        def use_noncanonical_compute_unit(profile):
            profile["spec"]["compute"]["capabilities"][0]["peakFLOPsPerS"][
                "unit"
            ] = "TFLOP/s"

        completed, result = self.run_mutated_ascend_contract(
            mutate_profile=use_noncanonical_compute_unit
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "DEVICE_PROFILE_UNIT_INVALID")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["ascend_profile"], "BLOCKED")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_hccl_model_formula_change_is_rejected(self):
        completed, result = self.run_mutated_ascend_contract(
            mutate_model=lambda model: model["spec"]["fit"].update(
                {"formula": "startup_ns + message_B"}
            )
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "HCCL_COST_MODEL_FORMULA_INVALID")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["hccl_cost_model"], "BLOCKED")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_allreduce_present_algorithm_and_statistics_must_match_model(self):
        def inject_conflicting_metadata(raw):
            raw["spec"]["algorithm"] = {
                "name": "TREE_NOT_RING",
                "version": "synthetic-conflict-v1",
            }
            raw["spec"]["statistics"] = {
                "timingStatistic": "MINIMUM",
                "sampleCount": 5,
                "warmupExcluded": False,
            }

        completed, result = self.run_mutated_ascend_contract(
            mutate_raw=inject_conflicting_metadata
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "RAW_OBSERVATION_SCHEMA_INVALID")
        self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_field_unverified_model_cannot_claim_measured_evidence(self):
        completed, result = self.run_mutated_ascend_contract(
            mutate_model=lambda model: model["spec"].update(
                {"evidenceClass": "MEASURED"}
            )
        )

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(result["status"], "INVALID_INPUT")
        self.assertEqual(result["reject_code"], "EVIDENCE_READINESS_CONFLICT")
        self.assertEqual(result["readiness"]["contract"], "BLOCKED")
        self.assertEqual(result["readiness"]["hccl_cost_model"], "BLOCKED")
        self.assertEqual(result["results"]["validity"], "UNKNOWN")

    def test_consumed_profile_field_requires_resolved_evidence_and_readiness(self):
        def remove_readiness(profile):
            del profile["spec"]["identity"]["physicalChipCount"]["readiness"]

        def use_unknown_readiness(profile):
            profile["spec"]["identity"]["physicalChipCount"][
                "readiness"
            ] = "UNKNOWN"

        def use_unresolved_evidence(profile):
            profile["spec"]["identity"]["physicalChipCount"][
                "evidenceRef"
            ] = "missing-evidence"

        for mutation in (
            remove_readiness,
            use_unknown_readiness,
            use_unresolved_evidence,
        ):
            with self.subTest(mutation=mutation.__name__):
                completed, result = self.run_mutated_ascend_contract(
                    mutate_profile=mutation
                )
                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["status"], "INVALID_INPUT")
                self.assertEqual(
                    result["reject_code"],
                    "DEVICE_PROFILE_FIELD_EVIDENCE_INVALID",
                )
                self.assertEqual(result["readiness"]["contract"], "BLOCKED")
                self.assertEqual(
                    result["readiness"]["ascend_profile"], "BLOCKED"
                )
                self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

    def test_raw_observation_must_match_model_and_normalized_bandwidth(self):
        def change_observed_time(raw):
            raw["spec"]["normalized"]["averageTime"]["value"] = 70000

        def change_normalized_bandwidth(raw):
            raw["spec"]["normalized"]["algBandwidth"]["value"] = 1

        for mutation in (change_observed_time, change_normalized_bandwidth):
            with self.subTest(mutation=mutation.__name__):
                completed, result = self.run_mutated_ascend_contract(
                    mutate_raw=mutation
                )
                self.assertEqual(completed.returncode, 2)
                self.assertEqual(result["status"], "INVALID_INPUT")
                self.assertEqual(
                    result["reject_code"],
                    "RAW_OBSERVATION_MODEL_INCONSISTENT",
                )
                self.assertEqual(result["readiness"]["contract"], "BLOCKED")
                self.assertEqual(
                    result["readiness"]["hccl_cost_model"], "BLOCKED"
                )
                self.assertEqual(result["results"]["timing_ns"], "UNKNOWN")

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
