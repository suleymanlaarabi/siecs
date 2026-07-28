BAKE_HOME := $(shell bake env | sed -n 's/^BAKE_HOME=//p')
DEPS_INCLUDE := -I$(BAKE_HOME)/include
QUIET_BAKE = grep -Ev '^\[[[:space:]]*(test|build|run|runall|[0-9]+%)|^cmd:|^path:'

.PHONY: clean bench test test-features test-c test-c-release test-cpp test-cpp-release test-leaks distr check-distr check-distr-standalone check-distr-cpp-standalone build-c build-c-release build-test build-test-release act-ci act-docs act

ACT ?= act
ACT_PLATFORM ?= ubuntu-latest=ghcr.io/catthehacker/ubuntu:act-latest

bench:
	@bake rebuild --cfg release >/dev/null
	@bake rebuild bench --cfg release >/dev/null
	@bake run bench --cfg release

clean:
	@rm -rf build-consumer-c build-consumer-cpp >/dev/null

test: clean test-features test-c test-c-release test-cpp test-cpp-release

test-features:
	@sh test/features.sh

build-c:
	@sh tools/rebuild_isolated.sh .

build-c-release:
	@sh tools/rebuild_isolated.sh . --cfg release

build-test:
	@bake rebuild test -r >/dev/null

build-test-release:
	@bake rebuild test -r --cfg release >/dev/null

test-c:
	@bash -o pipefail -c "bake test test 2>&1 | $(QUIET_BAKE)"

test-c-release:
	@bash -o pipefail -c "bake test test --cfg release 2>&1 | $(QUIET_BAKE)"

test-leaks:
	@sh tools/rebuild_isolated.sh . --cfg sanitize >/dev/null
	@sh tools/rebuild_isolated.sh test --cfg sanitize >/dev/null
	@bake rebuild example/c -r --cfg sanitize >/dev/null
	@bash -o pipefail -c "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 LSAN_OPTIONS=exitcode=23 bake run example/c --cfg sanitize 2>&1 | $(QUIET_BAKE)"

test-cpp:
	@bake rebuild test/cpp -r >/dev/null
	@bash -o pipefail -c "bake test test/cpp 2>&1 | $(QUIET_BAKE)"

test-cpp-release:
	@bake rebuild test/cpp -r --cfg release >/dev/null
	@bash -o pipefail -c "bake test test/cpp --cfg release 2>&1 | $(QUIET_BAKE)"

distr:
	@sh tools/rebuild_distr.sh

check-distr:
	set -e; \
	tmp_dir=$$(mktemp -d /tmp/siecs-check-distr.XXXXXX); \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	sh tools/rebuild_distr.sh "$$tmp_dir"; \
	diff -u distr/siecs.c "$$tmp_dir/distr/siecs.c"; \
	diff -u distr/siecs.h "$$tmp_dir/distr/siecs.h"; \
	diff -u distr/siecs_no_addons.c "$$tmp_dir/distr/siecs_no_addons.c"; \
	diff -u distr/siecs_no_addons.h "$$tmp_dir/distr/siecs_no_addons.h"; \
	diff -u include/siecs/bake_config.h "$$tmp_dir/include/siecs/bake_config.h"

check-distr-standalone:
	tmp_dir=$$(mktemp -d /tmp/siecs-distr.XXXXXX); \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	cp distr/siecs.c "$$tmp_dir/siecs.c"; \
	cp distr/siecs.h "$$tmp_dir/siecs.h"; \
	cd "$$tmp_dir"; \
	$(CC) -std=c23 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-function -pedantic -c siecs.c -o siecs-distr.o

check-distr-cpp-standalone:
	tmp_dir=$$(mktemp -d /tmp/siecs-distr-cpp.XXXXXX); \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	cp distr/siecs.c "$$tmp_dir/siecs.c"; \
	cp distr/siecs.h "$$tmp_dir/siecs.h"; \
	cp test/standalone/distr_cpp_standalone.cpp "$$tmp_dir/main.cpp"; \
	cd "$$tmp_dir"; \
	$(CC) -std=c23 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-function -pedantic -c siecs.c -o siecs.o; \
	$(CXX) -std=c++23 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-function -pedantic -I. main.cpp siecs.o -pthread -o siecs-cpp-standalone; \
	./siecs-cpp-standalone

act-ci:
	@$(ACT) push -W .github/workflows/ci-act.yml -P $(ACT_PLATFORM)

act-docs:
	@$(ACT) push -W .github/workflows/docs.yml -P $(ACT_PLATFORM)

act: act-ci act-docs
