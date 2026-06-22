# Metis Antique Marketplace Plus -- Commercial TODO

**Version 1.2.48**

Features and capabilities identified for future commercial release.
Items are grouped by category and ordered by estimated implementation priority.

No Docker, Doxygen, PyTest, GTest, or Jupyter. All implementations target
C++20 on Windows, Linux, and macOS.

---

## Table of Contents

1. [Platform Integrations](#1-platform-integrations)
2. [GPU Acceleration](#2-gpu-acceleration)
3. [Kubernetes Orchestration](#3-kubernetes-orchestration)
4. [Container Management](#4-container-management)
5. [Security Enhancements](#5-security-enhancements)
6. [Reporting and Analytics](#6-reporting-and-analytics)
7. [POS Enhancements](#7-pos-enhancements)
8. [Document Generation](#8-document-generation)
9. [Backup and Recovery](#9-backup-and-recovery)
10. [Multi-Location Support](#10-multi-location-support)

---

## 1. Platform Integrations

- [ ] **eBay active sync** -- poll eBay API for new orders, auto-mark sold in inventory
- [ ] **Etsy active sync** -- poll Etsy Listings API, reconcile sold status
- [ ] **Chairish active sync** -- webhook receiver for Chairish order events
- [ ] **1stDibs active sync** -- manual confirmation flow (platform requirement)
- [ ] **Ruby Lane integration** -- listing submission and order polling
- [ ] **USPS / UPS / FedEx label generation** -- C++20 REST client per carrier API
- [ ] **QuickBooks export** -- CSV format compatible with QuickBooks import
- [ ] **Xero export** -- reconciliation CSV for Xero accounting

---

## 2. GPU Acceleration

Planned as native C++20 via OpenCL (cross-platform) or CUDA (NVIDIA).
No Docker, no external GPU orchestration tools.

- [ ] **GPU-accelerated image resizing** -- batch photo thumbnail generation
- [ ] **GPU-accelerated image similarity** -- detect duplicate inventory photos
- [ ] **GPU analytics pipeline** -- accelerated P&L aggregation over large datasets
- [ ] **OpenCL backend** -- cross-platform GPU support (Windows, Linux, macOS)
- [ ] **CUDA backend** -- NVIDIA-specific path for higher throughput
- [ ] **Device selection** -- `compute.gpu_device_index` in config.pson

---

## 3. Kubernetes Orchestration

Planned as native C++20 Kubernetes API client. No kubectl, no Docker required.

- [ ] **C++20 Kubernetes REST client** -- connect to kube-apiserver via TLS
- [ ] **Pod lifecycle management** -- create / list / delete pods programmatically
- [ ] **Namespace isolation** -- `compute.kubernetes_namespace` in config.pson
- [ ] **Health probe integration** -- liveness and readiness endpoints
- [ ] **Config map sync** -- push config.pson sections to Kubernetes ConfigMaps
- [ ] **Rolling update support** -- coordinate multi-instance deployments

---

## 4. Container Management

Planned as native C++20 OCI-compatible container runtime. No Docker daemon.

- [ ] **OCI image pull** -- fetch and unpack OCI images via C++20 HTTP client
- [ ] **Namespace isolation** -- Linux namespaces (user, net, pid, mnt) via syscall
- [ ] **cgroup resource limits** -- CPU and memory limits for spawned containers
- [ ] **Volume mount** -- bind-mount data/ and photos/ into container filesystem
- [ ] **Container health checks** -- HTTP probe to container service port
- [ ] **macOS / Windows shim** -- lightweight VM shim for non-Linux platforms

---

## 5. Security Enhancements

- [ ] **FIDO2 / WebAuthn second factor** -- hardware key support for admin accounts
- [ ] **Let's Encrypt / ACME** -- automatic certificate provisioning and renewal
- [ ] **TOTP second factor** -- RFC 6238 TOTP authenticator app support
- [ ] **IP allowlist** -- restrict admin access to configured CIDR blocks
- [ ] **Audit log signing** -- HMAC-signed audit entries for tamper detection
- [ ] **Encrypted database** -- SQLCipher integration (optional, config-gated)

---

## 6. Reporting and Analytics

- [ ] **PDF export** -- C++20 native PDF generation (no LibreOffice, no Puppeteer)
- [ ] **Category trend charts** -- SVG line charts for category revenue over time
- [ ] **Buyer analytics** -- repeat buyer identification, lifetime value
- [ ] **Source ROI** -- acquisition source (estate sale, auction, etc.) vs. margin
- [ ] **Price recommendation** -- comparable sold items, suggested asking price
- [ ] **Tax report** -- sales tax collected by jurisdiction, export for filing
- [ ] **Inventory aging** -- items unsold past N days flagged for price review

---

## 7. POS Enhancements

- [ ] **USB serial printer support** -- ESC/POS over COM port (Windows / Linux)
- [ ] **Customer display** -- second display for buyer-facing price confirmation
- [ ] **Gift receipt** -- separate receipt without cost fields
- [ ] **Split tender** -- record cash + card split on single transaction
- [ ] **Layaway tracking** -- partial payment records with balance due
- [ ] **Integrated card reader** -- Square / Stripe terminal SDK integration

---

## 8. Document Generation

- [ ] **Consignor agreement PDF** -- generate from consignor record
- [ ] **Appraisal report PDF** -- formatted appraisal document for client delivery
- [ ] **Insurance valuation letter** -- signed PDF for insurance submission
- [ ] **Packing list** -- per-sale packing slip with item details
- [ ] **Purchase order** -- acquisition PO for estate or auction purchases

---

## 9. Backup and Recovery

- [ ] **Restore from backup UI** -- select backup file, restore with confirmation
- [ ] **S3-compatible backup** -- push nightly backup to MinIO / Backblaze B2 / AWS S3
- [ ] **Incremental backup** -- SQLite WAL checkpointing for continuous backup
- [ ] **Backup integrity check** -- SHA-256 manifest verification on restore

---

## 10. Multi-Location Support

- [ ] **Multi-location inventory** -- items tagged to physical location / warehouse
- [ ] **Location transfer** -- move items between locations with audit trail
- [ ] **Per-location P&L** -- revenue and expense breakdown by location
- [ ] **Multi-server sync** -- replicate inventory between Metis instances
