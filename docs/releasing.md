# Builds and prereleases

CircleCI has two deliberately separate workflows:

- `build_matrix` runs for ordinary pipelines. Its Ubuntu 24.04 and Windows Server 2022 jobs build
  and run the complete headless test suite with warnings treated as errors.
- `manual_prerelease` only runs when the boolean pipeline parameter `run_release` is `true`. It
  independently builds and tests the Qt Quick application on both platforms, creates a Debian
  package and an MSI, then publishes both files as a GitHub prerelease.

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
