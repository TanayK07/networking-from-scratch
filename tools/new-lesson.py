#!/usr/bin/env python3
"""Scaffold a new lesson directory with the six-section README.

Usage:
    python tools/new-lesson.py 02-link-layer 15 raw-sockets-on-linux --lang c
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

README_TEMPLATE = """# {nn}. {title}

> One-sentence promise: by the end of this lesson, you will have built X
> and understood Y.

## 1. Problem

[Concrete, observable problem this lesson solves. One paragraph.]

## 2. Theory

[Plain-prose explanation. Diagram if it helps.]

## 3. Math / Spec

[Formal definition. Quote the relevant RFC section with a permalink.]

## 4. Code

[Walk through the implementation. Annotated code blocks, not a code dump.]

## 5. Tests

[How we verify the implementation: unit, property, interop, fuzz.]

## 6. Exercises

- ★ Warm-up exercise.
- ★★ Concept exercise.
- ★★★ Extension exercise.

See `exercises.md` for full text.
"""

EXERCISES_TEMPLATE = """# Exercises for {nn}. {title}

## ★ Warm-up

[Goal, pointer, test reference.]

## ★★ Concept

[Goal, pointer, test reference.]

## ★★★ Extension

[Goal, pointer, test reference.]
"""

C_MAKEFILE = """CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic -Wno-unused-parameter -Werror -std=c11 \\
           -I../../../common/c
LDFLAGS ?=
LDLIBS  ?=

BIN := lesson
SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean test asan

all: $(BIN)

$(BIN): $(OBJ)
\t$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

asan: CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address,undefined
asan: clean all

test: all
\t$(MAKE) -C tests

clean:
\trm -f $(OBJ) $(BIN)
\t-$(MAKE) -C tests clean
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("phase", help="e.g. 02-link-layer")
    ap.add_argument("nn", help="lesson number, e.g. 15")
    ap.add_argument("slug", help="lesson slug, e.g. raw-sockets-on-linux")
    ap.add_argument("--title", help="Lesson title (default: derived from slug)")
    ap.add_argument("--lang", choices=["c", "python", "notes"], default="c")
    args = ap.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    phase_dir = repo_root / "phases" / args.phase
    if not phase_dir.is_dir():
        print(f"phase dir does not exist: {phase_dir}", file=sys.stderr)
        return 1

    title = args.title or args.slug.replace("-", " ").title()
    nn = args.nn.zfill(2)

    lesson_dir = phase_dir / f"{nn}-{args.slug}"
    if lesson_dir.exists():
        print(f"lesson already exists: {lesson_dir}", file=sys.stderr)
        return 1

    lesson_dir.mkdir(parents=True)
    (lesson_dir / "tests").mkdir()

    (lesson_dir / "README.md").write_text(README_TEMPLATE.format(nn=nn, title=title))
    (lesson_dir / "exercises.md").write_text(EXERCISES_TEMPLATE.format(nn=nn, title=title))

    if args.lang == "c":
        (lesson_dir / "Makefile").write_text(C_MAKEFILE)
        (lesson_dir / "lesson.c").write_text("/* TODO: implement */\nint main(void) { return 0; }\n")
    elif args.lang == "python":
        (lesson_dir / "lesson.py").write_text('"""TODO: implement."""\n')

    print(f"created {lesson_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
