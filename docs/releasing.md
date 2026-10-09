# Builds and prereleases

CircleCI has two deliberately separate workflows:

- `build_matrix` runs for ordinary pipelines. Its Ubuntu 24.04 and Windows Server 2022 jobs build
  and run the complete headless test suite with warnings treated as errors, with the frontends
  switched off -- which is what proves the core still builds without Qt. `build_qml_ubuntu` then
  builds the same tree with `MGV_BUILD_QML_APP=ON`, which compiles `qml/Main.qml` through
  `qmlcachegen` and brings `tests/qt/main_menu_controller_test.cpp` into the suite.
- `manual_prerelease` only runs when the boolean pipeline parameter `run_release` is `true`. It
  independently builds and tests the Qt Quick application on both platforms, creates a Debian
  package and an MSI, then publishes both files as a GitHub prerelease.

`build_qml_ubuntu` exists because the QML was previously built by nothing that runs on a pull
request. `manual_prerelease` compiled it, but that workflow refuses to run on any branch but
`main`, so a broken `Main.qml` could only be discovered after it had landed. Qt Quick is a separate
apt install from `qt6-base-dev`, which is why it is a job of its own rather than a flag on the
existing one.

Any job that builds a frontend also needs Jinja2, because `glad` generates its OpenGL loader at
build time by running `python3 -m glad`. It must be installed with pip and not with apt: CMake
resolves `python3` to the CircleCI image's pyenv shim, which cannot see a module in the system
`dist-packages`. `build_ubuntu` escapes this only because with every frontend off, glad is never
fetched at all.

It gates compilation, not correctness. `qmlcachegen` will reject a syntax error or a malformed
object tree; it cannot tell whether a binding onto `menuController` resolves, because that is a
context property injected at runtime. `qt_add_qml_module` also generates a `mgv_qml_qmllint`
target, which would catch more, and is not run here: it reports unqualified access to context
properties, which is exactly how this menu is wired, so it would fail on correct code.

## CircleCI project setup

Connect the GitHub repository to CircleCI and create a restricted CircleCI context named
`github-release`. Add `GITHUB_TOKEN` to that context. The token needs permission to create releases
and tags in this repository; for a fine-grained GitHub token, grant repository `Contents: write`.
Restrict access to the context to the maintainers who may publish prereleases.

Windows jobs use a pinned CircleCI Windows Server 2022 image and MSVC 2022. The release job
downloads the pinned Qt 6.8.3 SDK with `aqtinstall` and uses a pinned WiX through CPack. Ubuntu uses
the distribution's Qt packages and records its QML imports as Debian dependencies. On Windows,
Qt's CMake deployment script gathers the QML imports, plugins, and runtime dependencies before
CPack creates the MSI.

## Trigger a prerelease

In CircleCI, choose **Trigger Pipeline** for the `main` branch and supply these pipeline parameters:

| Parameter | Type | Example |
|---|---|---|
| `run_release` | boolean | `true` |
| `release_tag` | string | `v0.2.0-rc.1` |

`release_tag` must start with `v`, match the version declared by the top-level CMake project, and
include a prerelease suffix. The validation job rejects stable-looking tags, mismatched versions,
and any branch other than `main`. The final job creates the Git tag and GitHub prerelease only after
both packages pass their builds and tests. It intentionally fails if that release tag already
exists, preventing a retry from silently replacing published binaries.

The same parameters can be passed through CircleCI's API:

```json
{
  "branch": "main",
  "parameters": {
    "run_release": true,
    "release_tag": "v0.2.0-rc.1"
  }
}
```

The resulting packages are also retained as CircleCI job artifacts. They are currently unsigned,
so Windows SmartScreen and Debian package tools may warn users. Code signing and repository-level
Debian metadata should be added before producing stable releases.

## Reproduce packaging locally

With the QML prerequisites installed, use the ordinary CMake install/CPack path:

```sh
cmake -S . -B build/package -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMGV_BUILD_GLFW_APP=OFF \
  -DMGV_BUILD_QML_APP=ON
cmake --build build/package --parallel
ctest --test-dir build/package --output-on-failure
cpack --config build/package/CPackConfig.cmake -G DEB -B build/packages
```

The install tree places executables in `bin` and bundled models/shaders in
`share/modern-gl-viewer/assets`. Development builds still prefer the source-tree assets; installed
builds discover the relative package layout instead.
