#!/usr/bin/env python3
"""
Final release cleanup for marukitano/past-present-future.

Run this file from the repository root:

    python3 finalize_release.py

The script:
- switches the watchface from seconds back to minutes;
- removes the demo-mode branches;
- performs conservative code cleanup;
- makes app_settings_save internal;
- cleans release metadata and .gitignore;
- creates README.md when none exists;
- removes tracked image assets not referenced by package.json;
- updates package-lock.json when present;
- runs git diff --check and pebble build;
- restores every touched file automatically if an error occurs.

Options:
    --no-build             Apply cleanup without running the Pebble build.
    --keep-unused-assets   Do not remove unreferenced tracked image assets.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, Optional


class CleanupError(RuntimeError):
    pass


def run(
    command: Iterable[str],
    *,
    cwd: Path,
    check: bool = True,
    capture: bool = False,
) -> subprocess.CompletedProcess[str]:
    command_list = list(command)

    print("+", " ".join(command_list))

    result = subprocess.run(
        command_list,
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )

    if check and result.returncode != 0:
        detail = ""

        if capture:
            output = "\n".join(
                part.strip()
                for part in (result.stdout, result.stderr)
                if part and part.strip()
            )
            if output:
                detail = f"\n{output}"

        raise CleanupError(
            f"Command failed ({result.returncode}): "
            f"{' '.join(command_list)}{detail}"
        )

    return result


class Transaction:
    def __init__(self) -> None:
        self._original: Dict[Path, Optional[bytes]] = {}

    def remember(self, path: Path) -> None:
        path = path.resolve()

        if path in self._original:
            return

        self._original[path] = (
            path.read_bytes()
            if path.exists()
            else None
        )

    def write_text(self, path: Path, content: str) -> None:
        self.remember(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def delete(self, path: Path) -> None:
        self.remember(path)

        if path.exists():
            path.unlink()

    def rollback(self) -> None:
        print("\nAn error occurred. Restoring all touched files...")

        for path, original in reversed(list(self._original.items())):
            if original is None:
                if path.exists():
                    path.unlink()
                continue

            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(original)

        print("Rollback complete.")


def require_file(root: Path, relative_path: str) -> Path:
    path = root / relative_path

    if not path.is_file():
        raise CleanupError(f"Required file is missing: {relative_path}")

    return path


def replace_once(
    text: str,
    old: str,
    new: str,
    description: str,
) -> str:
    count = text.count(old)

    if count != 1:
        raise CleanupError(
            f"{description}: expected exactly one match, found {count}."
        )

    return text.replace(old, new, 1)


def replace_regex_once(
    text: str,
    pattern: str,
    replacement: str,
    description: str,
    *,
    flags: int = 0,
) -> str:
    updated, count = re.subn(
        pattern,
        replacement,
        text,
        count=1,
        flags=flags,
    )

    if count != 1:
        raise CleanupError(
            f"{description}: expected exactly one match, found {count}."
        )

    return updated


def ensure_repository(root: Path) -> None:
    package_path = require_file(root, "package.json")
    package = json.loads(package_path.read_text(encoding="utf-8"))

    if package.get("name") != "past-present-future":
        raise CleanupError(
            "This does not look like the past-present-future repository."
        )

    if not (root / ".git").exists():
        raise CleanupError("Run the script from the Git repository root.")


def ensure_no_tracked_changes(root: Path) -> None:
    unstaged = run(
        ["git", "diff", "--quiet"],
        cwd=root,
        check=False,
    )
    staged = run(
        ["git", "diff", "--cached", "--quiet"],
        cwd=root,
        check=False,
    )

    if unstaged.returncode != 0 or staged.returncode != 0:
        raise CleanupError(
            "Tracked files already contain local changes. "
            "Commit, push, stash or restore them before running this script. "
            "Untracked files such as this cleanup script are allowed."
        )


def cleanup_main_window(
    root: Path,
    transaction: Transaction,
) -> None:
    path = require_file(root, "src/windows/main_window.c")
    text = path.read_text(encoding="utf-8")

    demo_comment_pattern = (
        r"/\*\n"
        r" \* Für schnelles Testen auf 1 setzen:.*?"
        r"\*/\n"
        r"#define PPF_EFFECT_DEMO_MODE\s+[01]\n\n"
    )

    if "PPF_EFFECT_DEMO_MODE" in text:
        text = replace_regex_once(
            text,
            demo_comment_pattern,
            "",
            "Remove demo-mode declaration",
            flags=re.DOTALL,
        )

    clean_tick_handler = """static void tick_handler(
    struct tm *tick_time,
    TimeUnits units_changed
) {
  (void)units_changed;

  info_display_update(tick_time);

  time_row_set_value(
      &s_hour_row,
      tick_time->tm_hour,
      true
  );

  time_row_set_value(
      &s_minute_row,
      tick_time->tm_min,
      true
  );
}
"""

    tick_pattern = (
        r"static void tick_handler\(\n"
        r".*?"
        r"\n\}\n"
        r"(?=\n\nstatic BitmapLayer \*create_bitmap_layer)"
    )

    text = replace_regex_once(
        text,
        tick_pattern,
        clean_tick_handler,
        "Replace demo tick handler with minute handler",
        flags=re.DOTALL,
    )

    if "static const AppSettings *s_settings;" in text:
        text = text.replace(
            "static const AppSettings *s_settings;\n\n",
            "",
            1,
        )

        text = replace_once(
            text,
            """  s_settings = app_settings_get();

  create_column_layers(root_layer);
