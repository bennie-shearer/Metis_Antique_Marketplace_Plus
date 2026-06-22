# Metis Antique Marketplace Plus -- API Reference

**Version 1.2.48**

Base URL: `http://localhost:8480` (HTTP) or `https://localhost:8481` (HTTPS).

All `/api/` endpoints except `POST /api/auth/login` and `GET /api/config`
require: `Authorization: Bearer <token>`

Responses are JSON. Write endpoints (POST/PUT/DELETE) return HTTP 403 for
viewer role. User management endpoints require admin role.

---

## Config (1 endpoint)

### GET /api/config
No authorization required. Returns safe client-facing config values.
```json
{
  "currency": "USD",
  "currency_symbol": "$",
  "app_name": "Metis Antique Marketplace Plus",
  "version": "1.2.16"
}
```

---

## Authentication (8 endpoints)

### POST /api/auth/login
Body: `{"username":"admin","password":"Antique#2026"}`
Response: `{"token":"<hex>","user":"admin","role":"admin"}`
No Authorization header required.

### POST /api/auth/logout
Deletes current session token from database.
Response: `{"ok":true}`

### GET /api/auth/me
Response: `{"id":1,"username":"admin","role":"admin","created_at":"2026-06-20T..."}`

### POST /api/auth/change-password
Available to all roles. Body: `{"old_password":"...","new_password":"..."}`
Response: `{"ok":true}` or HTTP 401 if current password incorrect.

### GET /api/users
Admin only. Returns array of all users (passwords never returned).

### POST /api/users
Admin only. Body: `{"username":"...","password":"...","role":"dealer"}`
Role: admin | dealer | viewer. Response: `{"id":4}` (HTTP 201)

### PUT /api/users/:id
Admin only. Body: `{"role":"viewer"}` and/or `{"password":"newpass"}`
Guards: cannot remove last admin, cannot demote last admin.
Response: `{"ok":true}`

### DELETE /api/users/:id
Admin only. Guards: cannot delete self, cannot delete last admin.
Response: `{"ok":true}`

---

## Items / Inventory (6 endpoints)

### GET /api/items
Query params: `status`, `category`, `q` (search), `page`, `limit`
Response: array of item objects.
Fields: `id, title, category, era, maker, origin, material, description,
condition, cost_price, asking_price, status, provenance, photo_ref,
consignor_name, consignor_pct, source, created_at, updated_at`

### GET /api/items/stats
```json
{
  "inventory": 42,
  "listed": 8,
  "sold": 17,
  "inventory_value": 84500.00,
  "inventory_cost": 31200.00
}
```

### GET /api/items/:id
Returns single item. HTTP 404 if not found.

### POST /api/items
Required: `title`, `category`
Optional: `era, maker, origin, material, description, condition (default: Good),
cost_price (default: 0), asking_price (default: 0), status (default: inventory),
provenance, photo_ref, consignor_name, consignor_pct, source`
Response: `{"id":N}` (HTTP 201)

### PUT /api/items/:id
Same fields as POST. `updated_at` auto-set.
Response: `{"ok":true}`

### DELETE /api/items/:id
Hard delete. Response: `{"ok":true}`

---

## Listings (4 endpoints)

### GET /api/listings
Fields: `id, item_id, title, channel, list_price, list_type, auction_end,
views, watchers, status, created_at`

### POST /api/listings
Required: `item_id`, `channel`, `list_price`
Optional: `list_type` (fixed|auction), `auction_end`
Side effect: sets item status to 'listed'.
Response: `{"id":N}` (HTTP 201)

### DELETE /api/listings/:id
Soft delete (sets status to 'removed'). Response: `{"ok":true}`

### POST /api/listings/:id/view
Increments view counter. Response: `{"ok":true}`

---

## Sales (7 endpoints)

### GET /api/sales
Query params: `limit`, `offset`
Fields: `id, item_id, title, cost_price, sale_price, platform_fee,
shipping_cost, net_proceeds, margin_pct, payment_method, buyer_name,
buyer_email, buyer_phone, sale_date, channel, notes`

### GET /api/sales/summary
```json
{
  "month_revenue": 14320.00,
  "month_net": 13100.00,
  "month_count": 7,
  "rolling30_revenue": 18400.00,
  "avg_margin_pct": 38.4
}
```

### POST /api/sales
Required: `item_id`, `sale_price`
Optional: `listing_id, platform_fee, shipping_cost, payment_method,
buyer_name, buyer_email, buyer_phone, channel, sale_date, notes`
Side effect: sets item status to 'sold'.
Response: `{"id":N}` (HTTP 201)

### PUT /api/sales/:id
Same optional fields as POST. Response: `{"ok":true}`

