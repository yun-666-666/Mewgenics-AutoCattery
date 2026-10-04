"""Compare serial/parallel native experiments and completed-arm resume."""

import argparse
import json
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import time


def run(args, output, jobs, resume=False):
    start = time.perf_counter()
    subprocess.run([sys.executable, "-u", "tools/breeding_trait_experiment.py",
                    "--game", str(args.game), "--save", str(args.save),
                    "--output", str(output), "--days", str(args.days),
                    "--seeds", "7123", "--jobs", str(jobs),
                    *(["--resume"] if resume else [])], check=True)
    return time.perf_counter() - start


def observations(directory):
    result = {}
    for path in directory.glob("seed-*"):
        if path.suffix == ".checkpoint":
            with path.open("rb") as saved:
                result[path.name] = pickle.load(saved)
        elif path.suffix == ".json":
            result[path.name] = json.loads(path.read_text(encoding="utf-8"))
        else:
            result[path.name] = path.read_bytes()
    result["results"] = json.loads((directory / "results.json").read_text(encoding="utf-8"))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--save", type=Path, required=True)
    parser.add_argument("--days", type=int, default=2)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="simulator-scheduling-", dir=args.output.parent))
    serial, parallel = root / "serial", root / "parallel"
    serial_seconds = run(args, serial, 1)
    expected = observations(serial)
    parallel_seconds = run(args, parallel, 2)
    actual = observations(parallel)
    assert expected == actual, "native observations or checkpoint states changed with concurrency"
    resume_seconds = run(args, parallel, 2, resume=True)
    assert observations(parallel) == actual, "resume changed completed arm contents"
    timing = json.loads((parallel / "execution_timing.json").read_text(encoding="utf-8"))
    assert all(seconds == 0 for seconds in timing["arm_seconds"].values())
    report = {"days_per_arm": args.days, "serial_seconds": serial_seconds,
              "parallel_seconds": parallel_seconds, "speedup": serial_seconds / parallel_seconds,
              "resume_seconds": resume_seconds, "completed_arms_reused": True,
              "native_results_and_checkpoints_equal": True, "observations": str(root)}
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
