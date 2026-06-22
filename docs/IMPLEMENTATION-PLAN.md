# Metis Antique Marketplace Plus -- Implementation Plan

**Version 1.2.48**

Roadmap and phased delivery plan for future capabilities.
All implementations target C++20 on Windows, Linux, and macOS.
No Docker, Doxygen, PyTest, GTest, or Jupyter.

---

## Table of Contents

1. [Phase 1 -- Platform Sync (Near Term)](#phase-1----platform-sync-near-term)
2. [Phase 2 -- Security Hardening](#phase-2----security-hardening)
3. [Phase 3 -- GPU Acceleration](#phase-3----gpu-acceleration)
4. [Phase 4 -- Reporting and Document Generation](#phase-4----reporting-and-document-generation)
5. [Phase 5 -- POS Expansion](#phase-5----pos-expansion)
6. [Phase 6 -- Container Management (Native C++20)](#phase-6----container-management-native-c20)
7. [Phase 7 -- Kubernetes Orchestration (Native C++20)](#phase-7----kubernetes-orchestration-native-c20)
8. [Phase 8 -- Multi-Location and Replication](#phase-8----multi-location-and-replication)
9. [Guiding Constraints](#guiding-constraints)

---

## Phase 1 -- Platform Sync (Near Term)

**Goal:** Activate the eBay, Etsy, Chairish, and 1stDibs sync channels
that are already wired in config.pson.

### Deliverables

- C++20 HTTP/1.1 client with chunked transfer and gzip decoding
- eBay REST API client (OAuth 2.0 refresh token flow)
- Etsy v3 API client (API key + OAuth)
- Chairish API client
- 1stDibs API client (manual confirmation guard)
- Sync thread: poll at `sync_interval_min` interval, reconcile sold status
- `/api/sync/status` endpoint -- last sync time and error per channel
- UI panel in Listings page showing sync channel status

### Implementation notes

- All credentials come from config.pson `sync.*` block; none hardcoded
- `sandbox = true` in config.pson routes to sandbox API endpoint
- Auto-mark-sold writes a sale record and sets item status to 'sold'
- Sync errors logged to standard logger; not fatal

---

## Phase 2 -- Security Hardening

**Goal:** FIDO2 second factor for admin accounts and ACME certificate renewal.

### Deliverables

- FIDO2 / WebAuthn C++20 server implementation (no external lib)
- Authenticator registration: `POST /api/auth/webauthn/register`
- Assertion verification: `POST /api/auth/webauthn/verify`
- TOTP fallback: RFC 6238 TOTP in C++20 (HMAC-SHA1)
- ACME v2 client: Let's Encrypt cert provisioning and auto-renewal
- IP allowlist: CIDR-based block in config.pson `security.ip_allowlist`
- Audit log HMAC signing: SHA-256 chain on audit_log entries

---

## Phase 3 -- GPU Acceleration

**Goal:** Native C++20 GPU integration via OpenCL (cross-platform) and CUDA (NVIDIA).

### Deliverables

- OpenCL device enumeration at startup; log selected device
- GPU-accelerated JPEG resize for photo thumbnails (currently CPU-bound)
- GPU image similarity hash for duplicate photo detection
- CUDA backend (optional, NVIDIA-specific, `gpu_backend=cuda` in config.pson)
- `/api/gpu/status` endpoint -- device name, compute units, memory
- All gated by `compute.gpu_enabled = true` in config.pson

### Platform notes

- Windows: OpenCL via MSVC-compatible ICD loader (no Docker)
- Linux: OpenCL via system ICD (nvidia-opencl-dev or mesa-opencl-icd)
- macOS: OpenCL via Metal bridge or deprecated Apple OpenCL

---

## Phase 4 -- Reporting and Document Generation

**Goal:** PDF export and enhanced analytics, native C++20, no external tools.

### Deliverables

- C++20 PDF writer (RFC 32000 subset: text, lines, tables, simple images)
- PDF appraisal report: formatted document with item details and photos
- PDF consignor statement: printable payout summary per consignor
- PDF P&L report: date-range report with category table
- SVG trend charts: category revenue over time (server-side SVG generation)
- Buyer analytics: repeat buyer identification, lifetime value report
- Price recommendation: comparable sold items analysis
- Tax report: sales tax collected by jurisdiction, CSV export

---

## Phase 5 -- POS Expansion

**Goal:** Full hardware integration for walk-in retail operations.

### Deliverables

- USB serial receipt printer support (ESC/POS over COM port)
- Customer-facing display protocol (VFD via serial)
- Split tender: record cash + card amounts on single transaction
- Layaway: partial payment records with balance tracking
- Gift receipt: printer template without cost fields
- Integrated card reader: Square Terminal and Stripe Reader SDK (C++ wrapper)

---

## Phase 6 -- Container Management (Native C++20)

**Goal:** OCI-compatible container management without Docker.

### Deliverables

- OCI image fetch: pull from any OCI-compatible registry via C++20 HTTPS client
- Image unpack: layer extraction and overlay filesystem setup
- Linux namespace isolation: clone(2) with CLONE_NEWUSER/NEWPID/NEWNET/NEWNS
- cgroup v2 resource limits: CPU shares and memory limit via /sys/fs/cgroup
- Container health check: HTTP probe to configurable port
- `/api/containers` endpoint: list, start, stop, inspect
- macOS and Windows: lightweight VM shim via HyperKit / Hyper-V API
- All controlled by `compute.containers_enabled` in config.pson

---

## Phase 7 -- Kubernetes Orchestration (Native C++20)

**Goal:** Native C++20 Kubernetes API client for multi-node deployments.

### Deliverables

- kube-apiserver REST client over TLS (kubeconfig or in-cluster token)
- Pod lifecycle: GET/POST/DELETE /api/v1/namespaces/{ns}/pods
- ConfigMap sync: push config.pson sections to Kubernetes ConfigMaps
- Health probe registration: liveness and readiness endpoints in server.cpp
- Rolling update coordination: annotate deployment, watch rollout status
- `/api/k8s/status` endpoint: node count, pod count, cluster health
- All controlled by `compute.kubernetes_enabled` in config.pson

---

## Phase 8 -- Multi-Location and Replication

**Goal:** Support dealers with multiple physical locations and synchronized inventory.

### Deliverables

- Location table: named locations (stores, warehouses, booths)
- Items tagged to location with transfer workflow and audit trail
- Per-location P&L report
- C++20 replication protocol: delta sync between Metis instances over TLS
- Conflict resolution: last-write-wins with audit trail for manual review
- `/api/locations` CRUD endpoints
- `/api/sync/replicate` endpoint: trigger manual sync between instances

---

## Guiding Constraints

All phases adhere to the Metis Suite invariants:

- C++20 only. No Python, no JavaScript, no shell scripts in server code.
- Zero external runtime dependencies. No Docker, Doxygen, PyTest, GTest, Jupyter.
- PSON-only configuration. No hardcoded operational values.
- Exe-relative paths. No absolute paths.
- Single VERSION file drives all version strings.
- All docs in `docs/`. README.md at project root.
- Pure 7-bit ASCII in all shipped source files.
- CMake/Ninja build. `CMAKE_SUPPRESS_REGENERATION` set.
