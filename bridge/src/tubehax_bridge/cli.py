"""tubehax-bridge command line.

  run          the daemon (launchd runs this)
  hook         Claude Code hook entry point (stdin: hook JSON)
  statusline   Claude Code status line entry point (stdin: status JSON, prints the status line)
  install      launchd agent + hooks + status line;  uninstall: remove them
  status       is the daemon running, what does it publish
"""

import sys


def main() -> int:
    command = sys.argv[1] if len(sys.argv) > 1 else "help"
    # Hooks and the status line run on every Claude event: import nothing heavy for them.
    if command == "hook":
        from .client import hook

        return hook()
    if command == "statusline":
        from .client import statusline

        return statusline()
    if command == "run":
        from .daemon import run

        return run(verbose="-v" in sys.argv)
    if command == "install":
        from .install import install

        return install()
    if command == "uninstall":
        from .install import uninstall

        return uninstall()
    if command == "status":
        return status()
    print(__doc__.strip())
    return 0 if command in ("help", "-h", "--help") else 2


def status() -> int:
    import subprocess

    from .paths import LAUNCHD_LABEL, LOG_FILE, SOCKET_PATH

    import os

    result = subprocess.run(["launchctl", "print", f"gui/{os.getuid()}/{LAUNCHD_LABEL}"], capture_output=True, text=True)
    state = next((l.strip() for l in result.stdout.splitlines() if l.strip().startswith("state =")), "not loaded")
    print(f"launchd: {state}")
    print(f"socket: {'present' if SOCKET_PATH.exists() else 'missing'} ({SOCKET_PATH})")
    if LOG_FILE.exists():
        print("last log lines:")
        print("".join(LOG_FILE.read_text().splitlines(keepends=True)[-5:]), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