""",
            """  const AppSettings *settings =
      app_settings_get();

  create_column_layers(root_layer);
""",
            "Use local settings pointer",
        )

        text = text.replace(
            "      s_settings\n  );",
            "      settings\n  );",
        )

    subscription_pattern = (
        r"#if PPF_EFFECT_DEMO_MODE\n"
        r"  tick_timer_service_subscribe\(\n"
        r"      SECOND_UNIT,\n"
        r"      tick_handler\n"
        r"  \);\n"
        r"#else\n"
        r"  tick_timer_service_subscribe\(\n"
        r"      MINUTE_UNIT,\n"
        r"      tick_handler\n"
        r"  \);\n"
        r"#endif"
    )

    if "#if PPF_EFFECT_DEMO_MODE" in text:
        text = replace_regex_once(
            text,
            subscription_pattern,
            """  tick_timer_service_subscribe(
      MINUTE_UNIT,
      tick_handler
  );""",
            "Replace demo tick subscription",
        )

    text = text.replace(
        """  set_shake_subscription(false);
  cancel_wrist_shake_timer();

  if (s_inverter_layer) {
""",
        """  set_shake_subscription(false);

  if (s_inverter_layer) {
""",
        1,
    )

    text = text.replace(
        """  s_dark_mode = false;
  s_wrist_shake_locked = false;

  app_settings_set_changed_handler(NULL);
""",
        """  s_dark_mode = false;

  app_settings_set_changed_handler(NULL);
""",
        1,
    )

    old_overlay_comment = """  /*
   * PRESENT_SWISS_PROTECTED_OVERLAY
   *
   * Diese Ebene liegt oberhalb des Inverters.
   * Dadurch bleiben das rote Feld und das weisse
   * Kreuz unverändert.
   */
"""

    if old_overlay_comment in text:
        text = text.replace(
            old_overlay_comment,
            """  /*
   * Keeps the red emblem and white cross unchanged
   * above the black/white inverter.
   */
""",
            1,
        )

    forbidden = (
        "PPF_EFFECT_DEMO_MODE",
        "tick_time->tm_sec",
        "SECOND_UNIT",
        "s_settings",
    )

    leftovers = [token for token in forbidden if token in text]

    if leftovers:
        raise CleanupError(
            "Demo-mode cleanup is incomplete: "
            + ", ".join(leftovers)
        )

    if "MINUTE_UNIT" not in text:
        raise CleanupError(
            "Minute tick subscription is missing after cleanup."
        )

    transaction.write_text(path, text)


def cleanup_app_settings(
    root: Path,
    transaction: Transaction,
) -> None:
    header_path = require_file(
        root,
        "src/settings/app_settings.h",
    )
    source_path = require_file(
        root,
        "src/settings/app_settings.c",
    )

    grep = run(
        ["git", "grep", "-n", "app_settings_save", "--", "src"],
        cwd=root,
        check=False,
        capture=True,
    )

    references = [
        line
        for line in grep.stdout.splitlines()
        if line.strip()
    ]

    allowed_files = {
        "src/settings/app_settings.h",
        "src/settings/app_settings.c",
    }

    unexpected = []

    for line in references:
        filename = line.split(":", 1)[0]
        if filename not in allowed_files:
            unexpected.append(line)

    if unexpected:
        raise CleanupError(
            "app_settings_save is used outside app_settings.*:\n"
            + "\n".join(unexpected)
        )

    header = header_path.read_text(encoding="utf-8")

    declaration_pattern = (
        r"void app_settings_save\(\n"
        r"    const AppSettings \*settings\n"
        r"\);\n\n"
    )

    if "app_settings_save" in header:
        header = replace_regex_once(
            header,
            declaration_pattern,
            "",
            "Remove public app_settings_save declaration",
        )

    source = source_path.read_text(encoding="utf-8")

    if "void app_settings_save(" in source:
        source = replace_once(
            source,
            """void app_settings_save(
    const AppSettings *settings
) {
""",
            """static void save_settings(
    const AppSettings *settings
) {
""",
            "Make settings save function internal",
        )

    source = source.replace(
        "    app_settings_save(&updated);",
        "    save_settings(&updated);",
    )

    source = source.replace(
        """static AppSettingsChangedHandler
    s_changed_handler;
""",
        "static AppSettingsChangedHandler s_changed_handler;\n",
        1,
    )

    if "app_settings_save" in source or "app_settings_save" in header:
        raise CleanupError(
            "app_settings_save remained after internalization."
        )

    transaction.write_text(header_path, header)
    transaction.write_text(source_path, source)


def update_package_metadata(
    root: Path,
    transaction: Transaction,
) -> dict:
    path = require_file(root, "package.json")
    package = json.loads(path.read_text(encoding="utf-8"))

    pebble = package.setdefault("pebble", {})
    pebble["displayName"] = "Past Present Future"

    package["license"] = "MIT"
    package["repository"] = {
        "type": "git",
        "url": (
            "https://github.com/"
            "marukitano/past-present-future.git"
        ),
    }

    keywords = package.setdefault("keywords", [])

    for keyword in (
        "pebble-app",
        "pebble-time-2",
        "watchface",
    ):
        if keyword not in keywords:
            keywords.append(keyword)

    contributors = package.setdefault("contributors", [])

    if "marukitano" not in contributors:
        contributors.append("marukitano")

    transaction.write_text(
        path,
        json.dumps(package, indent=2) + "\n",
    )

    return package


def write_gitignore(
    root: Path,
    transaction: Transaction,
) -> None:
    path = root / ".gitignore"

    content = """build/
node_modules/
.lock-waf*
*.pbw
*.pyc
__pycache__/
"""

    transaction.write_text(path, content)


def create_readme_if_missing(
    root: Path,
    transaction: Transaction,
) -> None:
    path = root / "README.md"

    if path.exists():
        print("README.md already exists; leaving it unchanged.")
        return

    content = """# Past Present Future

A Pebble Time 2 watchface showing the previous, current and next hour and minute.

## Features

- Built for Pebble Time 2 / Emery
- Light, dark and shake-to-toggle themes
- Pixel Wave and Bounce animations
- Optional date and current temperature
- Optional Swiss emblem
- Clay configuration

## Build

```bash
npm install
pebble build
```

Install through the local Pebble development connection:

```bash
pebble install --phone PHONE_IP
```

## Credits

Original project by Chris Lewis.

Pebble Time 2 port, rendering changes, configuration, weather, themes and
additional artwork by marukitano.

## License

MIT. See [LICENSE](LICENSE).
"""

    transaction.write_text(path, content)


def referenced_resource_paths(
    root: Path,
    package: dict,
) -> set[Path]:
    media = (
        package.get("pebble", {})
        .get("resources", {})
        .get("media", [])
    )

    referenced: set[Path] = set()

    for entry in media:
        file_name = entry.get("file")
        if not file_name:
            continue

        referenced.add(
            (root / "resources" / file_name).resolve()
        )

    return referenced


def remove_unused_image_assets(
    root: Path,
    transaction: Transaction,
    package: dict,
) -> list[Path]:
    referenced = referenced_resource_paths(root, package)

    tracked = run(
        ["git", "ls-files", "-z", "resources/images"],
        cwd=root,
        capture=True,
    ).stdout.split("\0")

    removed: list[Path] = []

    for item in tracked:
        if not item:
            continue

        path = (root / item).resolve()

        if path in referenced:
            continue

        transaction.delete(path)
        removed.append(path)

    images_root = root / "resources" / "images"

    if images_root.exists():
        directories = sorted(
            (
                path
                for path in images_root.rglob("*")
                if path.is_dir()
            ),
            key=lambda item: len(item.parts),
            reverse=True,
        )

        for directory in directories:
            try:
                directory.rmdir()
            except OSError:
                pass

    return removed


def update_package_lock(
    root: Path,
    transaction: Transaction,
) -> None:
    lock_path = root / "package-lock.json"

    if not lock_path.exists():
        print("No package-lock.json found; skipping lock update.")
        return

    npm = shutil.which("npm")

    if not npm:
        raise CleanupError(
            "package-lock.json exists, but npm is not available."
        )

    transaction.remember(lock_path)

    run(
        [
            npm,
            "install",
            "--package-lock-only",
            "--ignore-scripts",
            "--no-audit",
            "--no-fund",
        ],
        cwd=root,
    )


def validate_cleanup(root: Path) -> None:
    run(
        ["git", "diff", "--check"],
        cwd=root,
    )

    result = run(
        [
            "git",
            "grep",
            "-nE",
            "PPF_EFFECT_DEMO_MODE|tm_sec|SECOND_UNIT",
            "--",
            "src",
        ],
        cwd=root,
        check=False,
        capture=True,
    )

    if result.returncode == 0:
        raise CleanupError(
            "Test-mode leftovers were found:\n"
            + result.stdout
        )

    package = json.loads(
        (root / "package.json").read_text(encoding="utf-8")
    )

    files = {
        (root / "resources" / item["file"]).resolve()
        for item in (
            package["pebble"]["resources"]["media"]
        )
        if "file" in item
    }

    missing = [
        str(path.relative_to(root))
        for path in files
        if not path.exists()
    ]

    if missing:
        raise CleanupError(
            "Referenced resources are missing:\n"
            + "\n".join(missing)
        )


def build_project(root: Path) -> None:
    pebble = shutil.which("pebble")

    if not pebble:
        raise CleanupError(
            "The Pebble CLI is not available in PATH."
        )

    run([pebble, "clean"], cwd=root)
    run([pebble, "build"], cwd=root)


def print_summary(
    root: Path,
    removed_assets: list[Path],
    build_enabled: bool,
) -> None:
    print("\nRelease cleanup completed successfully.")

    if removed_assets:
        print("\nRemoved unused tracked image assets:")
        for path in removed_assets:
            print("  -", path.relative_to(root))
    else:
        print("\nNo unused tracked image assets were removed.")

    status = run(
        ["git", "status", "--short"],
        cwd=root,
        capture=True,
    ).stdout.rstrip()

    print("\nGit status:")
    print(status or "  clean")

    print("\nNext commands:")

    if not build_enabled:
        print("  pebble clean")
        print("  pebble build")

    print("  pebble install --phone 192.168.1.106")
    print("  git add -A")
    print('  git commit -m "Prepare final Pebble Time 2 release"')
    print("  git push")
    print('  git tag -a v1.1.0 -m "Past Present Future v1.1.0"')
    print("  git push origin v1.1.0")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Prepare past-present-future for its final "
            "Pebble Time 2 release."
        )
    )

    parser.add_argument(
        "--no-build",
        action="store_true",
        help="Do not run pebble clean/build.",
    )

    parser.add_argument(
        "--keep-unused-assets",
        action="store_true",
        help=(
            "Keep tracked files below resources/images "
            "that package.json does not reference."
        ),
    )

    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    root = Path.cwd().resolve()
    transaction = Transaction()

    try:
        ensure_repository(root)
        ensure_no_tracked_changes(root)

        cleanup_main_window(root, transaction)
        cleanup_app_settings(root, transaction)
        package = update_package_metadata(root, transaction)
        write_gitignore(root, transaction)
        create_readme_if_missing(root, transaction)

        removed_assets: list[Path] = []

        if not args.keep_unused_assets:
            removed_assets = remove_unused_image_assets(
                root,
                transaction,
                package,
            )

        update_package_lock(root, transaction)
        validate_cleanup(root)

        if not args.no_build:
            build_project(root)

        print_summary(
            root,
            removed_assets,
            not args.no_build,
        )

        return 0

    except (
        CleanupError,
        OSError,
        ValueError,
        subprocess.SubprocessError,
    ) as error:
        transaction.rollback()
        print(f"\nERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
