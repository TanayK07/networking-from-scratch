# Networking from Scratch — top-level Makefile
#
# `make` walks every phase, `make -C` for one phase, etc.

PHASES := $(sort $(wildcard phases/*))
LESSONS := $(sort $(shell find phases -mindepth 2 -maxdepth 2 -type d))

.PHONY: all clean test test-c build lint type lessons help

help:
	@echo "Networking from Scratch"
	@echo ""
	@echo "Targets:"
	@echo "  make all       — build every C lesson under phases/"
	@echo "  make test      — run every test (C unit, pytest)"
	@echo "  make lint      — ruff + shellcheck + clang-tidy"
	@echo "  make type      — mypy --strict on Python sources"
	@echo "  make clean     — remove all build artifacts"
	@echo ""
	@echo "Per-lesson:"
	@echo "  make -C phases/02-link-layer/15-raw-sockets-on-linux"
	@echo "  make -C phases/02-link-layer/15-raw-sockets-on-linux test"

all: lessons
build: lessons

lessons:
	@for d in $(LESSONS); do \
	  if [ -f "$$d/Makefile" ]; then \
	    echo ">> $$d"; \
	    $(MAKE) -C "$$d" || exit 1; \
	  fi; \
	done

test:
	@for d in $(LESSONS); do \
	  if [ -f "$$d/Makefile" ] && [ -d "$$d/tests" ]; then \
	    echo ">> test $$d"; \
	    $(MAKE) -C "$$d" test || exit 1; \
	  fi; \
	done
	@echo ">> pytest"
	tox -e py312

test-c:
	@for d in $(LESSONS); do \
	  if [ -f "$$d/Makefile" ] && [ -d "$$d/tests" ]; then \
	    echo ">> test $$d"; \
	    $(MAKE) -C "$$d" test || exit 1; \
	  fi; \
	done

lint:
	tox -e lint
	@find phases -name '*.sh' -print0 | xargs -0 -r shellcheck

type:
	tox -e type

clean:
	@for d in $(LESSONS); do \
	  if [ -f "$$d/Makefile" ]; then \
	    $(MAKE) -C "$$d" clean 2>/dev/null || true; \
	  fi; \
	done
	rm -rf outputs/pcaps/* outputs/lkms/* outputs/bpf/*
	rm -rf .tox .pytest_cache **/__pycache__
