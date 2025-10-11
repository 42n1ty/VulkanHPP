md build
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE="C:\Progs\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DCMAKE_BUILD_TYPE=Debug
@REM cmake -B build ^
  @REM -G "Ninja" ^
  @REM -DCMAKE_C_COMPILER=clang-cl.exe ^
  @REM -DCMAKE_CXX_COMPILER=clang-cl.exe ^
  @REM -DCMAKE_TOOLCHAIN_FILE="C:\Progs\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  @REM -DVCPKG_TARGET_TRIPLET=x64-windows-static-md ^
  @REM -DCMAKE_BUILD_TYPE=Debug