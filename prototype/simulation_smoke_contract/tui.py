#!/usr/bin/env python3
"""Tiny TUI for driving the throwaway Simulation smoke state model."""

from contract import SmokeState, micro_matrix, reduce


BOLD = "\033[1m"
DIM = "\033[2m"
RESET = "\033[0m"


def render(state: SmokeState, show_matrix: bool) -> None:
    print("\033[2J\033[H", end="")
    print(f"{BOLD}PROTOTYPE — 16 ranks / 2 logical domains{RESET}")
    print(f"{BOLD}status:{RESET} {state.status}")
    print(f"{BOLD}structural_pass:{RESET} {state.structural_pass}")
    print(f"{BOLD}timing_within_30pct:{RESET} {state.timing_within_30pct}/{state.timing_cases}")
    print(f"{BOLD}e2e_cases:{RESET} {state.e2e_cases}")
    print(f"{BOLD}e2e_ordering_matches:{RESET} {state.e2e_ordering_matches}")
    print(f"{BOLD}micro_cases:{RESET} {len(micro_matrix())}")
    if show_matrix:
        print(f"\n{BOLD}Micro matrix{RESET}")
        for op, size, pattern in micro_matrix():
            print(f"{op:14} {size:>9} B  {pattern}")
    print(f"\n{BOLD}[s]{RESET} {DIM}structural pass{RESET}  "
          f"{BOLD}[t]{RESET} {DIM}timing pass{RESET}  "
          f"{BOLD}[d]{RESET} {DIM}inject disagreement{RESET}  "
          f"{BOLD}[m]{RESET} {DIM}toggle matrix{RESET}  "
          f"{BOLD}[r]{RESET} {DIM}reset{RESET}  "
          f"{BOLD}[q]{RESET} {DIM}quit{RESET}")


def main() -> None:
    state = SmokeState()
    show_matrix = False
    actions = {"s": "structural_pass", "t": "timing_pass", "d": "timing_disagreement", "r": "reset"}
    while True:
        render(state, show_matrix)
        key = input("> ").strip().lower()[:1]
        if key == "q":
            return
        if key == "m":
            show_matrix = not show_matrix
        elif key in actions:
            state = reduce(state, actions[key])


if __name__ == "__main__":
    main()
