# Metis Antique Marketplace Plus -- TODO

**Version 1.2.50**

---

## Completed

- [x] Multi-photo gallery per item (v1.0.5)
- [x] Dealer internal comments / notes (v1.0.5)
- [x] Buyer inquiry system with reply threads (v1.0.5)
- [x] Public buyer storefront at /shop (v1.0.6)
- [x] Audit & Logs page with trail, log viewer, sessions (v1.0.7)
- [x] Failed logins written to DB (v1.0.7)
- [x] Item detail 6-tab modal: Details|Photos|Notes|Purchase|Inquiries|Audit (v1.0.7/v1.0.9)
- [x] Purchases ledger with full cost breakdown (v1.0.8/v1.0.9)
- [x] Expenses tracking by category (v1.0.8/v1.0.9)
- [x] P&L report with date range and by-category table (v1.0.8/v1.0.9)
- [x] Item picker dropdown for purchase recording (v1.0.9)
- [x] Mobile layout -- hamburger sidebar below 768px (v1.1.7)
- [x] Keyboard navigation -- arrows, /, n, Ctrl+Enter (v1.1.7)
- [x] WebSocket real-time log streaming /ws/logs (v1.1.7)
- [x] QR code label printing -- pure-JS SVG QR encoder (v1.1.7)
- [x] SQLite updated to 3.53.1 (v1.1.8)
- [x] OpenSSL 4.0.0 prebuilt added for Windows (v1.1.8)
- [x] TLS listener on tls_port 8481 (v1.2.2)
- [x] All hardcoded operational values moved to config.pson (v1.2.16)
- [x] Email receipts via SMTP STARTTLS (v1.2.16)
- [x] GET /api/config endpoint (v1.2.16)
- [x] i18n support -- 6 locales (v1.2.14)
- [x] Font size controls (v1.2.14)
- [x] Dashboard page with KPIs (v1.2.15)
- [x] Multi-currency via Intl.NumberFormat (v1.2.22)
- [x] TLS/AES/Post-Quantum badges in sidebar (v1.2.23)
- [x] Treasure chest logo -- logo.svg + icon.svg (v1.2.20)
- [x] App name driven by config.pson app.name (v1.2.28)
- [x] Consignments module (v1.2.30)
- [x] Rentals module (v1.2.30)
- [x] Advertising module (v1.2.30)
- [x] Credit card fee calculator (v1.2.30)
- [x] Per-dealer inventory isolation -- owner_id on items and sales (v1.2.31)
- [x] SKU auto-generation PREFIX-YYYY-NNNNN (v1.2.31)
- [x] Code 128 barcode SVG renderer on QR labels (v1.2.31)
- [x] Automatic settlement checks -- nightly thread (v1.2.31)
- [x] Dealer personal dashboard (v1.2.31)
- [x] Vendor rent collection + automated booth discount engine (v1.2.32)
- [x] Three-tier discount: early payment, loyalty, multi-booth (v1.2.32)
- [x] Booth invoices with overdue detection and late fees (v1.2.32)
- [x] User email and display_name fields (v1.2.34)
- [x] Session revoke from Users page (v1.2.34)
- [x] Portfolio page -- inventory value by category + ROI by source (v1.2.34)
- [x] Consignor statement -- printable HTML per consignor (v1.2.34)
- [x] Bulk CSV import UI (wired to POST /api/items/import) (v1.2.34)
- [x] Sales CSV export button (wired to GET /api/sales/export) (v1.2.34)
- [x] Full documentation suite -- all 17 Metis Docs Model files (v1.2.48)
- [x] BACKGROUND.md with Table of Contents (v1.2.48)
- [x] CONFIGURATION.md -- all config.pson parameters documented (v1.2.48)
- [x] DATA_MODEL.md -- all 18 tables with columns (v1.2.48)
- [x] GPU / Kubernetes / Container stubs in config.pson compute block (v1.2.48)


---

## Deferred (require external services or significant scope)

- [ ] eBay / 1stDibs / Etsy API integration for automatic listing sync
- [ ] FIDO2 / WebAuthn second factor for admin accounts
- [ ] Let's Encrypt / ACME automatic certificate renewal
- [ ] PDF export of reports
- [ ] Restore from backup UI (currently manual file replace)
