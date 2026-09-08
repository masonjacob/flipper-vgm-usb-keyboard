## Nix shell to use packages with Cmake verion 3.27.7

{ pkgs ? import (fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/459104f841356362bfb9ce1c788c1d42846b2454.tar.gz";
  }) {} }:

pkgs.mkShell {
  packages = with pkgs; [
    cmake
    ninja
    gcc-arm-embedded
    protobuf

    python3
    python3Packages.setuptools
    python3Packages.protobuf
  ];
}


## Version that only downgrades Cmake


##{ pkgs ? import <nixpkgs> {} }:

##let
##  cmakePkgs = import (fetchTarball {
##    url = "https://github.com/NixOS/nixpkgs/archive/459104f841356362bfb9ce1c788c1d42846b2454.tar.gz";
##  }) {};
##in
##pkgs.mkShell {
##  packages = with pkgs; [
##    cmakePkgs.cmake
##    ninja
##    gcc-arm-embedded
##    protobuf

##    python3
##    python3Packages.setuptools
##    python3Packages.protobuf
##  ];
##}
