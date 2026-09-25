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
- **WebSocket Client (`ws://` & `wss://`)**: Native RFC 6455 client with interactive handshake headers, live message timeline (`▲ SENT` / `▼ RECV`), text/binary/ping/pong frames, automatic pong responses, and formatted JSON inspector.
- **gRPC Client & Protobuf Viewer**: HTTP/2 unary gRPC execution with 5-byte framing, `.proto` schema parser, service/method selectors, automated example JSON payload generator, and `grpc-status` / trailer decoding.
- **Native GraphQL Split Editor**: Dedicated query and JSON variables editor with automatic payload serialization (`{"query": "...", "variables": {...}}`) and syntax highlighting.
- **Dynamic URL Parameters**: Path variable substitution (`:param`) and query parameter key-value tables with toggles.
- **Flexible Body Formats**: JSON (with syntax highlighting), Multipart Form-Data (with file attachments), URL-Encoded, Raw text, and GraphQL.
- **Rich Telemetry**: Microsecond latency measurements, status code badges, payload size, DNS/TCP/SSL/TTFB timing breakdowns, and full server SSL certificate inspection.

### 🔍 Diff & Comparison Engine
- **Side-by-Side Response Diff Viewer**: Interactive visual difference tool with synchronized scrolling, additions/deletions/edits coloring, pane swapping, and one-click loading from disk or past execution history.

### 🔐 Authentication Suite
- **Bearer Token**: Token strings with dynamic variable expansion.
- **Basic Authentication**: Base64-encoded username and password credentials.
- **API Key**: Header or query parameter placement.
- **AWS Signature Version 4 (SigV4)**: Full canonical request signing with HMAC-SHA256, date stamping, region, and service scope.
- **Digest & NTLM Authentication**: Automated challenge-response authentication.
- **mTLS**: Client certificate (`.crt` / `.pem`) and private key authentication.
- **Interactive OAuth 2.0 Helper**:
  - **Client Credentials Grant**: Instant token exchange via direct endpoint communication.
  - **Authorization Code Flow**: Embedded local loopback server (`127.0.0.1:8089/callback`) that opens your browser, captures the redirect code, exchanges it for an access token, and automatically sets the Bearer token.

### 🧪 Scripting & Assertions Engine
- **Declarative Zero-Code Assertions**: Visual assertion builder supporting `eq`, `neq`, `gt`, `gte`, `lt`, `lte`, `contains`, and `not_contains` against `res.status`, `res.responseTime`, headers, and nested JSON dot-paths (e.g. `res.body.users[0].id`).
- **Sandboxed JavaScript Testing**: Integrated `QJSEngine` runtime exposing a secure `poppy` namespace (`poppy.req`, `poppy.res`, `poppy.setEnvVar`, `poppy.getEnvVar`).
- **Standard Chai/Mocha-Style Test Suite**: Write custom pre-request and post-response validation scripts using familiar `expect(res.getStatus()).to.equal(200)` assertions.
- **Hierarchical Variables**: Scoped environment, collection, and folder-level variables with recursive parent inheritance.

### 🔄 Migration, Interoperability & Exporters
- **cURL Importer**: Paste any cURL command to instantly parse methods, URLs, headers, bodies, and auth.
- **Postman v2.1 Importer & Exporter**: Full two-way conversion between Postman v2.1.0 collections and `.bru` directory trees.
- **Insomnia v4 Importer & Exporter**: Ingest and export Insomnia v4 workspaces.
- **OpenAPI v3.0 Importer & Exporter**: Ingest OpenAPI specifications into collections, or export entire collections to valid OpenAPI 3.0.3 JSON specs.
- **HTTP Archive (.har 1.2) Exporter**: Export collections to standard HAR format for sharing with browser devtools.

### ⚡ Developer Ergonomics & Productivity
- **Quick Open Command Palette (`Ctrl+P`)**: Instant keyboard fuzzy switcher across all requests in the active collection with colored HTTP method badges and directory breadcrumbs.
- **In-Response Search & JSONPath Filter (`Ctrl+F`)**: Interactive search bar with document match highlighting, previous/next cycling, match counter, case-sensitive/regex toggles, and live JSONPath query filtering (`$.items[*].name`).
- **Multi-Tab Document Workspace**: Pinned tabs, tab renaming, drag-and-drop collection reordering, and debounced auto-save.
- **Request Execution History**: Dedicated sidebar history tab with status codes, latency badges, and one-click replay.
- **Sidebar Git Sync (`Ctrl+Shift+G`)**: Native Git version control dialog to inspect branch status, view changed `.bru` collection files, commit, pull, and push directly from the app.

