# Metis Antique Marketplace Plus -- Background

**Version 1.2.48**

---

## Table of Contents

1. [History and Purpose of Antique Marketplace Software](#1-history-and-purpose-of-antique-marketplace-software)
2. [How Metis Antique Marketplace Plus Fits the Ecosystem](#2-how-metis-antique-marketplace-plus-fits-the-ecosystem)
3. [Design Philosophy](#3-design-philosophy)
4. [Technology Choices](#4-technology-choices)
5. [The Metis Suite Context](#5-the-metis-suite-context)

---

## 1. History and Purpose of Antique Marketplace Software

### The Pre-Digital Era

Before personal computing, antique dealers operated on handwritten ledgers, paper
price tags, and physical Rolodex files of buyers and consignors. Provenance records
were typed or handwritten on index cards. Pricing relied on reference books --
Kovels, Warman's, Miller's -- and dealer networks formed through trade shows and
auction house relationships.

The central challenge of the antique trade has always been information asymmetry:
a dealer who knows the true history, rarity, and market demand of an object holds
an enormous advantage. Managing that knowledge -- who bought what, what it cost,
what it sold for, who appraised it -- was the core operational problem.

### Early Software (1980s -- 1990s)

The first antique inventory tools were generic small-business database applications
adapted to dealer needs -- early versions of dBASE, FileMaker, and later Microsoft
Access were pressed into service. These required significant customization and
offered no integration with selling platforms because no selling platforms existed.

The introduction of the Internet changed everything. When eBay launched in 1995 and
began attracting antique and collectible sellers, the data problem exploded in scope.
Dealers suddenly had to track not just physical inventory but online listings, buyer
communications, shipping records, and feedback scores across multiple platforms.

### Marketplace Proliferation (2000s -- 2010s)

The 2000s brought a wave of specialized antique and collectibles marketplaces:
Ruby Lane (1998), 1stDibs (2000), Chairish (2013), and many regional platforms.
Each required its own listing format, pricing strategy, and communication workflow.
Multi-channel selling -- listing the same item across eBay, 1stDibs, and a dealer's
own website simultaneously -- became standard practice and a significant management
burden.

Point-of-sale needs grew as the distinction between online and physical sales blurred.
Dealers operating booths in antique malls needed systems that could handle walk-in
sales at the counter while keeping inventory synchronized with their online listings.

Software evolved to address these needs through specialized products:
- Inventory management systems with photo galleries and condition grading
- Multi-channel listing tools with cross-platform sync
- POS terminals integrated with inventory for instant sold-status updates
- Appraisal workflow systems for estate and insurance purposes
- Consignment tracking for dealers holding goods on behalf of other owners
- Booth rental management for mall operators managing dozens of dealer spaces

### The Dependency Problem

Most commercial solutions accumulated heavy dependencies: cloud databases requiring
ongoing subscription fees, browser-based SaaS products with data portability
concerns, point-of-sale hardware locked to proprietary ecosystems, and accounting
integrations requiring third-party middleware.

For independent dealers -- the backbone of the antique trade -- these dependency
stacks introduced cost, complexity, and data sovereignty risks that outweighed
the convenience of integrated features. A dealer whose business model depends on
knowing the provenance and purchase history of every object they handle cannot
afford to have that knowledge locked in a vendor's database.

### Modern Needs

Today's antique marketplace operator typically requires:

- Inventory tracking with rich provenance and condition fields
- Photo management tied to items
- Multi-channel listing records (not necessarily automated sync)
- Sales recording with buyer details, margin calculation, and tax support
- Professional appraisal workflow
- Consignment tracking with consignor statements
- Booth rent management for mall or co-op operators
- Point-of-sale capability with barcode and receipt support
- P&L and portfolio analytics
- Audit trails for insurance and estate purposes
- Secure, local operation without cloud dependencies

---

## 2. How Metis Antique Marketplace Plus Fits the Ecosystem

### Positioning

Metis Antique Marketplace Plus occupies the space between two failure modes common
in antique marketplace software:

**Over-engineered SaaS platforms** that require ongoing subscription fees, store
dealer data in vendor-controlled cloud databases, and lock operators into proprietary
ecosystems with limited export capability.

**Underbuilt spreadsheet solutions** that fail when inventory exceeds a few hundred
items, offer no multi-user access, cannot manage photos or appraisals, and provide
no audit trail.

Metis Antique Marketplace Plus is self-hosted, zero-external-dependency software
that runs on the dealer's own hardware -- a Windows workstation, a Linux server, or
a macOS machine -- with all data stored locally in a single SQLite file that the
dealer owns and controls.

### Scope of Coverage

The application covers the full operational workflow of an independent antique
dealer or small antique mall operator:

| Workflow Stage         | Coverage                                                   |
|------------------------|------------------------------------------------------------|
| Acquisition            | Purchase recording with cost basis, source, and provenance |
| Inventory management   | Rich metadata, photo gallery, SKU, barcode labels          |
| Appraisal              | Request tracking, appraiser records, valuation ranges      |
| Consignment            | Consignor tracking, percentage splits, statements          |
| Listing                | Multi-channel listing records with view tracking           |
| POS / Walk-in sales    | Barcode scanner, receipt printer, cash drawer support      |
| Online sale recording  | Buyer details, platform fees, shipping, net proceeds       |
| Settlement             | Per-dealer payout calculation for mall operators           |
| Booth management       | Rent invoicing, discounts, late fees, automated reminders  |
| Analytics              | P&L, portfolio value, category revenue, ROI by source      |
| Audit                  | Full change trail, session log, failed login tracking      |
| Multi-channel sync     | eBay, Etsy, Chairish, 1stDibs API integration (configurable) |

### What It Is Not

Metis Antique Marketplace Plus is not an accounting system, a shipping fulfilment
platform, or a customer-facing e-commerce storefront (though the public `/shop`
endpoint provides a read-only browsing view). It integrates with the dealer's
existing accounting and shipping workflows rather than replacing them.

### Competitive Differentiation

| Attribute              | Metis Antique Marketplace Plus       | Typical SaaS Alternative             |
|------------------------|--------------------------------------|---------------------------------------|
| Hosting                | Self-hosted, dealer's hardware       | Vendor cloud                          |
| Data ownership         | Local SQLite file, dealer owns it    | Vendor database                       |
| Dependencies           | Zero runtime (SQLite bundled)        | Multiple cloud services               |
| TLS                    | OpenSSL 4.0.0, TLS 1.3, PQ hybrid   | Vendor-managed                        |
| Ongoing cost           | None after build                     | Monthly/annual subscription           |
| Source access          | Full C++20 source included           | Closed source                         |
| Export                 | Direct SQLite access + CSV export    | Vendor-controlled export              |
| Multi-user             | Role-based (admin/dealer/viewer)     | Typically plan-gated                  |
| Offline operation      | Fully local, no internet required    | Internet-dependent                    |

---

## 3. Design Philosophy

### Zero External Runtime Dependencies

Every dependency is either compiled into the binary or bundled as source.
SQLite 3.53.1 is included as the standard amalgamation. OpenSSL 4.0.0 is bundled
as prebuilt static libraries for Windows; on Linux and macOS the system OpenSSL
is used if available, and TLS is gracefully disabled if not.

There is no requirement for a web server, a database server, a message broker,
a container runtime, or any cloud service. One executable handles every request.

### PSON-Only Configuration

All operational parameters -- ports, credentials, currency, email, POS hardware
addresses, sync channel credentials, booth rent rules, SKU prefixes, and fee
schedules -- live in `config.pson`. Nothing operational is hardcoded in source.
This allows the same binary to serve a single-dealer shop in US dollars, a
multi-booth mall in euros, or a consignment gallery in British pounds without
recompilation.

The PSON format (Metis Property Object Notation) is a clean, comment-friendly
configuration language purpose-built for the Metis Suite. It is simpler than
JSON (no quotes on keys, comments supported) and more structured than INI files.

### Exe-Relative Paths

All file paths -- database, log directory, photo storage, web root, TLS
certificates -- are resolved relative to the executable. This means the entire
application can be moved by copying the build output directory, and multiple
instances can run side by side on different ports with independent configurations.

### Vanilla JavaScript, No Build Step

The browser client is plain HTML5, CSS3, and ES2020 JavaScript. No framework,
no bundler, no transpiler, no node_modules. The `web/` directory is served
directly as static files. This means the client loads instantly, works in any
modern browser, and can be inspected and modified without a build environment.

### Role-Based Access at Two Layers

Authentication and authorization are enforced at two independent layers:
the TCP server (viewer writes blocked before reaching any handler) and individual
route handlers (admin-only operations checked explicitly). This means a bug in
one handler cannot accidentally grant write access to a viewer account.

### Future-Proof Compute Architecture

GPU acceleration, Kubernetes-style orchestration, and OCI container management
are designed as native C++20 implementations -- no Docker, no external orchestration
tools required. These capabilities are gated by `compute.*` settings in config.pson
and will activate as the C++20 implementations mature. The architecture ensures
these features integrate at the binary level rather than through external services.

### Audit-First Data Model

Every write operation is recorded in the audit trail. The audit log captures the
user, role, action, affected record, and timestamp for every create, update, and
delete. This is not optional instrumentation -- it is a first-class requirement
for a system handling objects whose value depends on documented provenance and
chain of custody.

### Single-File SQLite with WAL Mode

SQLite with WAL (Write-Ahead Logging) provides full ACID semantics, concurrent
reads during writes, and crash recovery without a separate database server process.
The database file is a single portable artifact that can be backed up with a file
copy, opened with any SQLite browser for ad-hoc queries, and restored without
special tooling.

---

## 4. Technology Choices

### C++20

C++20 provides the features needed for clean, maintainable server code without
external frameworks: coroutine foundations, concepts for type-safe generics,
ranges for data pipelines, `std::filesystem` for portable path handling, and
`std::chrono` for timestamp precision. The Metis codebase targets C++20 across
MinGW-w64 GCC 13 (Windows), GCC 13+ (Linux), and Clang 15+ (macOS).

### CMake / Ninja

CMake provides portable build configuration across all three target platforms
without IDE lock-in. Ninja provides fast incremental builds. The CMake build
auto-detects OpenSSL, auto-generates TLS certificates with Subject Alternative
Name fields, copies assets to the build output directory, and writes platform-
specific helper scripts -- all without user intervention.

### OpenSSL 4.0.0

OpenSSL 4.0.0 provides TLS 1.3 with post-quantum hybrid key exchange via
X25519MLKEM768. The prebuilt Windows static libraries are included in the
repository, eliminating the most common Windows build friction point.

### SQLite 3.53.1

The SQLite amalgamation is the standard approach for embedded database integration.
FTS5 and JSON1 extensions are compiled in for future full-text search and JSON
column support. WAL mode and foreign key enforcement are set on every open.

---

## 5. The Metis Suite Context

Metis Antique Marketplace Plus is one product in the Metis Suite, a family of
zero-dependency C++20 HTTP server applications built on a shared architecture.
Other Suite members address healthcare records (Metis Medical Plus), church
management (Metis Church Plus), financial analysis (Metis Financial Analyzer Plus),
legislative intelligence (Metis Congressional Intelligence Plus), system monitoring
(Metis System Monitor Platform), and AV security (Metis AV Guard).

All Suite members share:
- The same PSON configuration system
- The same exe-relative path model
- The same logger with millisecond timestamps and weekly rotation
- The same CMake/Ninja build infrastructure
- The same zero-external-dependency philosophy
- VERSION file as the single source of truth for version strings

Metis Antique Marketplace Plus extends the shared architecture with domain-specific
modules: photo management, appraisal workflow, consignment tracking, multi-channel
listing sync, POS hardware integration, and booth rent management.

---

*Metis Antique Marketplace Plus -- built and maintained by Bennie Shearer (Retired)*
