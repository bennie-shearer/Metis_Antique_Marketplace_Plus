# Metis Antique Marketplace Plus -- Comparison

**Version 1.2.50**

How Metis Antique Marketplace Plus compares to common alternatives.

---

## Feature Comparison

| Feature                         | Metis AMP      | SaaS (typical) | Spreadsheet  | Custom Access |
|---------------------------------|----------------|----------------|--------------|---------------|
| Self-hosted / no cloud          | Yes            | No             | Yes          | Yes           |
| Zero runtime dependencies       | Yes            | No             | No           | No            |
| Source code included            | Yes (C++20)    | No             | N/A          | Partial       |
| Multi-user with roles           | Yes (3 roles)  | Plan-gated     | No           | Custom        |
| TLS 1.3 + Post-Quantum          | Yes            | Vendor-managed | No           | Rarely        |
| SQLite -- single file, portable | Yes            | No             | N/A          | No            |
| REST API (121 endpoints)        | Yes            | Limited export | No           | Custom        |
| Photo gallery per item          | Yes            | Yes            | No           | Custom        |
| Multi-channel listing records   | Yes            | Yes            | No           | Custom        |
| POS (barcode, receipt, drawer)  | Yes            | Yes (hardware) | No           | Custom        |
| Appraisal workflow              | Yes            | Sometimes      | No           | Rarely        |
| Consignment with statements     | Yes            | Sometimes      | Manual       | Custom        |
| Booth rent + discount engine    | Yes            | Sometimes      | Manual       | Rarely        |
| P&L with expense categories     | Yes            | Yes            | Manual       | Custom        |
| Audit trail                     | Yes            | Sometimes      | No           | Rarely        |
| eBay / Etsy / Chairish sync     | Config-ready   | Yes            | No           | Custom        |
| QR + Code 128 barcode labels    | Yes            | Sometimes      | No           | Rarely        |
| WebSocket real-time log         | Yes            | No             | No           | No            |
| GPU / Kubernetes (planned)      | C++20 native   | Vendor choice  | No           | No            |
| Monthly subscription cost       | None           | $30-$200/mo    | None         | Build cost    |
| Data portability                | Full (SQLite)  | Vendor export  | Full         | Depends       |
| Offline operation               | Yes            | No             | Yes          | Yes           |
| Windows / Linux / macOS         | All three      | Browser-based  | All three    | Windows only  |

---

## Subscription Cost Analysis

A typical antique dealer using SaaS inventory software pays $50-$150 per month.
Over five years that is $3,000-$9,000. Metis Antique Marketplace Plus has no
recurring cost after build. The source, build toolchain (GCC, CMake, Ninja), and
all bundled dependencies (SQLite, OpenSSL, bcrypt) are free of charge.

---

## Data Sovereignty

SaaS platforms store dealer data in vendor-controlled cloud databases. When a
subscription lapses or a vendor shuts down, access to years of inventory history,
provenance records, and buyer data may be lost or held hostage to export fees.

Metis Antique Marketplace Plus stores everything in a single SQLite file that
the dealer owns. It can be opened with any SQLite browser (DB Browser for SQLite,
DBeaver, etc.), backed up with a file copy, and moved to any machine without
vendor involvement.

---

## Technical Architecture Comparison

| Attribute          | Metis AMP              | Node.js SaaS          | PHP/MySQL SaaS        |
|--------------------|------------------------|-----------------------|-----------------------|
| Language           | C++20                  | JavaScript            | PHP                   |
| Server             | Hand-written TCP       | Express / Fastify     | Apache / nginx        |
| Database           | SQLite (embedded)      | PostgreSQL / MongoDB  | MySQL                 |
| TLS                | OpenSSL 4.0.0          | Node TLS              | Apache/nginx TLS      |
| Config             | PSON (single file)     | .env / JSON           | .env / PHP config     |
| Dependencies       | Zero runtime           | node_modules (many)   | Composer (many)       |
| Build              | CMake + Ninja          | npm install           | composer install      |
| Binary             | Single native exe      | Node runtime required | PHP runtime required  |
| Memory footprint   | ~10-30 MB RSS          | ~80-200 MB RSS        | ~50-150 MB RSS        |
