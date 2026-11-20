# run inside a titled console; types real CLI for the 50fps gif
import ctypes
import subprocess
import sys
import time
from pathlib import Path

ctypes.windll.kernel32.SetConsoleTitleW("hdrchain")

EXE = Path(r"C:\Project\portfolio\hdrchain\build\Release\hdrchain.exe")
CWD = Path(r"C:\Project\portfolio\hdrchain\build\Release")
CHAIN = CWD / "chain.hdrc"


def type_out(s: str, delay=0.028):
    sys.stdout.write("C:\\hdrchain> ")
    sys.stdout.flush()
    time.sleep(0.12)
    for ch in s:
        sys.stdout.write(ch)
        sys.stdout.flush()
        time.sleep(delay)
    sys.stdout.write("\n")
    sys.stdout.flush()
    time.sleep(0.18)


def run(args: list[str]):
    type_out(" ".join(args))
    r = subprocess.run([str(EXE), *args[1:]], cwd=str(CWD), text=True, capture_output=True)
    out = (r.stdout or "") + (r.stderr or "")
    if out and not out.endswith("\n"):
        out += "\n"
    sys.stdout.write(out)
    sys.stdout.flush()
    time.sleep(0.85)


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    if CHAIN.exists():
        CHAIN.unlink()
    time.sleep(1.2)
    run(["hdrchain", "genesis", "chain.hdrc", "coinbase"])
    run(["hdrchain", "append", "chain.hdrc", "pay-alice"])
    run(["hdrchain", "verify", "chain.hdrc"])
    run(["hdrchain", "show", "chain.hdrc"])
    time.sleep(2.4)


if __name__ == "__main__":
    main()
