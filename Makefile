ifeq ($(OS), Windows_NT)
	MKDIR_BUILD = if not exist build mkdir build
	TESTS_BIN = build/tests/engine_tests.exe
	BIN = build/engine.exe
	JOBS_NUM = %NUMBER_OF_PROCESSORS%
	CLEAN = rmdir /s /q
else
	MKDIR_BUILD = mkdir -p build
	TESTS_BIN = ./build/tests/engine_tests
	BIN = ./build/engine
	JOBS_NUM = $(nproc)
	CLEAN = rm -rf
endif

configure:
	$(MKDIR_BUILD)
	cmake -S . -B build

build: build/CMakeCache.txt
	cmake --build build -- -j $($(JOBS_NUM))


build/CMakeCache.txt:
	@echo "CMake not configured. Running configure first..."
	$(MAKE) configure

test: build
	$(TESTS_BIN)

run: build
	$(BIN)

clean:
	$(CLEAN) build


.PHONY: build configure test run clean