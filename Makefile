# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

.DEFAULT_GOAL := help

# source: https://stackoverflow.com/questions/18136918/how-to-get-current-relative-directory-of-your-makefile#73509979
MAKEFILE_ABS_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

define PRINT_HELP_PYSCRIPT
import re, sys

for line in sys.stdin:
	match = re.match(r'^([0-9a-zA-Z_-]+):.*?## (.*)$$', line)
	if match:
		target, help = match.groups()
		print("%-20s %s" % (target, help))
endef
export PRINT_HELP_PYSCRIPT

# source: https://stackoverflow.com/questions/5618615/check-if-a-program-exists-from-a-makefile#25668869
#
# please note that there should not be any tabs before the "ifeq/endif"
# statement
.PHONY: verify-prerequisites
verify-prerequisites:
ifeq (, $(shell command -v clang-format 2> /dev/null))
	$(error ERROR: please install clang-format)
endif

.PHONY: check-code-formatting
# - globstar: enable recursive globbing via "**" in bash >= 4 (equivalent to
#             shopt -s globstar)
# - nullglob: when a glob does not match anything, return "" instead of the
#             literal glob text (equivalent to shopt -s globstar)
SHELL = /usr/bin/env bash -O globstar -O nullglob
check-code-formatting: verify-prerequisites ## Check if the code base is correctly formatted. Do not touch the files
	clang-format --Werror --style=file:.clang-format --ferror-limit=20 --dry-run "${MAKEFILE_ABS_DIR}"/src/*/*.{h,hpp,c,cpp}

.PHONY: reformat-code
# - globstar: enable recursive globbing via "**" in bash >= 4 (equivalent to
#             shopt -s globstar)
# - nullglob: when a glob does not match anything, return "" instead of the
#             literal glob text (equivalent to shopt -s globstar)
SHELL = /usr/bin/env bash -O globstar -O nullglob
reformat-code: verify-prerequisites ## Reformat the code base in the src directory. Can be used as pre-commit hook
	clang-format --Werror --style=file:.clang-format -i --verbose "${MAKEFILE_ABS_DIR}"/src/**/*.{h,hpp,c,cpp}

.PHONY: help
help:
	@python3 -c "$$PRINT_HELP_PYSCRIPT" < $(MAKEFILE_LIST)
