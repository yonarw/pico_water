{
  description = "pico_water build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # Fetch pico-sdk with all submodules (cyw43-driver, lwip, tinyusb, …)
    pico-sdk = {
      url = "git+https://github.com/raspberrypi/pico-sdk?submodules=1";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, pico-sdk }: let
    system = "x86_64-linux";
    pkgs   = nixpkgs.legacyPackages.${system};
  in {
    devShells.${system}.default = pkgs.mkShell {
      packages = [
        pkgs.gcc-arm-embedded
        pkgs.cmake
        pkgs.ninja
        pkgs.python3   # pico-sdk scripts require python
      ];

      PICO_SDK_PATH = pico-sdk;

      shellHook = ''
        echo "pico_water dev shell ready"
        echo "PICO_SDK_PATH = $PICO_SDK_PATH"
        echo ""
        echo "Build with:"
        echo "  mkdir -p build && cd build && cmake -G Ninja .. && ninja"
      '';
    };
  };
}
