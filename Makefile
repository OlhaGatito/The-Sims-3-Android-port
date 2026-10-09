# Canonical development entry points: build with the documented NextOS SDK.
# The loader has its own Makefile under loader/; this root Makefile must not build
# ARM code with the host compiler or overwrite the tracked loader binaries.
SHELL := /bin/bash

.PHONY: all bootstrap loader verify test package reconstruct clean

all: loader

bootstrap:
	bash scripts/nextos-bootstrap.sh

loader:
	bash scripts/build-loader-nextos.sh

verify:
	bash scripts/verify-loader-nextos.sh

test:
	bash scripts/test-shell-scripts.sh

package:
	bash scripts/package-nextos-port.sh

# Full reconstruction pipeline (extract -> build -> verify -> package -> report).
# Idempotent and non-destructive: never runs `make clean`, never installs
# dependencies, never overwrites tracked files.
reconstruct:
	bash scripts/reconstruct-port.sh $(APK)

clean:
	rm -rf build/nextos
	@echo "Removed generated NextOS build artifacts; tracked loaders and stubs were preserved."
