# Contributing to LeyoChat

Thank you for taking the time to improve LeyoChat. The project is currently a
Windows-first public preview, so reproducible reports and small, reviewable
changes are especially valuable.

## Before opening an issue

- Search existing issues first.
- Do not post credentials, private messages, databases, logs, crash dumps, or
  internal network details. Redact diagnostic exports and use synthetic data.
- Include the source commit, Windows version, Qt/compiler versions, routing
  mode, and a minimal reproduction when reporting a bug.
- Security vulnerabilities belong in GitHub Private Vulnerability Reporting,
  not in a public issue. See [`SECURITY.md`](SECURITY.md).

## Local development

Install Visual Studio 2022 with the x64 C++ workload, CMake 3.24+, and Qt 6.6.3
for MSVC x64. Qt is intentionally not vendored. Configure a fresh build tree
and set `CMAKE_PREFIX_PATH` to your Qt installation:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.6.3\msvc2019_64" `
  -DLEYOCHAT_BUILD_TESTS=ON
cmake --build build --config Release --clean-first --target LeyoChat LeyoChatLauncher
```

When the Qt HttpServer module is installed, also build
`LeyoChatService` and `LeyoChatServiceHost`.

## Tests and changes

- Keep production changes scoped and preserve existing P2P/service routing
  semantics unless the issue explicitly changes that contract.
- Add or update a focused Qt test for behavior changes.
- Run the relevant focused tests and `git diff --check` before opening a pull
  request. Run the full configured suite when practical, and report any tests
  that could not be built or run.
- Use `--clean-first` when validating a release or installer build so stale
  artifacts cannot hide missing dependencies.
- Do not commit build directories, installers, Qt DLLs, logs, databases, crash
  dumps, certificates, or private keys. The repository `.gitignore` lists the
  expected exclusions.

## Pull requests

Explain the user-visible behavior, the affected routing mode(s), and the
verification commands and results. Keep unrelated formatting or generated-file
changes out of the pull request. Changes under `third_party/` must preserve the
upstream license and update the relevant provenance notice.
