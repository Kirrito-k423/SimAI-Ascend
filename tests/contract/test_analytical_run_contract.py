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

    def run_generated_legacy_contract(
        self,
        workload_token,
        *,
        layer_id="legacy_layer",
        input_gradient_token="NONE",
        weight_gradient_token="NONE",
    ):
        manifest = json.loads(
            (FIXTURES / "minimal_legacy_gpu_run.json").read_text()
        )
        with tempfile.TemporaryDirectory(prefix="simai-contract-") as temp_dir:
            run_directory = self.prepare_run_directory(temp_dir)
            workload_path = run_directory / "legacy-workload.txt"
            workload_path.write_text(
                "HYBRID_TRANSFORMER model_parallel_NPU_group: 1 ep: 1 pp: 1 "
                "vpp: 1 ga: 1 all_gpus: 1 checkpoints: 0 "
                "checkpoint_initiates: 0 pp_comm 0\n"
                "1\n"
                f"{layer_id}\t-1\t10\t{workload_token}\t1048576"
                f"\t10\t{input_gradient_token}\t0"
                f"\t10\t{weight_gradient_token}\t0\t10\n"
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
    ):
        """Build immutable synthetic artifacts, then observe the real process."""
        profile_path = FIXTURES / "minimal_ascend_profile.json"
        profile_digest = "sha256:" + hashlib.sha256(
            profile_path.read_bytes()
        ).hexdigest()
        duration_ns = round(20000 + message_bytes / 25000000000 * 1000000000)
        raw = {
            "apiVersion": "simai.ascend.observation/v1alpha1",
            "kind": "HcclRawSample",
            "schemaSemver": "0.1.0",
            "metadata": {"id": f"synthetic-{collective.lower()}-point"},
            "spec": {
                "profileRef": "synthetic-a2-four-rank-host",
                "profileDigest": profile_digest,
                "collective": collective,
                "group": {
                    "rankCount": 4,
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
                        "rankCounts": [4],
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
                "HYBRID_TRANSFORMER model_parallel_NPU_group: 4 ep: 1 pp: 1 "
                "vpp: 1 ga: 1 all_gpus: 4 checkpoints: 0 "
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
                input=routing_content if routing_via_stdin else None,
                timeout=30,
                check=False,
            )
            result = json.loads(result_path.read_text())
        return completed, result

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
