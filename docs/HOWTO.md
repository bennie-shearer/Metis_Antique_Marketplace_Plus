# Metis Antique Marketplace Plus -- How-To Guide

**Version 1.2.48**

---

## Getting started

1. Open `https://localhost:8481` (TLS) or `http://localhost:8480`
2. Log in as `admin` / `Antique#2026`
3. Go to **Users** and change all default passwords immediately
4. Edit `config.pson` to set your app name, currency, and SMTP details

---

## Dashboard

Default landing page for admin. Shows KPIs, recent sales, recent acquisitions.
Dealers land on **My Dashboard** — a personal real-time view of only their own
inventory and sales.

---

## Inventory

**Add item** — Click **+ Add item**. Title and category are required. Cost price
and asking price drive the P&L and portfolio reports. Condition, provenance,
maker, era, and origin improve searchability.

**SKU** — Generated automatically on save: `PREFIX-YYYY-NNNNN` (prefix set via
`sku.prefix` in config.pson). Printed on QR labels as both text and Code 128
barcode.

**Edit / duplicate / delete** — Buttons on each row. Duplicate copies all
fields for similar items.

**CSV bulk import** — Click **📁 Import CSV** in the inventory toolbar. File
must have header row: `title,category,era,maker,origin,material,condition,
cost_price,asking_price,provenance`. Title and category required.

**QR + barcode labels** — Click **QR Labels** in the sidebar. Filter by status
or select items, then print from the browser.

---

## Listings

Record items as listed on a selling channel. Supported channels: 1stDibs, eBay,
Etsy, Ruby Lane, Chairish, etc. Creating a listing sets the item status to
**Listed**. Deleting a listing (soft delete) resets status to inventory.

---

## Sales

**Record a sale** — Select an item, enter price, platform fee, shipping, buyer
details, and payment method. Net proceeds are calculated automatically.
Recording a sale sets item status to **Sold**.

**Email receipt** — If a buyer email is recorded on the sale, an ✉ button
appears on the row. Click to send an HTML receipt via the configured SMTP server.
Requires `email.enabled = true` in config.pson.

**CSV export** — Click **📊 Export CSV** on the sales page to download all
sales for the current year.

---

## Appraisals

Request an appraisal for any item. Track appraiser name, valuation range,
condition notes, and status (pending → in review → complete).

---

## Purchases and Expenses

**Purchases** — Record acquisition costs per item. Feeds into P&L cost of goods.

**Expenses** — Record operating expenses by category (shipping, insurance,
repairs, booth fees, advertising). Feeds into P&L net profit.

---

## P&L Report

Select a date range and click **Run report**. Shows: revenue, COGS, gross
profit, expenses by category, and net profit. Margin percentages auto-calculated.

---

## Portfolio

Shows all items currently in stock:
- By category: item count, asking value, cost basis, unrealised gain, markup %
- ROI by acquisition source: average return across all sold items per source

---

## Market

Revenue by category, monthly revenue trend (last 12 months), and top 10 items
by profit across all recorded sales.

---

## Consignments

Track items received on consignment from external sellers.

**Add consignment** — Enter consignor name, email, phone, agreed percentage,
and optional floor price. Payout is calculated automatically when you mark the
item as sold: `sale_price × agreed_pct / 100`, with floor enforced.

**Update status** — Edit the consignment and set status to `sold`, enter the
sale price. Payout amount is auto-calculated. Set `payout_date` when you pay.

**Print consignor statement** — Click **📄 Statement** on any row. Opens a
printable HTML statement showing all items, sales, payout earned, and amount
owing for that consignor.

---

## Rentals and Booth Fees

Track booth rents, storage, display space, and any other recurring premises costs.

**Add rental** — Enter vendor, location, monthly rate, due day, grace days, and
late fee %. Set a manual discount % to override the automatic discount engine.

**Automated discounts** — Three tiers applied in order (first match wins):
1. **Early payment** — configurable % off if paid before day N of the month
2. **Loyalty** — configurable % off after N consecutive paid months
3. **Multi-booth** — configurable % off when vendor has ≥ N active booths
All thresholds set in `config.pson` `[booth_rent]` block.

**Collect rent** — Click **💳 Collect** on a rental row. The discount engine
runs automatically and shows the breakdown before saving.

**Generate invoice** — Click **📋 Invoice** on a rental row. Prompts for period,
generates an invoice record and returns a full HTML invoice with discount
and late fee breakdown.

**Automated collection** — Enable with `booth_rent.collection_enabled = true`.
The server generates invoices on `invoice_day` and marks unpaid records overdue
after `due_day + grace_days`.

---

## Advertising

Track ad campaigns across any platform (eBay, 1stDibs, Google Ads, social).
Record budget vs. spent, impressions, clicks, and conversions.
CTR and cost-per-click are calculated automatically on the summary row.

---

## Credit Card Fees

Record processing fees per transaction. The built-in fee calculator (on the
CC Fees page) lets you enter a sale amount and processor to see the fee instantly
before recording it. Processor rates (Stripe, Square, PayPal, Venmo) are all in
`config.pson` `[cc_fees]` — change them without rebuilding.

---

## Settlements

Nightly thread (enable with `settlements.enabled = true`) finds dealers with
unsettled sales older than `settlements.grace_days` and creates settlement
records. Admin sees all settlements; dealers see only their own.

Click **✅ Mark paid** when a settlement has been paid out.

---

## Users

**Add user** — Enter username, display name, email, password, and role.

| Role   | Access |
|--------|--------|
| admin  | Full access + user management + shutdown |
| dealer | Inventory, sales, business modules (own items only) |
| viewer | Read-only access to all pages |

**Edit user** — Change role, email, display name, or password.

**Revoke session** — Active sessions are shown below the user table. Click
**✕ Revoke** to immediately log out that session.

---

## Audit & Logs

Three tabs:
- **Audit trail** — every create/update/delete with before/after snapshot
- **Live log** — real-time WebSocket stream of the server log (admin only)
- **Sessions** — active sessions (also shown on Users page with revoke button)

---

## Languages and font size

Language selector and font size controls (A- / A / A+ / A++) are in the sidebar
footer. Six locales supported: English, English (UK), Français, Deutsch, Español,
日本語. Preferences are stored in browser localStorage.

---

## Backups

Automatic nightly backup runs at `app.backup_hour` (default 2am), writing to
`app.backup_dir` and keeping `app.backup_keep` copies.

Manual backup: copy `data/antique.db` at any time.
Restore: stop server, replace `data/antique.db`, restart.
