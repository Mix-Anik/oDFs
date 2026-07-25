.PHONY: qvm dll clean help

QVM_PK3 := zz-defrag_vm_191_custom.pk3

help:
	@echo "Available targets:"
	@echo "  make qvm   - Build qagame/cgame/ui as Q3VM bytecode and pack into $(QVM_PK3)"
	@echo "  make dll   - Build qagame as native DLL"
	@echo "  make test  - Build and run the testing/ standalone diagnostics"
	@echo "  make clean - Remove build directories"

qvm:
	@mkdir -p build-qvm
	@echo "Configuring QVM build..."
	@cd build-qvm && cmake -DBUILD_QVM=ON ..
	@echo "Building qagame.qvm..."
	@cmake --build build-qvm
	@echo "✓ Built: build-qvm/vm/qagame.qvm"
	@python3 scripts/create_pk3.py build-qvm/vm build-qvm/$(QVM_PK3)
	@echo "✓ Packed: build-qvm/$(QVM_PK3)"

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
