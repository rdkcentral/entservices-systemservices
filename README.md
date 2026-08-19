# template

> **Template repository with common workflows for future clones.**

This repository is the canonical starting point for RDK Central ENT-Services Thunder plugin projects. Clone or fork it to immediately inherit a production-grade CI/CD pipeline, documentation scaffolding, and build tooling — then focus on writing your plugin.

***

## Overview

This is a **template repository** for RDK Central ENT-Services Thunder plugin projects. It provides a standardized starting point that includes pre-configured GitHub Actions CI/CD workflows, a consistent documentation structure, and the project scaffolding common to all `entservices-*` plugin repositories in the [rdkcentral](https://github.com/rdkcentral) organization.

When you create a new repository from this template (or clone it), you immediately inherit:

- **Automated CI/CD pipelines** — L1 unit tests, L2 integration tests, native full builds, open-source license scanning, CLA enforcement, and automated release tooling.
- **Documentation conventions** — a pre-defined structure for `ARCHITECTURE.md` (technical deep-dive) and `PRODUCT.md` (product-facing feature documentation).
- **Build scaffolding** — CMake modules, dependency scripts, and coverage tooling aligned with the RDK build system.

### Documentation

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Component diagrams, data flows, threading model, dependencies, and integration points |
| [PRODUCT.md](PRODUCT.md) | Feature descriptions, use cases, API capabilities, and deployment details |
| [CHANGELOG.md](CHANGELOG.md) | Version history; **must be updated** with every pull request targeting `main` or `release/**` |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution guidelines, CLA requirements, and code standards |

> **New to this repository?** Start with [PRODUCT.md](PRODUCT.md) for a feature overview, then [ARCHITECTURE.md](ARCHITECTURE.md) for a technical understanding of how the plugin is structured.

***

## Repository Structure

```
.
├── .github/
│   └── workflows/          # GitHub Actions CI/CD workflow definitions
├── Tests/                  # L1 unit tests and L2 integration tests
├── cmake/                  # CMake module files and find-package helpers
├── plugin/                 # Thunder plugin source code (C++)
├── ARCHITECTURE.md         # Technical architecture documentation
├── CHANGELOG.md            # Version history (updated per PR to main/release)
├── CMakeLists.txt          # Top-level CMake build definition
├── CONTRIBUTING.md         # Contribution guidelines
├── COPYING                 # License summary
├── LICENSE                 # Full license text
├── NOTICE                  # Third-party notices
├── PRODUCT.md              # Product feature and API documentation
├── README.md               # This file
├── build_dependencies.sh   # Script to fetch and build external dependencies
├── cov_build.sh            # Coverage build script (used in native CI)
└── services.cmake          # Plugin-specific CMake service definitions
```

[[1]](https://github.com/rdkcentral/entservices-hdmicecsink)

### Key Directories

| Directory / File | Description |
|---|---|
| `.github/workflows/` | All CI/CD automation — see [Included Workflows](#included-workflows) for details [[2]](https://github.com/rdkcentral/entservices-hdmicecsink/tree/HEAD/.github/workflows) |
| `Tests/` | L1 (unit) and L2 (integration) test suites using Google Test and the `entservices-testframework` [[3]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml) |
| `cmake/` | Reusable CMake helpers for locating RDK libraries and configuring the Thunder plugin system |
| `plugin/` | Plugin shell (WPEFramework interface) and implementation (business logic, out-of-process worker) |
| `ARCHITECTURE.md` | Describes component structure, data flows, threading model, and external dependencies [[4]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/ARCHITECTURE.md) |
| `PRODUCT.md` | Describes product features, API surface (JSON-RPC methods and events), and deployment targets [[5]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/PRODUCT.md) |

***

## Setup and Installation

### Prerequisites

| Tool | Minimum Version | Notes |
|---|---|---|
| Git | Any recent | For cloning and submodule management |
| CMake | 3.16+ | Used throughout the build system [[6]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml#L79-L83) |
| Ninja | Any | Preferred generator (`cmake -G Ninja`) |
| GCC or Clang | GCC 11+ / Clang 14+ | Both compilers are supported in CI [[7]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml#L26-L35) |
| Python 3 | 3.x | Required for JSON schema tools (`pip install jsonref`) [[8]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml#L68-L72) |
| Docker | Any | Required for the native full-build workflow [[9]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/native_full_build.yml#L13-L14) |

Additional system packages required on Ubuntu 22.04 [[10]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml#L84-L93):

```bash
sudo apt install -y libsqlite3-dev libcurl4-openssl-dev valgrind lcov clang \
  libsystemd-dev libboost-all-dev libwebsocketpp-dev meson libcunit1 libcunit1-dev \
  curl protobuf-compiler-grpc libgrpc-dev libgrpc++-dev \
  libunwind-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

***

### Option 1 — Use as a GitHub Template

If this repository is [marked as a template](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-repository-from-a-template) on GitHub:

1. Click **"Use this template"** → **"Create a new repository"** on the repository's GitHub page.
2. Name your repository (e.g., `entservices-myplugin`) and choose your organization.
3. Clone the new repository:
   ```bash
   git clone https://github.com/rdkcentral/entservices-myplugin.git
   cd entservices-myplugin
   ```

### Option 2 — Fork

1. Click **"Fork"** on the GitHub repository page.
2. Clone your fork:
   ```bash
   git clone https://github.com/<your-org>/entservices-myplugin.git
   cd entservices-myplugin
   ```

### Option 3 — Direct Clone

```bash
git clone https://github.com/rdkcentral/template.git entservices-myplugin
cd entservices-myplugin
# Remove the original remote and point to your new repo
git remote set-url origin https://github.com/<your-org>/entservices-myplugin.git
```

***

### Building Locally

#### Quick Build (using RDK CI Docker image)

The native CI workflow uses the official RDK Docker image for a fully reproducible build [[11]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/native_full_build.yml):

```bash
docker pull ghcr.io/rdkcentral/docker-rdk-ci:latest
docker run --rm -v $(pwd):/workspace -w /workspace \
  ghcr.io/rdkcentral/docker-rdk-ci:latest \
  bash -c "sh -x build_dependencies.sh && sh -x cov_build.sh"
```

#### Manual CMake Build

```bash
# 1. Fetch external dependencies
bash build_dependencies.sh

# 2. Configure
cmake -G Ninja \
  -S . \
  -B build \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/usr" \
  -DPLUGIN_<YOURPLUGIN>=ON \
  -DUSE_THUNDER_R4=ON

# 3. Build and install
cmake --build build -j$(nproc)
cmake --install build
```

Replace `<YOURPLUGIN>` with the CMake option defined for your specific plugin (e.g., `PLUGIN_HDMICECSINK`).

***

## Included Workflows

All workflows live under `.github/workflows/` [[2]](https://github.com/rdkcentral/entservices-hdmicecsink/tree/HEAD/.github/workflows) and are shared across ENT-Services repositories. This is the primary value this template delivers — a battle-tested CI/CD pipeline you get for free with every clone.

### Automatic Triggers (`tests-trigger.yml`)

The main orchestrator workflow. It fires automatically on every **push** or **pull request** targeting `main`, `develop`, `sprint/**`, or `release/**` branches and dispatches both L1 and L2 test jobs in parallel [[12]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/tests-trigger.yml).

```
push/PR → tests-trigger.yml → L1-tests.yml (parallel)
                             → L2-tests.yml (parallel)
```

### L1 Unit Tests (`L1-tests.yml`)

Runs isolated unit tests without any hardware or runtime dependencies [[3]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml):

- **Platform**: Ubuntu 22.04
- **Compilers**: GCC (with coverage) and Clang
- **Framework**: Google Test (`v1.15.0`) + `entservices-testframework`
- **Coverage**: `lcov` + `genhtml` HTML report (GCC builds only)
- **Memory check**: Valgrind memcheck with leak detection
- **Artifacts**: Coverage HTML report, Valgrind log, JSON test results

Key build dependencies checked out during the job:

- `rdkcentral/Thunder` — WPEFramework core
- `rdkcentral/ThunderTools` — code generators
- `rdkcentral/entservices-apis` — JSON-RPC interface definitions
- `rdkcentral/entservices-testframework` — mock infrastructure and test harness
- `rdkcentral/entservices-helpers` — shared plugin helpers

### L2 Integration Tests (`L2-tests.yml`)

Runs tests against a live Thunder runtime inside the GitHub Actions runner [[13]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L2-tests.yml):

- **Platform**: Ubuntu 22.04
- **Runtime**: Full Thunder (WPEFramework) process started for each test run
- **Additional dependency**: `entservices-powermanager` (for cross-plugin integration scenarios)
- **Contract testing**: [Pact](https://docs.pact.io/) verifier CLI for API contract validation
- **Coverage**: `lcov` per-plugin coverage report
- **Memory check**: Valgrind memcheck
- **Artifacts**: Coverage HTML report, Valgrind log, JSON test results

### Native Full Build (`native_full_build.yml`)

Validates that the plugin compiles correctly in the actual RDK build environment [[11]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/native_full_build.yml):

- **Trigger**: Push/PR to `main`, `develop`, `sprint/**`, `release/**`
- **Container**: `ghcr.io/rdkcentral/docker-rdk-ci:latest`
- **Steps**: `build_dependencies.sh` → `cov_build.sh`

This is the closest approximation to a real target device build and catches dependency or toolchain issues that unit test builds (which use stub headers) would miss.

### Changelog Enforcement (`update-changelog-and-api-version.yml`)

Ensures every pull request to `main` or `release/**` branches includes an update to `CHANGELOG.md` [[14]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/update-changelog-and-api-version.yml). The workflow fails if `CHANGELOG.md` is not among the changed files, enforcing a consistent version history.

> **Note**: Changes to `docs/`, `Tests/`, `Tools/`, and `.github/` directories are excluded from this check.

### CLA Signature Check (`cla.yml`)

Automatically verifies that all PR authors have signed the Contributor License Agreement before merging [[15]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/cla.yml). This integrates with the `rdkcentral/cmf-actions` CLA system and posts a comment on the PR guiding contributors through the signing process.

### Component Release (`component-release-main.yml`)

A manually dispatched (`workflow_dispatch`) release workflow [[16]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/component-release-main.yml). Maintainers trigger it by providing a semantic version tag (e.g., `1.2.3`) through the GitHub Actions UI:

1. Navigate to **Actions** → **Component Release Main** → **Run workflow**
2. Enter the tag version
3. The workflow calls `rdkcentral/build_tools_workflows` to create the release

### Open-Source License Scan (`fossid_integration_stateless_diffscan_target_repo.yml`)

Runs a FossID stateless diff scan on every pull request to detect open-source license obligations introduced by changed files [[17]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/fossid_integration_stateless_diffscan_target_repo.yml). This workflow is skipped for PRs from forks.

### Manual CI (`manual-ci.yml`)

A placeholder `workflow_dispatch` workflow for manually triggering specific test types (`Sanity`, `Quick`, `L1`, `L2`) from the GitHub Actions UI [[18]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/manual-ci.yml). Customize this for your plugin's ad-hoc testing needs.

***

## Using This Template

This section walks through how to adapt the template for a new ENT-Services Thunder plugin project.

### Step 1 — Rename the Plugin

Update the plugin name throughout the codebase:

1. **`CMakeLists.txt`** — Change the project name and `PLUGIN_<NAME>` option.
2. **`services.cmake`** — Update the plugin registration entries.
3. **`plugin/`** — Rename source files and class names to match your plugin (e.g., `HdmiCecSink` → `MyPlugin`).
4. **`.github/workflows/L1-tests.yml` and `L2-tests.yml`** — Update `REPO_NAME` and all references to `entservices-hdmicecsink`.

### Step 2 — Update Documentation

Replace the placeholder content in the documentation files:

- **`README.md`** — Replace this file with your project-specific README.
- **`ARCHITECTURE.md`** — Document your plugin's component structure, data flows, and dependencies. See [ARCHITECTURE.md](ARCHITECTURE.md) for an example of the expected format [[4]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/ARCHITECTURE.md).
- **`PRODUCT.md`** — Document features, use cases, JSON-RPC API methods/events, and deployment targets. See [PRODUCT.md](PRODUCT.md) for an example [[5]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/PRODUCT.md).
- **`CHANGELOG.md`** — Initialize with your first version entry.

### Step 3 — Configure Workflows

The workflows work out of the box but may need minor adjustments:

- **`L1-tests.yml` / `L2-tests.yml`**: Update the `REPO_NAME` environment variable and any plugin-specific CMake flags (e.g., `-DPLUGIN_HDMICECSINK=ON` → `-DPLUGIN_MYPLUGIN=ON`). [[19]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml#L11-L14)
- **`component-release-main.yml`**: Ensure the `RDKCM_DEPLOY_KEY` secret is configured in your repository settings. [[16]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/component-release-main.yml)
- **`cla.yml`**: Ensure the `CLA_ASSISTANT` secret is set to allow the CLA bot to operate. [[15]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/cla.yml)

### Step 4 — Add Plugin Source Code

Place your WPEFramework Thunder plugin source files under `plugin/`. A typical Thunder plugin consists of:

- **Plugin shell** (`MyPlugin.cpp`, `MyPlugin.h`) — Handles WPEFramework lifecycle and JSON-RPC registration
- **Implementation** (`MyPluginImplementation.cpp`, `MyPluginImplementation.h`) — Core business logic, optionally running out-of-process for stability

See [ARCHITECTURE.md](ARCHITECTURE.md) for a detailed breakdown of the two-layer plugin pattern used across all ENT-Services repositories [[20]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/ARCHITECTURE.md#L48-L68).

### Step 5 — Write Tests

Add your tests under `Tests/`:

- **L1 Tests** — Unit tests using Google Test and mock headers. These run without any hardware or Thunder runtime.
- **L2 Tests** — Integration tests that exercise the plugin running inside a live Thunder process.

The `entservices-testframework` repository provides the mock infrastructure, test harness, and CMake integration used by both L1 and L2 test suites.

### Typical Use Cases

| Use Case | Approach |
|---|---|
| New RDK Thunder plugin | Use this template as a base; follow Steps 1–5 above |
| Adding CI to an existing plugin | Copy the `.github/workflows/` directory and adapt the workflow files |
| Onboarding a team to the test framework | Reference the L1/L2 test workflow files for dependency and build patterns |

***

## Contributing

Contributions to this template — including improvements to the common workflows, documentation conventions, or build scaffolding — are welcome and encouraged.

### Contribution Workflow

1. **Fork** this repository (or create a feature branch if you have write access).

2. **Create a branch** following the naming conventions used across RDK Central repositories:

   ```
   feature/<short-description>
   bugfix/<short-description>
   topic/RDK-<ticket-number>
   ```

   Branches targeting sprint or release timelines:

   ```
   sprint/<sprint-name>
   release/<version>
   ```

3. **Make your changes.** Keep commits focused and atomic.

4. **Update `CHANGELOG.md`** — this is **required** for all pull requests targeting `main` or `release/**` branches. The `update-changelog-and-api-version.yml` workflow will fail your PR if `CHANGELOG.md` is not modified [[14]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/update-changelog-and-api-version.yml). Use standard [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format.

5. **Open a Pull Request** against `main` or `develop`.

6. **Sign the CLA** — The `cla.yml` workflow will automatically prompt you if you haven't signed the Contributor License Agreement yet [[15]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/cla.yml). Follow the instructions in the PR comment.

7. **Ensure all checks pass**:

   - ✅ L1 unit tests (`L1-tests.yml`)
   - ✅ L2 integration tests (`L2-tests.yml`)
   - ✅ Native full build (`native_full_build.yml`)
   - ✅ FossID license scan (non-fork PRs only)
   - ✅ CLA signature

### Coding Standards

- **Language**: C++14 or later (aligned with WPEFramework/Thunder R4 requirements)
- **Compiler warnings**: All code must compile cleanly with `-Wall -Wno-unused-result -Wno-deprecated-declarations` [[21]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml#L384-L396)
- **Code coverage**: New features should include corresponding L1 or L2 tests; coverage reports are generated automatically
- **Memory safety**: Code is run under Valgrind memcheck in CI — avoid memory leaks and undefined behavior [[22]](https://github.com/rdkcentral/entservices-hdmicecsink/blob/aa28436cc84a9cdcb253de1057797bb720f44cb6/.github/workflows/L1-tests.yml#L642-L657)

### Suggesting Template Improvements

If you want to propose changes to the **template itself** (e.g., adding a new workflow, changing the documentation structure, or updating build tooling), please:

1. Open a GitHub Issue describing the improvement and its rationale.
2. Reference any relevant repositories where the change would have an impact.
3. If prototyping in a cloned repository, link the PR there so reviewers can see the real-world effect.

Template changes are propagated to downstream repositories manually — each repo that cloned this template maintains its own copy of the workflow files and should cherry-pick improvements as needed.

### Getting Help

- Browse the [RDK Central GitHub organization](https://github.com/rdkcentral) for related repositories and discussions.
- Open a GitHub Issue in this repository for template-specific questions.
- Refer to [CONTRIBUTING.md](CONTRIBUTING.md) for additional project-specific guidelines.
