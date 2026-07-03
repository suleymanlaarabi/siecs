BAKE_HOME := $(shell bake env | sed -n 's/^BAKE_HOME=//p')
DEPS_INCLUDE := -I$(BAKE_HOME)/include
QUIET_BAKE = grep -Ev '^\[[[:space:]]*(test|build|run|runall|[0-9]+%)|^cmd:|^path:'

.PHONY: clean bench clean-rust test test-c test-c-release test-cpp test-cpp-release test-leaks test-rust update-rust-vendor distr check-distr check-distr-standalone

bench:
	@bake rebuild --cfg release >/dev/null
	@bake rebuild bench --cfg release >/dev/null
	@bake run bench --cfg release

clean-rust:
	@cd addons/siecs_rust && cargo clean >/dev/null

clean:
	@rm -rf build-consumer-c build-consumer-cpp >/dev/null

test: clean test-c test-c-release test-cpp test-cpp-release

test-c:
	@bake rebuild test -r >/dev/null
	@bash -o pipefail -c "bake run test 2>&1 | $(QUIET_BAKE)"

test-c-release:
	@bake rebuild test -r --cfg release >/dev/null
	@bash -o pipefail -c "bake run test --cfg release 2>&1 | $(QUIET_BAKE)"

test-leaks:
	@bake rebuild . -r --cfg sanitize >/dev/null
	@bake rebuild test -r --cfg sanitize >/dev/null
	@bake rebuild example/c -r --cfg sanitize >/dev/null
	@bash -o pipefail -c "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 LSAN_OPTIONS=exitcode=23 bake run example/c --cfg sanitize 2>&1 | $(QUIET_BAKE)"

test-cpp:
	@bake rebuild addons/siecs_cpp/test -r >/dev/null
	@bash -o pipefail -c "bake run addons/siecs_cpp/test 2>&1 | $(QUIET_BAKE)"

test-cpp-release:
	@bake rebuild addons/siecs_cpp/test -r --cfg release >/dev/null
	@bash -o pipefail -c "bake run addons/siecs_cpp/test --cfg release 2>&1 | $(QUIET_BAKE)"

update-rust-vendor:
	@addons/siecs_rust/tools/update_vendor.sh

test-rust:
	@cd addons/siecs_rust && cargo test >/dev/null

distr:
	@sh tools/rebuild_distr.sh

check-distr: distr
	git diff --exit-code -- distr/siecs.c distr/siecs.h include/siecs/bake_config.h

check-distr-standalone:
	tmp_dir=$$(mktemp -d /tmp/siecs-distr.XXXXXX); \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	cp distr/siecs.c "$$tmp_dir/siecs.c"; \
	cp distr/siecs.h "$$tmp_dir/siecs.h"; \
	cd "$$tmp_dir"; \
	$(CC) -std=c23 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-function -pedantic -c siecs.c -o siecs-distr.o
