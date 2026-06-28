BAKE_HOME := $(shell bake env | sed -n 's/^BAKE_HOME=//p')
DEPS_INCLUDE := -I$(BAKE_HOME)/include

.PHONY: clean test test-c test-c-release test-cpp test-cpp-release test-rust update-rust-vendor distr check-distr

clean:
	rm -rf build-consumer-c build-consumer-cpp
	cd addons/siecs_rust && cargo clean

test: clean test-c test-c-release test-cpp test-cpp-release update-rust-vendor test-rust

test-c:
	bake rebuild test -r
	bake run test

test-c-release:
	bake rebuild test -r --cfg release
	bake run test --cfg release

test-cpp:
	bake rebuild addons/siecs_cpp/test -r
	bake run addons/siecs_cpp/test

test-cpp-release:
	bake rebuild addons/siecs_cpp/test -r --cfg release
	bake run addons/siecs_cpp/test --cfg release

update-rust-vendor:
	addons/siecs_rust/tools/update_vendor.sh

test-rust:
	cd addons/siecs_rust && cargo test

distr:
	cp include/siecs/bake_config.h /tmp/siecs-bake_config.h
	bake rebuild . -r
	cp /tmp/siecs-bake_config.h include/siecs/bake_config.h

check-distr: distr
	git diff --exit-code -- distr/siecs.c distr/siecs.h include/siecs/bake_config.h
	$(CC) -std=c23 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-function -pedantic $(DEPS_INCLUDE) -c distr/siecs.c -o /tmp/siecs-distr.o
