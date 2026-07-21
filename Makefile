.PHONY: qvm dll clean help

help:
	@echo "Available targets:"
	@echo "  make qvm   - Build qagame as Q3VM bytecode"
	@echo "  make dll   - Build qagame as native DLL"
	@echo "  make clean - Remove build directories"

qvm:
	@mkdir -p build-qvm
	@echo "Configuring QVM build..."
	@cd build-qvm && cmake -DBUILD_QVM=ON ..
	@echo "Building qagame.qvm..."
	@cmake --build build-qvm
	@echo "✓ Built: build-qvm/vm/qagame.qvm"

dll:
	@mkdir -p build-dll
	@echo "Configuring DLL build..."
	@cd build-dll && cmake -DBUILD_QVM=OFF ..
	@echo "Building qagame DLL..."
	@cmake --build build-dll
	@echo "✓ Built: build-dll/qagame*.dll"

clean:
	@echo "Cleaning build directories..."
	@rm -rf build-qvm build-dll
	@echo "✓ Clean complete"
