{
  description = "C++23 Minimal Template";

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
              # LLVM Tooling
              llvm.clang-tools
              llvm.lld
              llvm.lldb

              # Build Tooling
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.neocmakelsp
              pkgs.cmake-format

              # Libs
              pkgs.doctest
              pkgs.nanobench
              pkgs.doxygen

              # SDL3 Dependecies
              pkgs.wayland
              pkgs.wayland-scanner
              pkgs.wayland-protocols
              pkgs.libxkbcommon
              pkgs.libx11
              pkgs.libxext
              pkgs.libxcursor
              pkgs.libxi
              pkgs.libxfixes
              pkgs.libxrandr
              pkgs.libxscrnsaver
              pkgs.libxtst
              pkgs.alsa-lib

              pkgs.pulseaudio
              pkgs.pipewire
              pkgs.libGL
              pkgs.vulkan-loader
              pkgs.vulkan-headers
              pkgs.libxcb

            ];

            shellHook = ''
              export LD_LIBRARY_PATH="${llvm.libcxx}/lib:$LD_LIBRARY_PATH"
              export LD_LIBRARY_PATH="${
                pkgs.lib.makeLibraryPath (
                  with pkgs;
                  [
                    wayland
                    libxkbcommon
                    libxcb
                    libx11
                    libxcursor
                    libxext
                    libxi
                    libxfixes
                    libxrandr
                    libxscrnsaver
                    libGL
                    vulkan-loader
                  ]
                )
              }:/run/opengl-driver/lib:/run/graphics/lib:$LD_LIBRARY_PATH"

              echo "======== C++23 DevShell ========"
              echo "Compiler : $(clang++ --version | head -1)"

              echo "CMake    : $(cmake --version | head -1)"
              echo "Ninja    : $(ninja --version)"
              echo ""
              echo "Presets  : cmake --preset <debug|release|msan|tsan>"
              echo "Build    : cmake --build --preset <debug|release|msan|tsan>"
            '';
          };
    };
}
