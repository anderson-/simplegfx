{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    bash
    bash-completion
    gnumake
    SDL
    SDL2
    git
    android-tools
  ];

  shellHook = ''
    # Enable bash completion
    source ${pkgs.bash-completion}/etc/profile.d/bash_completion.sh
  '';
}