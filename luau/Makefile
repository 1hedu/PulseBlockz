# Standalone (no Godot) build of the sandbox + harness. `make test`
LUAU ?= ./luau
SECP ?= ./secp256k1
CXXFLAGS += -std=c++17 -O2 -pthread -I$(LUAU)/VM/include -I$(LUAU)/Compiler/include -I$(SECP)/include -Isrc
LDLIBS += -L$(LUAU)/build -lLuau.Compiler -lLuau.Bytecode -lLuau.Ast -lLuau.VM -lLuau.Common -L$(SECP)/build/lib -lsecp256k1

LUAU_REF ?= bb15c71   # 0.736 + 2; the engine uses lua_break, lua_singlestep and the userthread callback, which move between releases
luau-libs: secp-libs
	@test -d $(LUAU) || (git clone --no-checkout https://github.com/luau-lang/luau.git $(LUAU) && git -C $(LUAU) checkout $(LUAU_REF))
	cmake -S $(LUAU) -B $(LUAU)/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLUAU_BUILD_CLI=OFF -DLUAU_BUILD_TESTS=OFF
	cmake --build $(LUAU)/build --target Luau.VM Luau.Compiler --config Release -j

# libsecp256k1 (bitcoin-core) with the recovery module: curation-list signature checks
secp-libs:
	@test -d $(SECP) || git clone --depth 1 -b v0.6.0 https://github.com/bitcoin-core/secp256k1.git $(SECP)
	cmake -S $(SECP) -B $(SECP)/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	  -DSECP256K1_ENABLE_MODULE_RECOVERY=ON -DSECP256K1_ENABLE_MODULE_ECDH=OFF -DSECP256K1_ENABLE_MODULE_SCHNORRSIG=OFF \
	  -DSECP256K1_ENABLE_MODULE_EXTRAKEYS=OFF -DSECP256K1_ENABLE_MODULE_ELLSWIFT=OFF -DSECP256K1_ENABLE_MODULE_MUSIG=OFF \
	  -DSECP256K1_BUILD_TESTS=OFF -DSECP256K1_BUILD_EXHAUSTIVE_TESTS=OFF -DSECP256K1_BUILD_BENCHMARK=OFF \
	  -DSECP256K1_BUILD_CTIME_TESTS=OFF -DSECP256K1_DISABLE_SHARED=ON
	cmake --build $(SECP)/build --config Release -j

RBX_SRC = src/rbx_instance.cpp src/rbx_types.cpp src/rbx_runtime.cpp src/rbx_api.cpp src/rbx_host.cpp src/rbx_net.cpp src/rbx_wire.cpp src/rbx_chat.cpp src/rbx_spatial.cpp src/rbx_pathfinding.cpp src/rbx_editable_mesh.cpp
harness: src/luau_sandbox.cpp src/script_worker.cpp src/eip712.cpp src/chain_assets.cpp $(RBX_SRC) test/harness.cpp test/rbx_harness.cpp src/luau_sandbox.h src/script_worker.h src/eip712.h src/chain_assets.h src/rbx_instance.h src/rbx_runtime.h src/rbx_internal.h test/vectors_assetstore.h test/vectors_txasset.h
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) $(LDLIBS) -o $@

test: harness
	./harness
