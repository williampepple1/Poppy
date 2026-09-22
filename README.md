<p align="center">
  <img src="assets/icon.png" width="140" height="140" alt="Poppy Logo" />
</p>

<h1 align="center">Poppy</h1>

<p align="center">
  <strong>A blazingly fast, lightweight, offline-first native API testing client and CLI runner.</strong><br />
  Built with modern <strong>C++20</strong>, <strong>Qt 6</strong>, and <strong>libcurl</strong>.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B" alt="C++20" />
  <img src="https://img.shields.io/badge/Qt-6.10%2B-green.svg?style=flat-square&logo=qt" alt="Qt 6" />
  <img src="https://img.shields.io/badge/Engine-libcurl-orange.svg?style=flat-square" alt="libcurl" />
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg?style=flat-square" alt="Platforms" />
  <img src="https://img.shields.io/badge/License-MIT-blueviolet.svg?style=flat-square" alt="License" />
</p>

---

## Overview

**Poppy** is an open-source, local-first API client and testing platform inspired by Bruno. While traditional tools rely on heavy Electron runtimes (Node.js + Chromium) that consume hundreds of megabytes of RAM and require cloud logins, Poppy delivers:

- ⚡ **Sub-100ms Cold Startup**: Instantaneous launches directly from your terminal or desktop.
- 🪶 **< 30MB Memory Footprint**: Orders of magnitude lighter than Electron-based alternatives.
- 📁 **Git-First & Local Storage**: Collections are saved as human-readable `.bru` plain-text files directly on your local filesystem—perfect for version control, branching, and pull requests.
- 🔒 **Zero Cloud Lock-in**: No mandatory accounts, telemetry tracking, or cloud synchronization.

---

## Key Features

