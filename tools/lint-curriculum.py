#!/usr/bin/env python3
"""Lint a lesson directory for the six-section invariant.

Usage:
    python tools/lint-curriculum.py phases/02-link-layer/15-raw-sockets-on-linux
    python tools/lint-curriculum.py --all   # lint every lesson under phases/

Exits non-zero on any violation.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REQUIRED_SECTIONS: list[str] = [
    "## 1. Problem",
    "## 2. Theory",
    "## 3. Math / Spec",
    "## 4. Code",
    "## 5. Tests",
    "## 6. Exercises",
]


def lint_lesson(lesson_dir: Path) -> list[str]:
    """Return a list of human-readable problems for one lesson directory."""
    problems: list[str] = []

    readme = lesson_dir / "README.md"
    if not readme.is_file():
        problems.append(f"missing README.md")
        return problems

    text = readme.read_text(encoding="utf-8")

    # 1. All six sections, in order.
    last_pos = -1
    for section in REQUIRED_SECTIONS:
        pos = text.find(section)
        if pos < 0:
            problems.append(f"missing section: {section!r}")
        elif pos < last_pos:
            problems.append(f"section out of order: {section!r}")
        else:
            last_pos = pos

    # 2. exercises.md should exist OR section 6 should have inline exercises.
    exercises_md = lesson_dir / "exercises.md"
    if not exercises_md.is_file():
        ex_section = text.split("## 6. Exercises", 1)
        if len(ex_section) < 2 or len(ex_section[1].strip()) < 80:
            problems.append("section 6 is too short and exercises.md missing")

    # 3. tests/ directory expected unless this is a notes-only lesson.
    has_code = any(lesson_dir.glob("*.c")) or any(lesson_dir.glob("*.py"))
    if has_code and not (lesson_dir / "tests").is_dir():
        problems.append("has code but no tests/ directory")

    # 4. Lesson title should match `# NN. Title` on first line.
    first_line = text.lstrip().splitlines()[0] if text.strip() else ""
    if not re.match(r"^# \d{2,3}\. ", first_line):
        problems.append(f"first line should be '# NN. Title', got {first_line!r}")

    # 5. One-sentence promise on the line(s) right after the title.
    if "> " not in text[: text.find("## 1. Problem") if "## 1. Problem" in text else 200]:
        problems.append("missing one-sentence promise (blockquote after title)")

    return problems


def find_lessons(root: Path) -> list[Path]:
    """All `phases/NN-*/MM-*/` directories that look like lessons."""
    out: list[Path] = []
    for phase in sorted((root / "phases").iterdir()):
        if not phase.is_dir():
            continue
        for lesson in sorted(phase.iterdir()):
            if not lesson.is_dir():
                continue
            # Skip "capstones" and "shared" subdirs that don't follow the NN-slug pattern.
            if not re.match(r"^\d{2,3}-", lesson.name):
                continue
            out.append(lesson)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="*", help="Lesson directories to lint.")
    ap.add_argument("--all", action="store_true", help="Lint every lesson.")
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repo root (default: parent of tools/)",
    )
    args = ap.parse_args()

    if args.all:
        targets = find_lessons(args.root)
    else:
        targets = [Path(p) for p in args.paths]

    if not targets:
        ap.error("Pass lesson dirs or --all")

    failures = 0
    for t in targets:
        problems = lint_lesson(t)
        if problems:
            failures += 1
            print(f"FAIL {t}")
            for p in problems:
                print(f"  - {p}")
        else:
            print(f"ok   {t}")

    if failures:
        print(f"\n{failures} lesson(s) failed lint", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