### 🎭 Embedded Mock Server
- **Local HTTP Mocking Engine (`Tools -> Mock Server...`)**:
  - Run a lightweight HTTP mock server on any custom port (`8080`, `3000`, etc.).
  - Configure mock routes with HTTP method, path, custom headers, status codes, and JSON response bodies.
  - **Simulated Latency**: Add configurable delay (ms) to test frontend loading states and timeouts.
  - **One-Click Import**: Automatically populate mock endpoints from all requests in your active collection.
  - **Live Request Logging**: Inspect all incoming client requests, headers, and payloads in real time.

### 🌊 Server-Sent Events (SSE) Stream Inspector
- **Real-Time Event Streaming (`Tools -> Server-Sent Events...`)**:
  - Inspect live `text/event-stream` connections with sub-millisecond timeline logging.
  - Automatic event parsing (`event`, `id`, `data`, `retry`) with search and filtering.
  - **LLM Token Accumulator**: Automatically extracts and streams OpenAI/ChatGPT style `delta.content` tokens in real time.

### 📖 Interactive API Documentation Generator (`Tools -> Generate API Documentation...`)
- **Self-Contained HTML Export**: Export entire collections into a clean, responsive single-file offline documentation site (`docs.html`).
- **Sidebar & Live Search**: Instant live filtering across endpoints, methods, and paths.
- **Multi-Language Examples**: Embedded interactive code snippet tabs for cURL, Python (`requests`), and JavaScript (`fetch()`).
- **Full Variable Resolution**: Automatically evaluates environment and folder variables into realistic examples.

### 📊 Response Visualizer Tab & Summary Charts
- **Interactive JSON Data Grid**: Dedicated "Visualize" tab transforms JSON arrays into sortable data tables with column resizing and row filtering.
- **ASCII & Bar Chart Generator**: Automatically renders visual comparative bar charts for numeric metrics against label keys.
- **Property Inspector**: Nicely formats single JSON objects into Property, Value, and Type tables.

### 🔍 Variable Hover Tooltips & Autocomplete
- **Live Hover Inspection**: Hover over any `{{variable_name}}` in the URL bar to view its resolved value and originating scope (`Environment: ...`, `Folder: ...`, `Collection: ...`).
- **Intelligent Autocomplete**: Type `{{` in the URL bar to trigger instant popup completion of all active variable tokens.
- **Status Bar Quick-Look Widget**: Displays total active variable count with one-click full variable inspection table.

### 🍪 Response Cookies Inspector (Tab 7)
- **Parsed Cookie Table**: Dedicated response tab automatically parses all incoming `Set-Cookie` response headers into a clean table displaying Name, Value, Domain, Path, Expires / Max-Age, Secure, and HttpOnly flags.
- **Dynamic Badge Counter**: Instantly view how many cookies were set by the endpoint directly on the tab header (e.g. `Cookies (3)`).

### 🏷️ RFC HTTP Status Code Tooltips
- **Interactive Explanations**: Hover over any response status badge to view official RFC explanations and semantic descriptions (200 OK, 201 Created, 400 Bad Request, 401 Unauthorized, 403 Forbidden, 404 Not Found, 429 Too Many Requests, 500 Internal Server Error, etc.).

### 📋 cURL Paste Auto-Detection
- **Intelligent URL Bar Import**: Simply paste any raw `curl ...` command into the main URL bar. Poppy automatically detects the cURL syntax, parses method, headers, parameters, and request body, loads them straight into the active editor, and notifies you in the status bar.

### ⚡ Quick Sidebar Code Exporters
- **One-Click Export**: Right-click any request in either the Collection Tree or History Sidebar to instantly `Copy as Fetch (JS)` or `Copy as Python` without opening any modal dialogs.

### ✨ JSON Request Body Prettifier & Minifier
- **Instant Formatting**: Directly in the Body editor, click **Prettify** to re-indent and align JSON payloads or **Minify** to strip whitespace for compact wire transmission.
- **Real-Time Syntax Validation**: As you type, dynamic validation highlights valid JSON or points out syntax error details with exact byte offset indicators.

### 🔐 Environment Secrets Vault & Value Masking
- **Secret Value Masking**: Toggle sensitive environment variables (API tokens, passwords, private keys) with `••••••••` masking and an interactive `👁 Show Secrets` / `🔒 Hide Secrets` switch.
- **Secure File Isolation**: Secret variables are isolated and persisted into `.env.secret` files to prevent accidental leakage into public Git repositories.

