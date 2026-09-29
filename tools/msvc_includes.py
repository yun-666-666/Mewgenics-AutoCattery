"""Keep Ninja's MSVC header dependencies independent of console encoding."""

import subprocess
import sys


def normalize(output):
    try:
        text = output.decode("utf-8")
    except UnicodeDecodeError:
        text = output.decode("mbcs")
    return "".join(
        "Note: including file: " + line.removeprefix("注意: 包含文件:").lstrip()
        if line.startswith("注意: 包含文件:") else line
        for line in text.splitlines(keepends=True)
    ).encode("utf-8")


if __name__ == "__main__":
    result = subprocess.run(sys.argv[1:], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    sys.stdout.buffer.write(normalize(result.stdout))
    sys.exit(result.returncode)
