# CivSimLibrary

A simple simulation of civilisations and their trade needs. This is designed to be used within the [Space Truckers](https://github.com/ivanhawkes/space-truckers) framework.

Each civilisation is a planet or satellite within a solar system that is generated at run-time.

The software is being delivered as a cross-platform C++ library and Unreal Engine 5.8 runtime plugin.

The core parser is independent of Unreal Engine and uses [yaml-cpp](https://github.com/jbeder/yaml-cpp).

## Goal

The simulation aims to provide a satisfying space trading element to games based off the [Space Truckers](https://github.com/ivanhawkes/space-truckers) framework.

## Architecture

- `CivSimCore`: plain C++ core, built independently with CMake for development and tests.
- `YamlCpp`: Unreal Build Tool module that compiles a pinned yaml-cpp source checkout.
- `CivSimLibrary`: Unreal runtime module and Blueprint-facing adapter.

The Unreal plugin compiles its C++ sources and yaml-cpp with UBT for the target platform. Do not copy a Linux-built `.a` into a Windows build or vice versa.

## Requirements

I am doing the majority of the work on a Nixos Linux machine. I intend to keep the code as decoupled from Unreal Engine as possible so I have the freedom to use Nixos and the development tools of my choice. You are free to use whichever OS makes your heart flutter. Fellow Nixos users get a streamlined onboarding process.

### NixOS standalone development

- Nix with flakes enabled
- devenv
- direnv and nix-direnv
- CMake and Ninja (provided by devenv)

### Unreal plugin builds

Use an installed Unreal Engine 5.8 toolchain for each target platform:
- Linux: the compiler/sysroot toolchain configured by Epic's UE 5.8 Linux toolchain setup.
- Windows: the MSVC toolchain and Windows SDK supported by that UE 5.8 installation.

The Nix development shell is for standalone development. It does not replace Epic's platform toolchains. NixOS is not an officially listed UE Linux host distribution, so Unreal build tools may require an FHS-compatible environment or another supported Linux build host.

## 1. Activate the Nix development environment

From this directory:

```bash
direnv allow
```

The `.envrc` uses `devenv`. Ensure direnv is hooked into your interactive shell and devenv is installed.

Example NixOS configuration for direnv:

```nix
{
  programs.direnv = {
    enable = true;
    nix-direnv.enable = true;
  };
}
```

Rebuild your NixOS configuration after changing it, then add the appropriate shell hook:

```bash
# Bash
eval "$(direnv hook bash)"

# Zsh
eval "$(direnv hook zsh)"

# Fish
direnv hook fish | source
```

## 2. Build and test the standalone core

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build
ctest --test-dir build --output-on-failure
```

CMake fetches yaml-cpp 0.9.0 at configure time. This is a separate stand-alone build; the Unreal plugin uses its own source checkout as described below.

## 3. Prepare the Unreal plugin dependency

The zip does not embed a copy of yaml-cpp. Fetch the exact pinned release into the plugin's YamlCpp module:

```bash
./Scripts/FetchYamlCpp.sh
```

This creates:

```text
Source/YamlCpp/Private/ThirdParty/yaml-cpp/
```

The script requires Git and network access. Commit the fetched dependency (or manage it as a Git submodule) if you want fully reproducible/offline plugin builds.

## 4. Install the plugin in an Unreal project

Copy the complete `CivSimLibrary` directory into:

```text
YourProject/Plugins/CivSimLibrary/
```

The plugin descriptor is `CivSimLibrary.uplugin`. Its filename is intentionally retained as the plugin descriptor; the plugin's display name and C++ module are CivSimLibrary.

Generate project files if required, then build the Unreal project with the UE 5.8 version installed for that platform. UBT compiles yaml-cpp and the plugin sources for the selected target.

## 5. YAML example

`Tests/Data/server.yaml`:

```yaml
hostname: localhost
port: 7777
enabled: true
```

The core API is:

```cpp
#include "CivSimLibrary/Core/SimulationConfig.h"

CivSimLibrary::SimulationConfig Config;
std::string Error;

if (!CivSimLibrary::LoadSimulationConfig("server.yaml", Config, Error))
{
    // Handle Error.
}
```

The Unreal-facing Blueprint function is `Load Server Config From YAML`, exposed by `UCivSimLibraryBPLibrary`.

## Compatibility notes

- The public core API does not expose yaml-cpp types.
- The Unreal adapter converts between the core C++ structure and Unreal `FString`/`USTRUCT` types.
- yaml-cpp needs C++ exceptions. They are enabled only for the `YamlCpp` UBT module.
- The sample uses C++20. Confirm that this matches the language-standard setting of your UE 5.8 installation.
- This is a starter scaffold. Validate a full Editor build and packaged Development/Shipping builds on both Windows and Linux before distribution.

## Processing the YAML

The YAML is edited as individual files with relative links and stored in **/data**. This is to keep them manageable and the ideas separate from each other during development. I am not aware of a way to make **yq** honour those links as separate files, so they are all combined into monolithic files before being used in the simulation.

From this directory:

```bash
# Build all of the game-ready files using the source YAML.
build.sh
```

This writes the game-ready documents to **/data/generated**:

| File | Contents |
| --- | --- |
| `full.yaml` | Every item in the game, with the ages and the categories. |
| `simple.yaml` | The small subset of items used for quick matches and testing. |
| `ages.yaml` | The ages on their own. |
| `categories.yaml` | The categories on their own. |

The `*-base` keys - `ages-base`, `categories-base` and `item-base` - exist only to
define the anchors used by the relative links, and `item-base` is repeated in every
item file. `build.sh` explodes the links and drops them, so every key in a generated
file appears exactly once, and each item carries the fields inherited from its
category and age.

The full item list in `tech-tree/60-items-full.yaml` is generated from the anchors
defined in the `30-item-*` files. After adding, removing or renaming an item,
regenerate it from the **/data** directory:

```bash
../Scripts/GenerateItemList.sh
build.sh
```

## AI Disclosure

This project is being made with the assistance of several AI models:

- Qwen 3.8 125B - Swift IQ3 - KV 8,4 (local AI)
- Chat GPT (scaffold)
- Claude (not used yet)

AI will be utilised in research, ideation, writing specifications, requirements gathering and code production.

I have 45 years of development experience from micro-controllers through to security and devops for highly available high traffic websites. 

At this point in time it would be utterly foolish to create software without support from AI models. In the hands of a skilled and experienced developer it is an incredible force multiplier.
