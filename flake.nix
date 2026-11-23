{
  description = "Basic raylib dev template";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "ray-basic";
            version = "0.1.0";

            src = pkgs.lib.cleanSourceWith {
              src = ./.;
              filter = path: type:
                let
                  base = baseNameOf path;
                in
                base != "game" && base != ".git" && !(pkgs.lib.hasSuffix ".o" base);
            };

            nativeBuildInputs = [
              pkgs.pkg-config
            ];
            buildInputs = [
              pkgs.raylib
            ];

            buildPhase = ''
              runHook preBuild
              make
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out/bin
              cp game $out/bin/
              runHook postInstall
            '';
          };
        }
      );

      apps = forAllSystems (
        system:
        {
          default = {
            type = "app";
            program = "${self.packages.${system}.default}/bin/game";
          };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              raylib
              gcc
              pkg-config
              gdb
              just
            ];

            shellHook = ''
              echo "raylib dev shell — run 'make' to build, 'make run' to run"
            '';
          };
        }
      );
    };
}