### DELETE /api/sales/:id
Deletes sale and resets item status to 'inventory'. Response: `{"ok":true}`

### GET /api/sales/:id/invoice
Returns HTML invoice page for the sale. Opens in browser for printing.

### POST /api/sales/:id/email-receipt
Sends HTML receipt to the buyer email recorded on the sale.
Returns HTTP 503 if `email.enabled = false` in config.pson.
Returns HTTP 400 if the sale has no buyer email.
Response: `{"ok":true}` or `{"error":"..."}`

---

## Appraisals (3 endpoints)

### GET /api/appraisals
Fields: `id, item_id, title, appraiser, appraised_at, value_low, value_high,
condition, notes, status`
Status values: pending | in review | complete

### POST /api/appraisals
Required: `item_id`
Optional: `appraiser, value_low, value_high, condition, notes,
status (default: pending)`
Response: `{"id":N}` (HTTP 201)

### PUT /api/appraisals/:id
Fields: `appraiser, value_low, value_high, condition, notes, status`
`appraised_at` auto-updated. Response: `{"ok":true}`

---

## Market (3 endpoints)

### GET /api/market/categories
Revenue and profit by category across all sales.
Response: `[{"category":"Jewelry","sold_count":3,"revenue":15800,"avg_profit":2100}]`

### GET /api/market/trend
Monthly revenue for last 12 months, newest first.
Response: `[{"month":"2026-06","count":7,"revenue":14320}]`

### GET /api/market/top
Top 10 sold items by profit.
Response: `[{"title":"...","category":"...","sale_price":N,"cost_price":N,"profit":N}]`

---

## Photos (3 endpoints)

### GET /api/photos/:item_id
Returns all photos for an item.
Fields: `id, item_id, filename, caption, sort_order, created_at`

### POST /api/photos/:item_id
Multipart form upload. Field: `photo` (image file).
Optional: `caption`
Response: `{"id":N}` (HTTP 201)

### DELETE /api/photos/:id
Deletes photo record and file from disk. Response: `{"ok":true}`

---

## Comments / Notes (4 endpoints)

### GET /api/items/:id/notes
Returns dealer notes for an item ordered by created_at descending.

### POST /api/items/:id/notes
Body: `{"content":"..."}`
Response: `{"id":N}` (HTTP 201)

### GET /api/items/:id/inquiries
Returns buyer inquiries linked to an item.

### POST /api/inquiries/:id/replies
Body: `{"content":"..."}`
Response: `{"id":N}` (HTTP 201)

---

## Inquiries (3 endpoints)

### GET /api/inquiries
Returns all buyer inquiries with reply counts.
Fields: `id, item_id, item_title, buyer_name, buyer_email, buyer_phone,
subject, message, status, created_at, reply_count`

### GET /api/inquiries/summary
```json
{"open": 3, "closed": 12, "total": 15}
```

### PUT /api/inquiries/:id
Body: `{"status":"closed"}` or `{"status":"open"}`
Response: `{"ok":true}`

---

## Purchases and Expenses (7 endpoints)

### GET /api/purchases
Returns all purchase records.

### POST /api/purchases
Records a purchase (stock acquisition or expense).

### GET /api/purchases/:id
Single purchase record.

### PUT /api/purchases/:id
Update purchase record.

### DELETE /api/purchases/:id
Delete purchase record.

### GET /api/expenses/summary
```json
{"month_total": 1240.00, "ytd_total": 8430.00}
```

### GET /api/reports/pnl
Query params: `from` (date), `to` (date)
Returns profit and loss breakdown by category for the date range.

---

## Audit (2 endpoints)

### GET /api/audit
Returns audit trail entries ordered by created_at descending.
Fields: `id, user_id, username, action, entity, entity_id, detail, created_at`

### GET /api/sessions
Admin only. Returns all active sessions.
Fields: `id, user_id, username, created_at, expires_at`

---

## WebSocket

### WS /ws/logs
Requires admin role (validated via Authorization header during upgrade).
Streams server log lines as JSON objects in real time:
```json
{"ts":"2026-06-21 02:15:33.412","level":"system","msg":"Backup written: ..."}
```
On connect, the last 50 log lines are replayed as history, followed by
`{"type":"history_end"}`. Live lines are pushed as they are written.

---

## HTTP status codes

| Code | Meaning                                      |
|------|----------------------------------------------|
| 200  | OK                                           |
| 201  | Created (POST returning new id)              |
| 400  | Bad request (missing required field)         |
| 401  | Unauthorized (no valid session)              |
| 403  | Forbidden (wrong role)                       |
| 404  | Not found                                    |
| 409  | Conflict (duplicate username, last admin)    |
| 500  | Internal server error                        |
| 503  | Service unavailable (email not configured)   |
