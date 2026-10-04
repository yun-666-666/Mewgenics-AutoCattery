"""Build the external workbench with an official, isolated Windows Python runtime."""

import argparse
import ast
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.request
import zipfile


def source_files(tools):
    """Collect local imports, including the SWF readers used by cat visuals."""
    pending = [tools / "breeding_web.py"]
    included = set()
    while pending:
        path = pending.pop()
        if path in included:
            continue
        included.add(path)
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8-sig"))):
            names = ([alias.name for alias in node.names] if isinstance(node, ast.Import)
                     else [node.module] if isinstance(node, ast.ImportFrom) and node.module else [])
            for name in names:
                local = tools / (name.split(".")[0] + ".py")
                if local.is_file() and local not in included:
                    pending.append(local)
    return sorted(included)


def build(output, cache):
    root = Path(__file__).resolve().parents[1]
    output.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / "python-3.12.10-embed-amd64.zip"
    if not archive.exists():
        print("Downloading official Python 3.12.10 embedded runtime", flush=True)
        temporary = archive.with_suffix(".download")
        with urllib.request.urlopen(
                "https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip",
                timeout=60) as response, temporary.open("wb") as target:
            shutil.copyfileobj(response, target)
        temporary.replace(archive)
    wheels = cache / "wheels"
    wheels.mkdir(exist_ok=True)
    if not list(wheels.glob("pefile-2024.8.26-*.whl")) or not list(wheels.glob("unicorn-2.1.4-*.whl")):
        subprocess.run([
            sys.executable, "-m", "pip", "download", "--disable-pip-version-check",
            "--only-binary=:all:", "--no-deps", "--platform", "win_amd64",
            "--python-version", "312", "--implementation", "cp", "--abi", "cp312",
            "-r", str(root / "tools" / "breeding_requirements.txt"), "-d", str(wheels),
        ], check=True)
    runtime = output / "runtime"
    runtime.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        bundle.extractall(runtime)
    packages = runtime / "Lib" / "site-packages"
    packages.mkdir(parents=True, exist_ok=True)
    for wheel in sorted(wheels.glob("*.whl")):
        with zipfile.ZipFile(wheel) as bundle:
            bundle.extractall(packages)
    (runtime / "python312._pth").write_text(
        "python312.zip\n.\n../tools\nLib/site-packages\nimport site\n", encoding="utf-8")
    destination = output / "tools"
    destination.mkdir(exist_ok=True)
    sources = source_files(root / "tools")
    for path in sources + [root / "tools" / "breeding_web.html"]:
        shutil.copy2(path, destination / path.name)
    for name in ("StartBreeding.cmd", "README.md"):
        shutil.copy2(root / "assets" / "breeding" / name, output / name)
    subprocess.run([
        str(runtime / "python.exe"), "-B", "-X", "utf8", "-c",
        "import sqlite3, pefile, unicorn, breeding_web; "
        "from breeding_save_application import game_is_running; "
        "print('Bundled runtime ready; game_running=', game_is_running())",
    ], check=True)
    print(f"Workbench ready: {output} ({len(sources)} source modules)", flush=True)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=root / "dist" / "Release" / "AutoCatteryWorkbench")
    parser.add_argument("--cache", type=Path, default=root / ".local" / "breeding-runtime-cache")
    parser.add_argument("--zip", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    build(output, args.cache.resolve())
    if args.zip:
        args.zip.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.zip, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
            for path in sorted(output.rglob("*")):
                if path.is_file() and "__pycache__" not in path.parts:
                    bundle.write(path, Path(output.name) / path.relative_to(output))
        print(f"Archive ready: {args.zip}", flush=True)


if __name__ == "__main__":
    main()