### 📈 Session Network Telemetry & Bandwidth Tracker
- **Status Bar Live Telemetry**: Persistent status bar widget (`⚡ N reqs | 📦 N KB | ⏱ avg N ms`) continuously tracks request volume, received byte throughput, and average response latency.
- **Session Breakdown Modal**: Click the telemetry widget anytime to inspect execution counts, 2xx/3xx successes vs. 4xx/5xx failures, success percentage rate, and cumulative network latency, with a one-click session reset button.

### 📝 Markdown API Runbook Exporter (`Tools -> Export Collection as Markdown...`)
- **GitHub-Flavored API Runbook**: Export any collection to formatted markdown (`API_RUNBOOK.md`) with a table of contents, endpoint badges, query parameter & header tables, formatted JSON request bodies, declarative assertions, and copyable cURL commands.

### 🔎 Global Find & Replace Across Collection (`Ctrl+Shift+F`)
- **Deep Collection Search**: Search across all request names, URLs, headers, parameters, request bodies, and scripts across your entire collection.
- **Flexible Match Options**: Supports match case, whole-word matching, granular scope toggles, and selective or batch replacement across all matching requests.

### 🧪 GUI Data-Driven Collection Runner
- **CSV & JSON Fixture Support**: Load test data files directly inside the GUI Collection Runner dialog. Poppy automatically sets iteration counts matching your fixture rows and injects each row's columns into `{{variable}}` templates on every cycle.

### 🌓 Instant Theme Toggle (`Ctrl+T`)
- **One-Key Switcher**: Seamlessly toggle between dark and light themes at runtime using `Ctrl+T` or `View -> Toggle Dark/Light Theme`.

### 💻 Multi-Language Code Generation
- Export any request with one click to production-ready code across 9 languages and libraries:
  - **Python** (`requests`)
  - **JavaScript** (Fetch API & Axios)
  - **Go** (`net/http`)
  - **Rust** (`reqwest` + `tokio`)
  - **C#** (`System.Net.Http.HttpClient`)
  - **Java** (`java.net.http.HttpClient`)
  - **PHP** & **Ruby**
  - **C++** (`libcurl`)
  - **cURL** CLI command

### 🤖 Automation & Headless CLI Runner (`poppy-cli`)
Poppy includes a standalone CLI binary designed for CI/CD pipelines (GitHub Actions, GitLab CI, Jenkins):
- **Multiple Output Reporters**: Formatted terminal output, structured JSON, and standard **JUnit XML** (`--reporter junit`) for test dashboard ingestion.
- **Multi-Iteration Execution**: Run test suites for multiple loops (`--iterations <n>`).
- **Data-Driven Fixtures**: Bind runtime variables per iteration using JSON or CSV data files (`--data fixtures.json`).
- **Rate-Limiting & Pacing**: Configurable inter-request throttling (`--delay <ms>`).

### 🌐 Networking & Security Controls
- **Persistent Cookie Jar & Manager**: Cross-request session cookie storage using Netscape cookie format with a visual Cookie Manager (`Ctrl+K`).
- **Per-Request & Global Proxy**: HTTP, HTTPS, and SOCKS5 proxy routing with URL bar quick-override.
- **SSL / TLS Verification & Certificate Chains**: One-click certificate inspection and self-signed certificate toggles.
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

#### Local Standalone Package & Installer (Windows)
Create a standalone portable `.zip` and legit Windows Setup installer (`.exe`) bundled with all necessary Qt and MinGW runtime libraries via `windeployqt` and Inno Setup:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
# Generates:
#   dist/Poppy-windows-x64.zip (Portable)
#   release-installer/Poppy-windows-x64-setup.exe (Windows Setup Installer Wizard)
```

#### Automated CI/CD Releases (GitHub Actions)
The repository includes an automated release workflow ([`.github/workflows/release.yml`](.github/workflows/release.yml)):
- **Continuous Deploy**: Every push to `main` compiles, tests, packages, and updates the rolling [Latest](https://github.com/williampepple1/Poppy/releases/latest) GitHub Release with:
  - `Poppy-windows-x64-setup.exe` (Windows Installer with Start Menu, Desktop Shortcut, Uninstaller & `.bru` Association)
  - `Poppy-windows-x64.zip` (Windows Portable)
  - `Poppy-linux-x64.tar.gz` (Linux Portable)
  - `Poppy-macos.dmg` & `Poppy-macos.zip` (macOS Apple Silicon & Intel)
- **Version Tag**: Push any version tag (e.g. `v1.4.4`) to publish a release snapshot with all installers and archives:
  ```bash
  git tag v1.4.4
  git push origin v1.4.4
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
