# Metis Antique Marketplace Plus -- Database Reference

**Version 1.2.50**

Engine: SQLite 3.53.1 amalgamation (bundled in third_party/sqlite/).
Path: exe-relative `data/antique.db` (created automatically on first run).
Settings: WAL journal mode, foreign keys ON, SQLITE_THREADSAFE=1,
          SQLITE_ENABLE_FTS5, SQLITE_ENABLE_JSON1.

---

## Table: users

Stores all login accounts. Passwords are stored as bcrypt hashes.
Default rows inserted via INSERT OR IGNORE on every startup from config.pson.

| Column     | Type    | Constraints           | Notes                        |
|------------|---------|-----------------------|------------------------------|
| id         | INTEGER | PK AUTOINCREMENT      |                              |
| username   | TEXT    | NOT NULL UNIQUE       | case-sensitive               |
| pass_hash  | TEXT    | NOT NULL              | bcrypt hash       |
| role       | TEXT    | NOT NULL DEFAULT dealer | admin / dealer / viewer    |
| created_at | TEXT    | DEFAULT datetime('now') | UTC ISO timestamp          |

Roles:
- admin  -- full access plus user management (GET/POST/PUT/DELETE /api/users)
- dealer -- full read/write on all business data, cannot manage users
- viewer -- read-only; POST/PUT/DELETE blocked at server layer (HTTP 403)

---

## Table: sessions

One row per active login session. Expired rows are not cleaned up automatically
(they are simply ignored by the WHERE expires_at > datetime('now') guard).

| Column     | Type | Constraints      | Notes                              |
|------------|------|------------------|------------------------------------|
| token      | TEXT | PK               | random token |
| user_id    | INTEGER | NOT NULL FK   | references users(id)               |
| expires_at | TEXT | NOT NULL         | datetime('now','+N hours')         |
| created_at | TEXT | DEFAULT datetime('now') |                           |

Session lifetime is controlled by `auth.session_hours` in config.pson (default 8).
Sessions are explicitly deleted on logout (DELETE WHERE token=?).

---

## Table: items

Central inventory record. Every listing, sale, and appraisal references items.

| Column       | Type    | Constraints              | Notes                                |
|--------------|---------|--------------------------|--------------------------------------|
| id           | INTEGER | PK AUTOINCREMENT         |                                      |
| title        | TEXT    | NOT NULL                 | required on create                   |
| category     | TEXT    | NOT NULL                 | Furniture / Ceramics / Jewelry / Art & Prints / Silver & Metals / Clocks & Watches / Books & Maps / Other |
| era          | TEXT    |                          | e.g. "c. 1875", "Art Deco", "Victorian" |
| maker        | TEXT    |                          | artist, manufacturer, foundry        |
| origin       | TEXT    |                          | country or region                    |
| material     | TEXT    |                          | walnut, sterling silver, porcelain   |
| description  | TEXT    |                          | long-form item description           |
| condition    | TEXT    | NOT NULL DEFAULT Good    | Excellent / Very Good / Good / Fair / Poor |
| cost_price   | REAL    | NOT NULL DEFAULT 0       | acquisition cost                     |
| asking_price | REAL    | NOT NULL DEFAULT 0       | current asking / listing price       |
| status       | TEXT    | NOT NULL DEFAULT inventory | inventory / listed / sold          |
| provenance   | TEXT    |                          | ownership history, documentation     |
| photo_ref    | TEXT    |                          | filename or URL reference            |
| created_at   | TEXT    | NOT NULL DEFAULT datetime('now') |                          |
| updated_at   | TEXT    | NOT NULL DEFAULT datetime('now') | updated on every PUT       |

Status flow:
  inventory -> listed (when POST /api/listings)
  listed -> sold (when POST /api/sales)
  Any status can be set manually via PUT /api/items/:id.

---

## Table: listings

Tracks an item's presence on a selling channel. An item may have multiple
listing records (e.g. listed on eBay, then relisted on 1stDibs).

| Column     | Type    | Constraints           | Notes                              |
|------------|---------|-----------------------|------------------------------------|
| id         | INTEGER | PK AUTOINCREMENT      |                                    |
| item_id    | INTEGER | NOT NULL FK           | references items(id)               |
| channel    | TEXT    | NOT NULL              | 1stDibs / eBay / Etsy / Ruby Lane / Chairish / Own website / Auction house |
| list_price | REAL    | NOT NULL              |                                    |
| list_type  | TEXT    | NOT NULL DEFAULT fixed | fixed / auction                   |
| auction_end| TEXT    |                       | nullable ISO date                  |
| views      | INTEGER | NOT NULL DEFAULT 0    | incremented via /api/listings/:id/view |
| watchers   | INTEGER | NOT NULL DEFAULT 0    | updated manually                   |
| status     | TEXT    | NOT NULL DEFAULT active | active / removed                 |
| created_at | TEXT    | NOT NULL DEFAULT datetime('now') |                        |

DELETE /api/listings/:id sets status='removed' (soft delete).

---

## Table: sales

Records a completed sale. Recording a sale sets the item status to 'sold'.

| Column      | Type    | Constraints           | Notes                              |
|-------------|---------|-----------------------|------------------------------------|
| id          | INTEGER | PK AUTOINCREMENT      |                                    |
| item_id     | INTEGER | NOT NULL FK           | references items(id)               |
| listing_id  | INTEGER | FK nullable           | references listings(id)            |
| buyer_name  | TEXT    |                       |                                    |
| buyer_email | TEXT    |                       |                                    |
| sale_price  | REAL    | NOT NULL              |                                    |
| sale_date   | TEXT    | NOT NULL DEFAULT datetime('now') | UTC                   |
| channel     | TEXT    |                       | selling channel for this sale      |
| notes       | TEXT    |                       |                                    |

Margin calculation (computed in app_sales.cpp, not stored):
  margin_pct = (sale_price - items.cost_price) / items.cost_price * 100

---

## Table: appraisals

Tracks professional appraisal requests and results for inventory items.

| Column      | Type    | Constraints           | Notes                              |
|-------------|---------|-----------------------|------------------------------------|
| id          | INTEGER | PK AUTOINCREMENT      |                                    |
| item_id     | INTEGER | NOT NULL FK           | references items(id)               |
| appraiser   | TEXT    |                       | appraiser name or firm             |
| appraised_at| TEXT    | NOT NULL DEFAULT datetime('now') | updated on PUT         |
| value_low   | REAL    |                       | nullable lower bound               |
| value_high  | REAL    |                       | nullable upper bound               |
| condition   | TEXT    |                       | appraiser's condition assessment   |
| notes       | TEXT    |                       | full appraisal notes               |
| status      | TEXT    | NOT NULL DEFAULT pending | pending / in review / complete  |

---

## Schema creation

Tables are created via CREATE TABLE IF NOT EXISTS on every startup in
Db::create_schema() (db.cpp). Safe to run repeatedly. No migration system
is needed at this version; add new columns with ALTER TABLE in a future
migration if required.

## Backup

SQLite WAL mode means a simple file copy of data/antique.db is safe while
the server is running (WAL files are also consistent). For production use,
schedule a nightly copy of data/antique.db to a separate location.

## Direct inspection

```
sqlite3 data/antique.db
.tables
.schema items
SELECT title, asking_price, status FROM items ORDER BY updated_at DESC LIMIT 10;
.quit
```
