{
  description = "A basic flake with a shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    systems.url = "github:nix-systems/default";
  };

  outputs =
    inputs@{ nixpkgs, systems, ... }:
    let
      forAllSystems =
        fn: nixpkgs.lib.genAttrs (import systems) (
          system: fn {
            pkgs = nixpkgs.legacyPackages.${system};
            inherit system;
          }
        );
    in {
      packages = forAllSystems ({ pkgs, system }: {
        default = pkgs.stdenv.mkDerivation {
          name = "iotac";
          src = ./.;
          buildInputs = with pkgs; [ redo-apenwarr gcc ];
          buildPhase = ''
            cp configs/linux.env config.env
            redo cmd/iotac
          '';
          installPhase = ''
            mkdir -p $out/bin
            cp cmd/iotac $out/bin
          '';
        };
      });
      devShells = forAllSystems ({ pkgs, ... }: {
        default = pkgs.mkShell {
          packages = with pkgs; [ redo-apenwarr clang peg python3 ];
          hardeningDisable = [ "fortify" ];
        };
      });
    };
}