### 🚀 Protocols & Requests
- **Full REST / HTTP**: HTTP/1.1 and HTTP/2 support across all standard verbs (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD`, `OPTIONS`).
- **Native GraphQL Split Editor**: Dedicated query and JSON variables editor with automatic payload serialization (`{"query": "...", "variables": {...}}`) and syntax highlighting.
- **Dynamic URL Parameters**: Path variable substitution (`:param`) and query parameter key-value tables with toggles.
- **Flexible Body Formats**: JSON (with syntax highlighting), Multipart Form-Data, URL-Encoded, Raw text, and GraphQL.
- **Rich Telemetry**: Microsecond latency measurements, status code badges, payload size, and detailed timing breakdowns.

### 🔐 Authentication Suite
- **Bearer Token**: Token strings with dynamic variable expansion.
- **Basic Authentication**: Base64-encoded username and password credentials.
- **API Key**: Header or query parameter placement.
- **AWS Signature Version 4 (SigV4)**: Full canonical request signing with HMAC-SHA256, date stamping, region, and service scope.
- **Interactive OAuth 2.0 Helper**:
  - **Client Credentials Grant**: Instant token exchange via direct endpoint communication.
  - **Authorization Code Flow**: Embedded local loopback server (`127.0.0.1:8089/callback`) that opens your browser, captures the redirect code, exchanges it for an access token, and automatically sets the Bearer token.

### 🧪 Scripting & Assertions Engine
- **Declarative Zero-Code Assertions**: Visual assertion builder supporting `eq`, `neq`, `gt`, `gte`, `lt`, `lte`, `contains`, and `not_contains` against `res.status`, `res.responseTime`, headers, and nested JSON dot-paths (e.g. `res.body.users[0].id`).
- **Sandboxed JavaScript Testing**: Integrated `QJSEngine` runtime exposing a secure `poppy` namespace (`poppy.req`, `poppy.res`, `poppy.setEnvVar`, `poppy.getEnvVar`).
- **Standard Chai/Mocha-Style Test Suite**: Write custom pre-request and post-response validation scripts using familiar `expect(res.getStatus()).to.equal(200)` assertions.

### 🔄 Migration, Interoperability & Exporters
- **cURL Importer**: Paste any cURL command to instantly parse methods, URLs, headers, bodies, and auth.
- **Postman v2.1 Importer**: Recursively converts Postman collections, folders, environments, and requests into `.bru` directory trees.
- **Insomnia v4 Importer**: Ingests Insomnia workspace exports, maintaining nested folder structures and auth profiles.
- **OpenAPI v3.0 Importer & Exporter**: Ingest OpenAPI specifications into collections, or export entire collections and active requests to valid OpenAPI 3.0.3 JSON specs with a single click.

### ⚡ Developer Ergonomics & Productivity
- **Quick Open Command Palette (`Ctrl+P`)**: Instant keyboard fuzzy switcher across all requests in the active collection with colored HTTP method badges and directory breadcrumbs.
- **In-Response Search & JSONPath Filter (`Ctrl+F`)**: Interactive search bar with document match highlighting, previous/next cycling, match counter, case-sensitive/regex toggles, and live JSONPath query filtering (`$.items[*].name`).
- **Multi-Tab Document Workspace**: Tabbed interface with `Ctrl+N` (New Request), `Ctrl+S` (Save), `Ctrl+W` (Close), and unsaved changes dirty indicators (`*`).

### 💻 Multi-Language Code Generation
- Export any request with one click to production-ready code:
  - **Python** (`requests`)
  - **JavaScript** (Fetch API & Axios)
  - **Go** (`net/http`)
  - **C++** (`libcurl`)
  - **cURL** command-line syntax

### 🤖 Automation & Headless CLI Runner (`poppy-cli`)
Poppy includes a standalone CLI binary designed for CI/CD pipelines (GitHub Actions, GitLab CI, Jenkins):
- **Multiple Output Reporters**: Formatted terminal output, structured JSON, and standard **JUnit XML** (`--reporter junit`) for test dashboard ingestion.
- **Multi-Iteration Execution**: Run test suites for multiple loops (`--iterations <n>`).
- **Data-Driven Fixtures**: Bind runtime variables per iteration using JSON or CSV data files (`--data fixtures.json`).
- **Rate-Limiting & Pacing**: Configurable inter-request throttling (`--delay <ms>`).

### 🌐 Networking & Security Controls
- **Persistent Cookie Jar**: Cross-request session cookie storage using libcurl's cookie engine with one-click clearing.
- **Proxy Configuration**: System proxy or custom HTTP, HTTPS, and SOCKS5 proxy routing with authentication.
- **SSL / TLS Verification**: One-click toggle to ignore self-signed certificates in development environments.
- **Request Timeouts**: Configurable connection and read timeouts.

---

## Benchmark: Poppy vs Electron Alternatives

| Metric | Postman / Insomnia | Bruno | Poppy (Native C++/Qt) |
| :--- | :---: | :---: | :---: |
| **Technology** | Electron (Chromium + Node) | Electron (Chromium + Node) | **C++20 + Qt 6 + libcurl** |
| **Cold Startup Time** | 2.5s - 5.0s | 1.5s - 3.0s | **< 100 ms** |
| **Baseline RAM (Idle)** | 250MB - 500MB | 150MB - 300MB | **< 30 MB** |
| **Distribution Size** | 150MB - 250MB | 100MB - 180MB | **Single Native Binary** |
| **Storage Model** | Proprietary Cloud | Local Filesystem (`.bru`) | **Local Filesystem (`.bru`)** |
| **Cloud Dependency** | Mandatory / Cloud-first | Offline-first | **100% Offline-First** |

---

## Directory & Collection Format

Poppy collections mirror ordinary folders on your disk:

```
my-api-collection/
├── poppy.json                 # Collection metadata and configuration
├── environments/
│   ├── dev.env                # Environment variables (committed to git)
│   ├── dev.secret.env         # Ignored by git (.gitignore) for secrets
│   └── prod.env
├── auth/
│   └── login.bru              # Bru request file
└── users/
    ├── get-user.bru
    └── create-user.bru
```

### Example `.bru` Request File:

```bru
meta {
  name: Get User Profile
  type: http
  seq: 1
}

get {
  url: {{baseUrl}}/api/users/:id
  body: none
  auth: bearer
}

params:path {
  id: 123
}

headers {
  Accept: application/json
}

auth:bearer {
  token: {{authToken}}
}

assertions {
  res.status eq 200
  res.responseTime lt 1000
  res.body.id eq 123
}

tests {
  test("User ID matches", function() {
    expect(res.getBody().id).to.equal(123);
  });
}
```

---

## Building from Source

### Prerequisites
- **CMake** (>= 3.20)
- **C++20 Compatible Compiler**:
  - Windows: MinGW GCC 13+ or MSVC 2022+
  - Linux: GCC 11+ or Clang 14+
  - macOS: Apple Clang 14+
- **Qt 6** (Core, Gui, Widgets, Network, Qml)
- **Ninja** (recommended build generator)

### Build Instructions

```bash
# 1. Clone the repository
git clone https://github.com/williampepple1/Poppy.git
cd Poppy

# 2. Configure with CMake
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. Compile binaries (GUI and CLI)
cmake --build build

# 4. Run test suite (13 test suites)
ctest --test-dir build --output-on-failure
```

Compiled executables will be available in:
- `build/bin/poppy` (GUI Desktop Application)
- `build/bin/poppy-cli` (Headless CLI Runner)

### Packaging for Release

#### Local Standalone Package (Windows)
Create a standalone, portable `.zip` release bundled with all necessary Qt and MinGW runtime libraries via `windeployqt`:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
# Generates: dist/Poppy-windows-x64.zip
```

#### Automated CI/CD Releases (GitHub Actions)
The repository includes an automated release workflow ([`.github/workflows/release.yml`](.github/workflows/release.yml)):
- **Continuous Deploy**: Every push to `main` compiles, tests, packages, and updates the rolling [Latest](https://github.com/williampepple1/Poppy/releases/latest) GitHub Release (`Poppy-windows-x64.zip` and `Poppy-linux-x64.tar.gz`).
- **Version Tag**: Push any version tag (e.g. `v1.0.0`) to publish a snapshot release for that version:
  ```bash
  git tag v1.0.0
  git push origin v1.0.0
  ```
- **Manual Trigger**: Navigate to **GitHub Actions -> Release -> Run workflow**, specify the tag name, and trigger on demand.

---

## CLI Usage

Run an entire collection headlessly from your terminal:

```bash
# Basic run with environment
./build/bin/poppy-cli run ./examples/sample-collection --env dev

# Multi-iteration run with delay pacing
./build/bin/poppy-cli run ./examples/sample-collection --env dev --iterations 3 --delay 100

# Data-driven testing with JSON fixture
./build/bin/poppy-cli run ./examples/sample-collection --env dev --data ./fixtures.json

# CI/CD: Export JUnit XML test report
./build/bin/poppy-cli run ./examples/sample-collection --env ci --reporter junit --output test-results.xml
```

---

## Contributing

Contributions are warmly welcome! Whether you are reporting bugs, improving documentation, or submitting pull requests:

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Ensure all tests pass (`ctest --test-dir build --output-on-failure`)
4. Commit your changes (`git commit -m 'feat: add amazing feature'`)
5. Push to the branch (`git push origin feature/amazing-feature`)
6. Open a Pull Request

---

## License

Distributed under the **MIT License**. See `LICENSE` for more information.
