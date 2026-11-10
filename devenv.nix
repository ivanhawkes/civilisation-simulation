{ pkgs, ... }:

{
  # Enable dotenv support for loading environment variables from a .env file.
  dotenv.enable = true;

  # Use a complete, consistent LLVM 21 toolchain.
  stdenv = pkgs.llvmPackages_21.stdenv;

  packages = with pkgs; [
    # Git and repository management
    git
    git-lfs
    github-cli

    # Node and PNPM added directly via standard system package attributes
    nodejs_24
    pnpm

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

    echo "  pnpm: $(pnpm --version)"

    # Automatically register Context7 MCP if the key was loaded from your .env
    if [ -n "''${CONTEXT7_API_KEY:-}" ]; then
      echo "🔄 Found Context7 API key. Registering MCP server in Pi Agent..."
      pi mcp add context7 --url https://context7.com \
        --bearer-token-env-var CONTEXT7_API_KEY --exposure direct 2>/dev/null || true
    else
      echo "⚠️ Warning: CONTEXT7_API_KEY is not set. Check your .env file."
    fi

    if [ ! -f ".config/wt.toml" ]; then
      echo "💡 Run 'wt config create --project' to link this repo."
    fi

    wt step copy-ignored
  '';
}
