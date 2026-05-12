{
  description = "Farfocel Game Engine flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs =
    { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      llvm = pkgs.llvmPackages_latest;
    in
    {
      devShells.${system}.default =
        pkgs.mkShell.override
          {
            stdenv = llvm.libcxxStdenv;
          }
          {
            packages = [
              llvm.clang-tools
              llvm.lld
              llvm.lldb

              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.neocmakelsp
              pkgs.cmake-format

              pkgs.doctest
              pkgs.nanobench
              pkgs.doxygen
            ];

            shellHook = ''
              export LD_LIBRARY_PATH="${llvm.libcxx}/lib:$LD_LIBRARY_PATH"

              alias fr-build='cmake -B build; cmake --build build -j'
              alias fr-debug='cmake --preset debug; cmake --build --preset debug -j'
              alias fr-docs='cmake -B build; cmake --build build --target docs'

              echo "======== Farfocel DevShell ========"
              echo "Compiler : $(clang++ --version | head -1)"
              echo "CMake    : $(cmake --version | head -1)"
              echo "Ninja    : $(ninja --version)"
              echo ""
              echo "Presets  : cmake --preset <debug|release|msan|tsan>"
              echo "Build    : cmake --build --preset <debug|release|msan|tsan>"
              echo "Aliases  : fr-build, fr-debug, fr-docs"
            '';
          };
    };
}

