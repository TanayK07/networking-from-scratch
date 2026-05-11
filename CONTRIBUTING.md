# Contributing to Networking from Scratch

Thank you for considering a contribution. This curriculum lives or dies by community work — every lesson written, test added, or typo fixed makes it better for the next person.

## Ways to contribute

### 1. Write a lesson

The single most valuable contribution. Pick any planned lesson from the [catalog](https://networking-from-scratch.com/catalog.html), claim it on the [project board](https://github.com/TanayK07/networking-from-scratch/projects/1), then:

```bash
# Scaffold the lesson directory
python3 tools/new-lesson.py --phase 03-network-layer --num 11 \
    --slug ip-options --lang c --title "IP options"

# This creates:
#   phases/03-network-layer/11-ip-options/
#   ├── README.md                    (with the 6-section template)
#   ├── ip_options.c
#   ├── ip_options.h
#   ├── Makefile
#   ├── tests/
#   │   ├── test_ip_options.c
#   │   └── test_ip_options.py
#   └── exercises.md
```

Then write the lesson, following [`docs/style-guide.md`](docs/style-guide.md). Open a PR.

### 2. Add tests to an existing lesson

Existing lessons are improved by:
- More **property tests** (round-trip, idempotence, monotonicity).
- **Fuzz harnesses** for parsers (AFL++, libFuzzer).
- **Interop tests** that run our code against the Linux kernel, OpenSSL, Cyclone DDS, etc.

### 3. Fix a bug or improve prose

Small fixes are welcome. Open a PR with a clear title (`fix(P4.27): off-by-one in CUBIC window growth`).

### 4. Translate

We accept translations as sibling files: `README.es.md`, `README.zh.md`, etc. One PR per language per lesson.

## Lesson invariants (enforced by CI)

The lint script (`tools/lint-curriculum.py`) checks every lesson directory. Your PR must satisfy:

- [ ] `README.md` has all 6 sections in the correct order: Problem, Theory, Math/Spec, Code, Tests, Exercises
- [ ] At least one source file in C (`*.c`/`*.h`) or Python (`*.py`) matching the declared language
- [ ] A `Makefile` (for C lessons) that supports `all`, `test`, `clean`
- [ ] A `tests/` directory with at least one passing test
- [ ] An `exercises.md` with at least 5 graded exercises (★ markers)
- [ ] No external dependencies beyond what's in `tools/setup-vm.sh`
- [ ] All code compiles with `-Wall -Wextra -Werror -std=c11` for C
- [ ] All Python passes `ruff check` and `mypy --strict`

## Building and testing locally

```bash
# Build everything
make build

# Test everything
make test

# Test just one lesson
make -C phases/03-network-layer/02-internet-checksum test

# Python tests
tox

# Lint the curriculum structure
python3 tools/lint-curriculum.py
```

## Pull request checklist

- [ ] One lesson or one logical change per PR
- [ ] CI green (`c-build`, `python-tests`, `lint-curriculum`)
- [ ] Commit messages follow `<type>(<scope>): <description>` (e.g. `feat(P3.07): IP fragmentation reassembly`)
- [ ] Tests pass on a fresh `tools/setup-vm.sh` Ubuntu 24.04 VM
- [ ] Updated the catalog data in `website/catalog.html` if you added or renamed a lesson

## Style

- **Prose**: clear, direct, no hype. Don't oversell. Say "this implements RFC 826 ARP" not "this revolutionary implementation."
- **Code**: simple over clever. A junior engineer should be able to read it.
- **Comments**: explain *why*, not *what*. The code says what.
- **No frameworks until you've built one yourself.** This is the core philosophy.

## Code of Conduct

We follow the [Contributor Covenant 2.1](CODE_OF_CONDUCT.md). Be kind. Disagree with ideas, not with people.

## License

By contributing, you agree your contribution is licensed under [MIT](LICENSE).

## Questions

Open a [Discussion](https://github.com/TanayK07/networking-from-scratch/discussions) or file an [issue](https://github.com/TanayK07/networking-from-scratch/issues/new/choose).
