#!/usr/bin/env python3
"""SKYFELL test runner — the project's gate output.

Discovers tests/lua/test_*.lua, prepends the harness to each (no require/dofile
inside the Mesen sandbox), runs each against the TEST ROM via MesenCE's
headless testrunner, and prints the pass/fail table recorded in
docs/CONTINUATION.md. Exit code 0 iff every test passes.

Invocation per test (proven in sibling prophet's Phase 0; switches NEED the --
prefix or MesenCE silently treats them as missing files, and --enablestdout is
what surfaces emu.log lines on stdout):
  Mesen.exe --testrunner --enablestdout --timeout=120 <rom> <combined.lua>
"""

import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ROM = REPO / "build" / "skyfell-test.sfc"
HARNESS = REPO / "tests" / "lua" / "lib" / "harness.lua"
TESTS_DIR = REPO / "tests" / "lua"
ARTIFACTS = REPO / "artifacts"
MESEN = Path(os.environ.get("MESEN_EXE", "C:/Users/LCM/snesdev/mesence/Mesen.exe"))
TIMEOUT_S = 240  # subprocess belt-and-braces; Mesen's own timeout=120 fires first

# MesenCE is a framework-dependent net10.0 app; the runtime lives user-local
# on this machine (no system dotnet). Without DOTNET_ROOT the apphost pops an
# invisible install dialog and hangs (prophet's Phase 0 lesson).
_home = os.environ.get("USERPROFILE") or os.environ.get("HOME") or "C:/Users/LCM"
MESEN_ENV = dict(os.environ)
MESEN_ENV.setdefault("DOTNET_ROOT", str(Path(_home) / "AppData/Local/Microsoft/dotnet"))


def main() -> int:
    if not ROM.exists():
        print(f"ERROR: {ROM} not found - run `make test` (it builds the TEST ROM first)")
        return 2
    if not MESEN.exists():
        print(f"ERROR: MesenCE not found at {MESEN} (set MESEN_EXE)")
        return 2

    tests = sorted(TESTS_DIR.glob("test_*.lua"))
    if not tests:
        print("ERROR: no tests found in tests/lua/")
        return 2

    harness_src = HARNESS.read_text(encoding="utf-8")
    results = []

    for test in tests:
        name = test.stem
        outdir = ARTIFACTS / name
        outdir.mkdir(parents=True, exist_ok=True)
        # H.fail writes fail.txt only on failure — clear the previous run's,
        # or a later pass leaves a stale failure lying around to mislead.
        # Same for the flake-retry autopsy log (written only when rc<0).
        (outdir / "fail.txt").unlink(missing_ok=True)
        (outdir / "run.flake1.log").unlink(missing_ok=True)

        # Mesen sets its own cwd to its home folder, so the harness needs an
        # absolute artifacts path for screenshots — injected as OUTDIR.
        combined = outdir / "_combined.lua"
        combined.write_text(
            f"OUTDIR = [[{outdir.as_posix()}]]\n"
            + harness_src + "\n-- ==== " + test.name + " ====\n" + test.read_text(encoding="utf-8"),
            encoding="utf-8",
        )

        cmd = [str(MESEN), "--testrunner", "--enablestdout", "--timeout=120", str(ROM), str(combined)]

        def run_once():
            try:
                proc = subprocess.run(
                    cmd, cwd=outdir, capture_output=True, text=True, timeout=TIMEOUT_S,
                    env=MESEN_ENV,
                )
                return proc.returncode, (proc.stdout or "") + (proc.stderr or "")
            except subprocess.TimeoutExpired:
                return -1, "(runner timeout)"

        rc, log = run_once()
        retried = False
        if rc < 0:
            # Documented sequential-Mesen flake (prophet DECISIONS D-062):
            # under a full sequential suite, occasional runs drop exactly ONE
            # test at rc=-1 (emulator death/timeout, NOT a Lua assert —
            # asserts exit with their positive stop code and are NEVER
            # retried). Retry ONCE, loudly; the first attempt's log is kept.
            (outdir / "run.flake1.log").write_text(log, encoding="utf-8")
            print(f"  {name:<28} rc={rc} (emulator died, not an assert) - "
                  f"RETRYING ONCE [sequential-Mesen flake mitigation]")
            rc, log = run_once()
            retried = True

        (outdir / "run.log").write_text(log, encoding="utf-8")
        results.append((name, rc, retried))
        status = "PASS" if rc == 0 else f"FAIL (rc={rc})"
        if retried:
            status += " (on retry)" if rc == 0 else " (retry did not save it)"
        print(f"  {name:<28} {status}")
        if rc != 0:
            tail = "\n".join(log.strip().splitlines()[-6:])
            if tail:
                print("    " + "\n    ".join(tail.splitlines()))

    passed = sum(1 for _, rc, _r in results if rc == 0)
    retries = sum(1 for _, _rc, r in results if r)
    if retries:
        # Loud, but ABOVE the count line — the gate contract reads the
        # runner's own LAST line for the pass count.
        names = ", ".join(n for n, _rc, r in results if r)
        print(f"NOTE: {retries} flake retr{'y' if retries == 1 else 'ies'} "
              f"(rc<0 emulator death, retried once): {names} - "
              f"first-attempt logs in artifacts/<test>/run.flake1.log")
    print("-" * 40)
    print(f"{passed}/{len(results)} tests passed")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
