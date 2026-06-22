# Metis Antique Marketplace Plus -- Data Model

**Version 1.2.48**

SQLite 3.53.1. WAL journal mode. Foreign keys ON. All timestamps UTC ISO 8601.

---

## Table of Contents

1. [Entity Relationship Overview](#1-entity-relationship-overview)
2. [Table: users](#2-table-users)
3. [Table: sessions](#3-table-sessions)
4. [Table: items](#4-table-items)
5. [Table: listings](#5-table-listings)
6. [Table: sales](#6-table-sales)
7. [Table: appraisals](#7-table-appraisals)
8. [Table: purchases](#8-table-purchases)
9. [Table: expenses](#9-table-expenses)
10. [Table: comments](#10-table-comments)
11. [Table: photos](#11-table-photos)
12. [Table: audit_log](#12-table-audit_log)
13. [Table: booths](#13-table-booths)
14. [Table: booth_invoices](#14-table-booth_invoices)
15. [Table: settlements](#15-table-settlements)
16. [Table: consignors](#16-table-consignors)
17. [Table: rentals](#17-table-rentals)
18. [Table: advertising](#18-table-advertising)
19. [Status Flows](#19-status-flows)

---

## 1. Entity Relationship Overview

```
users --(1:N)--> sessions
users --(1:N)--> items        (owner_id)
items --(1:N)--> listings
items --(1:N)--> sales
items --(1:N)--> appraisals
items --(1:N)--> purchases
items --(1:N)--> photos
items --(1:N)--> comments
items --(1:N)--> audit_log    (entity_type='item')
users --(1:N)--> booths
booths --(1:N)--> booth_invoices
users --(1:N)--> settlements
```

---

## 2. Table: users

Login accounts. Passwords stored as bcrypt hashes.

| Column         | Type    | Constraints              | Notes                                |
|----------------|---------|--------------------------|--------------------------------------|
| id             | INTEGER | PK AUTOINCREMENT         |                                      |
| username       | TEXT    | NOT NULL UNIQUE          | Case-sensitive                       |
| pass_hash      | TEXT    | NOT NULL                 | bcrypt hash (cost from config.pson)  |
| role           | TEXT    | NOT NULL DEFAULT dealer  | admin / dealer / viewer              |
| display_name   | TEXT    |                          | Optional display name for UI         |
| email          | TEXT    |                          | Optional email for notifications     |
| created_at     | TEXT    | DEFAULT datetime('now')  | UTC ISO timestamp                    |

Default rows seeded from `auth.*` in config.pson on every startup (INSERT OR IGNORE).

---

## 3. Table: sessions

Active login tokens. Expired rows are ignored by the `expires_at > datetime('now')` guard.

| Column     | Type    | Constraints             | Notes                                  |
|------------|---------|-------------------------|----------------------------------------|
| token      | TEXT    | PK                      | bcrypt-derived hex token               |
| user_id    | INTEGER | NOT NULL FK -> users.id |                                        |
| expires_at | TEXT    | NOT NULL                | datetime('now', '+N hours')            |
| created_at | TEXT    | DEFAULT datetime('now') |                                        |

Session lifetime: `auth.session_hours` in config.pson (default 8).

---

## 4. Table: items

Central inventory record. All other business records reference items.

| Column        | Type    | Constraints                    | Notes                                       |
|---------------|---------|--------------------------------|---------------------------------------------|
| id            | INTEGER | PK AUTOINCREMENT               |                                             |
| owner_id      | INTEGER | NOT NULL FK -> users.id        | Dealer who owns this item                   |
| title         | TEXT    | NOT NULL                       | Required on create                          |
| category      | TEXT    | NOT NULL                       | Furniture / Ceramics / Jewelry / Art & Prints / Silver & Metals / Clocks & Watches / Books & Maps / Other |
| era           | TEXT    |                                | e.g. "c. 1875", "Art Deco", "Victorian"     |
| maker         | TEXT    |                                | Artist, manufacturer, foundry               |
| origin        | TEXT    |                                | Country or region                           |
| material      | TEXT    |                                | Walnut, sterling silver, porcelain          |
| description   | TEXT    |                                | Long-form item description                  |
| condition     | TEXT    | NOT NULL DEFAULT Good          | Excellent / Very Good / Good / Fair / Poor  |
| cost_price    | REAL    | NOT NULL DEFAULT 0             | Acquisition cost                            |
| asking_price  | REAL    | NOT NULL DEFAULT 0             | Current asking price                        |
| status        | TEXT    | NOT NULL DEFAULT inventory     | inventory / listed / sold                   |
| provenance    | TEXT    |                                | Ownership history, documentation            |
| photo_ref     | TEXT    |                                | Primary photo filename                      |
| sku           | TEXT    | UNIQUE                         | Auto-generated: PREFIX-YYYY-NNNNN           |
| source        | TEXT    |                                | Acquisition source (estate, auction, etc.)  |
| consignor_name| TEXT    |                                | Consignor name if item held on consignment  |
| consignor_pct | REAL    | NOT NULL DEFAULT 0             | Consignor split percentage (0-100)          |
| created_at    | TEXT    | NOT NULL DEFAULT datetime('now')|                                            |
| updated_at    | TEXT    | NOT NULL DEFAULT datetime('now')| Updated on every PUT                       |

---

## 5. Table: listings

Item's presence on a selling channel. Multiple listings per item are allowed.

| Column      | Type    | Constraints               | Notes                                       |
|-------------|---------|---------------------------|---------------------------------------------|
| id          | INTEGER | PK AUTOINCREMENT          |                                             |
| item_id     | INTEGER | NOT NULL FK -> items.id   |                                             |
| channel     | TEXT    | NOT NULL                  | 1stDibs / eBay / Etsy / Ruby Lane / Chairish / Own website / Auction house |
| list_price  | REAL    | NOT NULL                  |                                             |
| list_type   | TEXT    | NOT NULL DEFAULT fixed    | fixed / auction                             |
| auction_end | TEXT    |                           | Nullable ISO date for auction listings      |
| views       | INTEGER | NOT NULL DEFAULT 0        | Incremented by POST /api/listings/:id/view  |
| watchers    | INTEGER | NOT NULL DEFAULT 0        | Updated manually                            |
| status      | TEXT    | NOT NULL DEFAULT active   | active / removed                            |
| created_at  | TEXT    | NOT NULL DEFAULT datetime('now') |                                      |

DELETE /api/listings/:id performs a soft delete (sets status='removed').

---

## 6. Table: sales

Completed sale record. Recording a sale sets item status to 'sold'.

| Column          | Type    | Constraints               | Notes                                    |
|-----------------|---------|---------------------------|------------------------------------------|
| id              | INTEGER | PK AUTOINCREMENT          |                                          |
| item_id         | INTEGER | NOT NULL FK -> items.id   |                                          |
| listing_id      | INTEGER | FK -> listings.id         | Nullable                                 |
| owner_id        | INTEGER | FK -> users.id            | Dealer who recorded the sale             |
| buyer_name      | TEXT    |                           |                                          |
| buyer_email     | TEXT    |                           |                                          |
| sale_price      | REAL    | NOT NULL                  | Gross price received                     |
| platform_fee    | REAL    | NOT NULL DEFAULT 0        | Marketplace commission                   |
| shipping_cost   | REAL    | NOT NULL DEFAULT 0        | Shipping charged to buyer                |
| payment_method  | TEXT    |                           | cash / check / card / venmo / paypal     |
| sale_date       | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |
| channel         | TEXT    |                           | Selling channel for this sale            |
| notes           | TEXT    |                           |                                          |
| created_at      | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 7. Table: appraisals

Professional appraisal requests and results.

| Column       | Type    | Constraints               | Notes                                    |
|--------------|---------|---------------------------|------------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT          |                                          |
| item_id      | INTEGER | NOT NULL FK -> items.id   |                                          |
| appraiser    | TEXT    |                           | Appraiser name or firm                   |
| appraised_at | TEXT    | NOT NULL DEFAULT datetime('now') | Updated on PUT                   |
| value_low    | REAL    |                           | Nullable lower bound of valuation range  |
| value_high   | REAL    |                           | Nullable upper bound of valuation range  |
| condition    | TEXT    |                           | Appraiser's condition assessment         |
| notes        | TEXT    |                           | Full appraisal notes                     |
| status       | TEXT    | NOT NULL DEFAULT pending  | pending / in review / complete           |

---

## 8. Table: purchases

Acquisition cost records linked to inventory items.

| Column       | Type    | Constraints               | Notes                                    |
|--------------|---------|---------------------------|------------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT          |                                          |
| item_id      | INTEGER | FK -> items.id            | Nullable (expense not tied to item)      |
| purchase_date| TEXT    | NOT NULL DEFAULT datetime('now') |                                  |
| amount       | REAL    | NOT NULL                  |                                          |
| source       | TEXT    |                           | Acquisition source (estate, auction, dealer) |
| notes        | TEXT    |                           |                                          |
| created_at   | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 9. Table: expenses

Operating expenses by category for P&L reporting.

| Column        | Type    | Constraints               | Notes                                    |
|---------------|---------|---------------------------|------------------------------------------|
| id            | INTEGER | PK AUTOINCREMENT          |                                          |
| expense_date  | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |
| category      | TEXT    | NOT NULL                  | Rent / Shipping / Insurance / etc.       |
| amount        | REAL    | NOT NULL                  |                                          |
| description   | TEXT    |                           |                                          |
| created_at    | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 10. Table: comments

Dealer internal notes and buyer inquiry threads per item.

| Column       | Type    | Constraints               | Notes                                    |
|--------------|---------|---------------------------|------------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT          |                                          |
| item_id      | INTEGER | NOT NULL FK -> items.id   |                                          |
| user_id      | INTEGER | FK -> users.id            |                                          |
| parent_id    | INTEGER | FK -> comments.id         | Nullable (for threaded replies)          |
| type         | TEXT    | NOT NULL DEFAULT note     | note / inquiry / reply                   |
| content      | TEXT    | NOT NULL                  |                                          |
| buyer_name   | TEXT    |                           | For inquiry type (external buyer)        |
| buyer_email  | TEXT    |                           |                                          |
| created_at   | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 11. Table: photos

Multi-photo gallery per item. Photos stored in `photos/` directory (exe-relative).

| Column      | Type    | Constraints               | Notes                                    |
|-------------|---------|---------------------------|------------------------------------------|
| id          | INTEGER | PK AUTOINCREMENT          |                                          |
| item_id     | INTEGER | NOT NULL FK -> items.id   |                                          |
| filename    | TEXT    | NOT NULL                  | Stored in photos/ directory              |
| caption     | TEXT    |                           |                                          |
| sort_order  | INTEGER | NOT NULL DEFAULT 0        | Display order (lower = first)            |
| created_at  | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 12. Table: audit_log

Full change trail for all write operations.

| Column      | Type    | Constraints               | Notes                                    |
|-------------|---------|---------------------------|------------------------------------------|
| id          | INTEGER | PK AUTOINCREMENT          |                                          |
| user_id     | INTEGER | FK -> users.id            |                                          |
| username    | TEXT    |                           | Denormalized for log readability         |
| role        | TEXT    |                           | Role at time of action                   |
| action      | TEXT    | NOT NULL                  | create / update / delete / login / etc.  |
| entity_type | TEXT    |                           | item / listing / sale / appraisal / user |
| entity_id   | TEXT    |                           | ID of affected record                    |
| detail      | TEXT    |                           | JSON or free text description of change  |
| ip_address  | TEXT    |                           | Client IP at time of action              |
| created_at  | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 13. Table: booths

Dealer booth records for mall/co-op operators.

| Column        | Type    | Constraints               | Notes                                    |
|---------------|---------|---------------------------|------------------------------------------|
| id            | INTEGER | PK AUTOINCREMENT          |                                          |
| user_id       | INTEGER | NOT NULL FK -> users.id   | Dealer who rents this booth              |
| booth_number  | TEXT    | NOT NULL                  |                                          |
| monthly_rate  | REAL    | NOT NULL                  |                                          |
| active        | INTEGER | NOT NULL DEFAULT 1        | 1 = active, 0 = vacated                  |
| start_date    | TEXT    |                           |                                          |
| notes         | TEXT    |                           |                                          |
| created_at    | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 14. Table: booth_invoices

Monthly rent invoices generated by automated collection.

| Column       | Type    | Constraints               | Notes                                    |
|--------------|---------|---------------------------|------------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT          |                                          |
| booth_id     | INTEGER | NOT NULL FK -> booths.id  |                                          |
| user_id      | INTEGER | NOT NULL FK -> users.id   |                                          |
| period_month | TEXT    | NOT NULL                  | YYYY-MM                                  |
| base_amount  | REAL    | NOT NULL                  | Monthly rate before discounts            |
| discount_pct | REAL    | NOT NULL DEFAULT 0        | Combined discount percentage             |
| late_fee     | REAL    | NOT NULL DEFAULT 0        |                                          |
| total_due    | REAL    | NOT NULL                  |                                          |
| status       | TEXT    | NOT NULL DEFAULT unpaid   | unpaid / paid / overdue / waived         |
| paid_at      | TEXT    |                           |                                          |
| created_at   | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 15. Table: settlements

Per-dealer revenue settlements after sales.

| Column       | Type    | Constraints               | Notes                                    |
|--------------|---------|---------------------------|------------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT          |                                          |
| owner_id     | INTEGER | NOT NULL FK -> users.id   |                                          |
| period_start | TEXT    | NOT NULL                  |                                          |
| period_end   | TEXT    | NOT NULL                  |                                          |
| total_sales  | REAL    | NOT NULL DEFAULT 0        |                                          |
| total_fees   | REAL    | NOT NULL DEFAULT 0        |                                          |
| net_due      | REAL    | NOT NULL DEFAULT 0        |                                          |
| status       | TEXT    | NOT NULL DEFAULT pending  | pending / sent / paid                    |
| sent_at      | TEXT    |                           |                                          |
| notes        | TEXT    |                           |                                          |
| created_at   | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 16. Table: consignors

Consignor contact records for items held on consignment.

| Column     | Type    | Constraints               | Notes                                    |
|------------|---------|---------------------------|------------------------------------------|
| id         | INTEGER | PK AUTOINCREMENT          |                                          |
| name       | TEXT    | NOT NULL UNIQUE           |                                          |
| email      | TEXT    |                           |                                          |
| phone      | TEXT    |                           |                                          |
| address    | TEXT    |                           |                                          |
| notes      | TEXT    |                           |                                          |
| created_at | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 17. Table: rentals

Furniture or equipment rental tracking.

| Column       | Type    | Constraints               | Notes                                    |
|--------------|---------|---------------------------|------------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT          |                                          |
| item_id      | INTEGER | NOT NULL FK -> items.id   |                                          |
| renter_name  | TEXT    | NOT NULL                  |                                          |
| renter_email | TEXT    |                           |                                          |
| rental_start | TEXT    | NOT NULL                  |                                          |
| rental_end   | TEXT    |                           |                                          |
| daily_rate   | REAL    |                           |                                          |
| deposit      | REAL    |                           |                                          |
| status       | TEXT    | NOT NULL DEFAULT active   | active / returned / overdue              |
| notes        | TEXT    |                           |                                          |
| created_at   | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 18. Table: advertising

Advertising campaigns linked to items or general promotions.

| Column       | Type    | Constraints               | Notes                                    |
|--------------|---------|---------------------------|------------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT          |                                          |
| item_id      | INTEGER | FK -> items.id            | Nullable (general campaign)              |
| platform     | TEXT    | NOT NULL                  | Google / Facebook / Instagram / Print    |
| campaign_name| TEXT    | NOT NULL                  |                                          |
| start_date   | TEXT    |                           |                                          |
| end_date     | TEXT    |                           |                                          |
| budget       | REAL    |                           |                                          |
| spend        | REAL    | NOT NULL DEFAULT 0        |                                          |
| impressions  | INTEGER | NOT NULL DEFAULT 0        |                                          |
| clicks       | INTEGER | NOT NULL DEFAULT 0        |                                          |
| status       | TEXT    | NOT NULL DEFAULT active   | active / paused / complete               |
| notes        | TEXT    |                           |                                          |
| created_at   | TEXT    | NOT NULL DEFAULT datetime('now') |                                  |

---

## 19. Status Flows

### Item status

```
inventory  -->  listed   (POST /api/listings)
listed     -->  sold     (POST /api/sales)
any        -->  any      (PUT /api/items/:id with status field)
```

### Appraisal status

```
pending  -->  in review  -->  complete
```

### Settlement status

```
pending  -->  sent  -->  paid
```

### Booth invoice status

```
unpaid  -->  paid
unpaid  -->  overdue    (after grace period)
any     -->  waived     (admin only)
```

### Listing status

```
active  -->  removed   (DELETE /api/listings/:id, soft delete)
```
