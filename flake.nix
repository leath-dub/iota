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
      devShells = forAllSystems ({ pkgs, ... }: {
        default = pkgs.mkShell {
          packages = with pkgs; [ redo-apenwarr clang ];
          hardeningDisable = [ "fortify" ];
        };
      });
    };
}
