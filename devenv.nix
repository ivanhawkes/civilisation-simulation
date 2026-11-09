{ pkgs, ... }:

{
  # Use a complete, consistent LLVM 21 toolchain.
  stdenv = pkgs.llvmPackages_21.stdenv;

  packages = with pkgs; [
    # Git and repository management
    git
    git-lfs
    github-cli

    # Build tools
    llvmPackages_21.clang
    llvmPackages_21.clang-tools
    cmake
    ninja
    pkg-config

    # Debugging
    llvmPackages_21.lldb
    gdb
    
    # Pi Coding Agent - simple, light, customise for the project.
    pi-coding-agent

    # Bring worktrunk into the project.
    worktrunk

    # Clipboard
    wl-clipboard

    # Languages and utilities
    go
    curl
    ripgrep
    jq
    fd

    # Python tooling (version-agnostic CLIs; the interpreter comes from languages.python)
    uv
    ruff
    black
    mypy
    python314Packages.pytest
    python314Packages.ipython

    # YAML
    yq-go
  ];

  delta.enable = true;

  languages.cplusplus.enable = true;

  languages.python = {
    enable = true;
    package = pkgs.python314;
    venv.enable = true;
  };

  enterShell = ''
    if [ -d .git ]; then
      echo "Checking Git LFS initialization..."
      git lfs install --local --force
    else
      echo "Not a Git repository. Skipping Git LFS setup."
    fi

    if [ ! -f ".config/wt.toml" ]; then
      echo "💡 Run 'wt config create --project' to link this repo."
    fi

    wt step copy-ignored
  '';

  tasks."project:init" = {
    before = [ "devenv:enterShell" ];
  };
}