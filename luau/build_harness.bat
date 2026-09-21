@echo off
rem Windows build of the standalone harness (no Godot). Run from anywhere.
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d %~dp0
cl /nologo /MP /EHsc /O2 /MT /std:c++17 /DSECP256K1_STATIC /I src /I luau\VM\include /I luau\Compiler\include /I secp256k1\include ^
  test\harness.cpp test\rbx_harness.cpp src\luau_sandbox.cpp src\script_worker.cpp src\eip712.cpp src\chain_assets.cpp ^
  src\rbx_instance.cpp src\rbx_types.cpp src\rbx_runtime.cpp src\rbx_api.cpp src\rbx_host.cpp src\rbx_net.cpp src\rbx_wire.cpp src\rbx_chat.cpp src\rbx_spatial.cpp src\rbx_pathfinding.cpp src\rbx_editable_mesh.cpp ^
  /Fe:harness.exe /link luau\build\Release\Luau.VM.lib luau\build\Release\Luau.Compiler.lib luau\build\Release\Luau.Ast.lib ^
  luau\build\Release\Luau.Bytecode.lib luau\build\Release\Luau.Common.lib ^
  secp256k1\build\lib\Release\libsecp256k1.lib secp256k1\build\src\secp256k1_precomputed.dir\Release\secp256k1_precomputed.lib
