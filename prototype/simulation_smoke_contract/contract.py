"""Pure state transition logic for the throwaway smoke-contract prototype."""

from dataclasses import dataclass, replace


MESSAGE_BYTES = (4 * 1024, 1024 * 1024, 64 * 1024 * 1024)
COLLECTIVES = ("ALLREDUCE", "ALLGATHER", "REDUCESCATTER", "ALLTOALL", "ALLTOALLV")


@dataclass(frozen=True)
class SmokeState:
    structural_pass: bool = False
    timing_cases: int = 0
    timing_within_30pct: int = 0
    e2e_cases: int = 0
    e2e_ordering_matches: bool = False
    status: str = "PENDING"


def micro_matrix() -> tuple[tuple[str, int, str], ...]:
    regular = tuple((op, size, "UNIFORM") for op in COLLECTIVES for size in MESSAGE_BYTES)
    a2av_extra = (
        ("ALLTOALLV", 1024 * 1024, "LOCALITY"),
        ("ALLTOALLV", 1024 * 1024, "HOTSPOT"),
    )
    return regular + a2av_extra


def reduce(state: SmokeState, action: str) -> SmokeState:
    if action == "reset":
        return SmokeState()
    if action == "structural_pass":
        return replace(state, structural_pass=True, status="FLOW_SMOKE_PASS")
    if action == "timing_pass" and state.structural_pass:
        count = len(micro_matrix())
        return replace(
            state,
            timing_cases=count,
            timing_within_30pct=count,
            e2e_cases=2,
            e2e_ordering_matches=True,
            status="F3_SIMULATION_AUDITED",
        )
    if action == "timing_disagreement" and state.structural_pass:
        count = len(micro_matrix())
        return replace(
            state,
            timing_cases=count,
            timing_within_30pct=count - 1,
            e2e_cases=2,
            e2e_ordering_matches=False,
            status="SIMULATION_DISAGREEMENT",
        )
    if action in {"timing_pass", "timing_disagreement"}:
        return replace(state, status="INVALID_TRANSITION_NEEDS_STRUCTURAL_PASS")
    return state
