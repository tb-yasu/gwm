# Publishing gwm to PyPI

## One-time setup: PyPI Trusted Publishing

`.github/workflows/wheels.yml`'s `publish` job uploads via
[Trusted Publishing](https://docs.pypi.org/trusted-publishers/) (OIDC) —
no PyPI API token is stored anywhere.

1. Log in to PyPI, go to https://pypi.org/manage/account/publishing/.
2. Register a new **pending publisher** (the project doesn't need to exist on
   PyPI yet) with:
   - PyPI project name: `gwm`
   - Owner: `tb-yasu`
   - Repository name: `gwm`
   - Workflow filename: `wheels.yml`
   - Environment name: `pypi`
3. The first successful publish from that workflow claims the project name
   and converts the pending publisher into a regular one.

## Release flow

1. Bump the version everywhere (see checklist below) and commit.
2. Tag and push: `git tag v3.1.3 && git push origin v3.1.3` — the tag must
   start with `v` to match the workflow's `tags: ["v*"]` trigger and its
   `if: startsWith(github.ref, 'refs/tags/v')` publish condition.
3. GitHub Actions builds wheels on all 4 runners plus the sdist, runs
   `pytest python/tests` against each wheel, and only then (because the ref
   is a tag) publishes everything in one `pypa/gh-action-pypi-publish` step.
4. A plain push to `main` (no tag) runs the same build-and-test matrix
   without publishing — that's the CI signal for regular commits.

## Version bump checklist

The version string is not derived from one source; update all of these
before tagging:

- `pyproject.toml` — `[project] version = "..."`
- `CMakeLists.txt` — `project(gwm VERSION ... LANGUAGES CXX)`
- `src/Build.cpp` — the `"gWM version ..."` string in `version()`
- `src/Search.cpp` — the `"gWM version ..."` string in `version()`

## Local fallback (no CI)

Only produces a wheel for the current machine's platform and interpreter —
not a substitute for the CI matrix, which is the supported release path:

```sh
python -m pip install --upgrade build "twine>=6.1"   # >=6.1 for Metadata 2.4 (license-files)

# macOS only: pin the same deployment target CI uses for this arch, otherwise
# the wheel tag reflects whatever OS version happens to be running the build.
export MACOSX_DEPLOYMENT_TARGET=11.0   # 10.13 for an Intel build

python -m build
twine check dist/*
twine upload dist/*   # prompts for a PyPI API token
```

## Troubleshooting

- **LTO/IPO vs. the pybind11 extension**: `GWM_BUILD_PYTHON=ON` links
  `gwm_core` (built with interprocedural optimization on, see
  `CMakeLists.txt`) into the `_core` extension module in the same CMake
  tree, which is expected to be safe. If a release build ever hits an
  LTO-related linker error that a local `cmake -S . -B build` doesn't
  reproduce, add `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF` via
  `CIBW_CONFIG_SETTINGS="cmake.define.CMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF"`
  (or the equivalent `CMAKE_ARGS` for a local build) as an escape hatch.
- **`twine check` failing on license metadata**: needs `twine>=6.1`, which
  understands Metadata 2.4's `License-Expression`/`License-File` fields (see
  `license`/`license-files` in `pyproject.toml`). Older twine reports these
  as invalid.
