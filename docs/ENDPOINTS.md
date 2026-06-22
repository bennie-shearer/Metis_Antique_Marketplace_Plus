# Metis Antique Marketplace Plus -- Endpoint Quick Reference

**Version 1.2.48** | 121 endpoints across 16 modules

Authorization: `Authorization: Bearer <token>` on all endpoints except
POST /api/auth/login and GET /api/config.
Viewer role: GET endpoints only. POST/PUT/DELETE return HTTP 403.
Admin role required: all /api/users endpoints and GET /api/sessions.

---

## Config (1 endpoint)

| Method | Path                   | Auth   | Role  | Description                        |
|--------|------------------------|--------|-------|------------------------------------|
| GET    | /api/config            | None   | Any   | Safe client config (currency, etc) |
| POST   | /api/admin/shutdown    | Bearer | Admin | Graceful server shutdown           |

---

## Auth module (8 endpoints)

| Method | Path                      | Auth   | Role  | Description               |
|--------|---------------------------|--------|-------|---------------------------|
| POST   | /api/auth/login           | None   | Any   | Returns token + role      |
| POST   | /api/auth/logout          | Bearer | Any   | Deletes session token     |
| GET    | /api/auth/me              | Bearer | Any   | Returns id, username, role|
| POST   | /api/auth/change-password | Bearer | Any   | Change own password       |
| GET    | /api/users                | Bearer | Admin | List all users            |
| POST   | /api/users                | Bearer | Admin | Create user               |
| PUT    | /api/users/:id            | Bearer | Admin | Update role / password    |
| DELETE | /api/users/:id            | Bearer | Admin | Delete user               |

---

## Items module (6 endpoints)

| Method | Path             | Auth   | Role         | Description                   |
|--------|------------------|--------|--------------|-------------------------------|
| GET    | /api/items       | Bearer | Any          | All inventory items           |
| GET    | /api/items/stats | Bearer | Any          | Counts + value + cost         |
| GET    | /api/items/:id   | Bearer | Any          | Single item                   |
| POST   | /api/items       | Bearer | Admin/Dealer | Create item                   |
| PUT    | /api/items/:id   | Bearer | Admin/Dealer | Update item                   |
| DELETE | /api/items/:id   | Bearer | Admin/Dealer | Delete item (hard)            |

---

## Listings module (4 endpoints)

| Method | Path                   | Auth   | Role         | Description                   |
|--------|------------------------|--------|--------------|-------------------------------|
| GET    | /api/listings          | Bearer | Any          | All listings with item title  |
| POST   | /api/listings          | Bearer | Admin/Dealer | Create; sets item='listed'    |
| DELETE | /api/listings/:id      | Bearer | Admin/Dealer | Soft delete (status=removed)  |
| POST   | /api/listings/:id/view | Bearer | Any          | Increment view counter        |

---

## Sales module (7 endpoints)

| Method | Path                           | Auth   | Role         | Description                   |
|--------|--------------------------------|--------|--------------|-------------------------------|
| GET    | /api/sales                     | Bearer | Any          | All sales with margin_pct     |
| GET    | /api/sales/summary             | Bearer | Any          | Monthly + rolling 30d revenue |
| POST   | /api/sales                     | Bearer | Admin/Dealer | Record sale; sets item='sold' |
| PUT    | /api/sales/:id                 | Bearer | Admin/Dealer | Update sale record            |
| DELETE | /api/sales/:id                 | Bearer | Admin/Dealer | Delete sale; resets item      |
| GET    | /api/sales/:id/invoice         | Bearer | Any          | HTML invoice (for printing)   |
| POST   | /api/sales/:id/email-receipt   | Bearer | Admin/Dealer | Email receipt to buyer        |

---

## Appraisals module (3 endpoints)

| Method | Path                | Auth   | Role         | Description                   |
|--------|---------------------|--------|--------------|-------------------------------|
| GET    | /api/appraisals     | Bearer | Any          | All appraisals with item title|
| POST   | /api/appraisals     | Bearer | Admin/Dealer | Submit appraisal request      |
| PUT    | /api/appraisals/:id | Bearer | Admin/Dealer | Update values / status        |

---

## Market module (3 endpoints)

| Method | Path                   | Auth   | Role | Description                  |
|--------|------------------------|--------|------|------------------------------|
| GET    | /api/market/categories | Bearer | Any  | Revenue by category          |
| GET    | /api/market/trend      | Bearer | Any  | Monthly revenue last 12mo    |
| GET    | /api/market/top        | Bearer | Any  | Top 10 items by profit       |

---

## Photos module (3 endpoints)

| Method | Path                  | Auth   | Role         | Description                   |
|--------|-----------------------|--------|--------------|-------------------------------|
| GET    | /api/photos/:item_id  | Bearer | Any          | All photos for item           |
| POST   | /api/photos/:item_id  | Bearer | Admin/Dealer | Upload photo (multipart)      |
| DELETE | /api/photos/:id       | Bearer | Admin/Dealer | Delete photo + file           |

---

## Inquiries module (5 endpoints)

| Method | Path                       | Auth   | Role         | Description                  |
|--------|----------------------------|--------|--------------|------------------------------|
| GET    | /api/inquiries             | Bearer | Any          | All inquiries + reply counts |
| GET    | /api/inquiries/summary     | Bearer | Any          | open/closed/total counts     |
| PUT    | /api/inquiries/:id         | Bearer | Admin/Dealer | Update status                |
| GET    | /api/items/:id/inquiries   | Bearer | Any          | Inquiries for one item       |
| POST   | /api/inquiries/:id/replies | Bearer | Admin/Dealer | Post reply                   |

---

## Purchases / Expenses module (6 endpoints)

| Method | Path                   | Auth   | Role         | Description                  |
|--------|------------------------|--------|--------------|------------------------------|
| GET    | /api/purchases         | Bearer | Any          | All purchase records         |
| POST   | /api/purchases         | Bearer | Admin/Dealer | Record purchase              |
| GET    | /api/purchases/:id     | Bearer | Any          | Single purchase              |
| PUT    | /api/purchases/:id     | Bearer | Admin/Dealer | Update purchase              |
| DELETE | /api/purchases/:id     | Bearer | Admin/Dealer | Delete purchase              |
| GET    | /api/expenses/summary  | Bearer | Any          | Month + YTD expense totals   |
| GET    | /api/reports/pnl       | Bearer | Any          | P&L by category + date range |

---

## Audit module (2 endpoints)

| Method | Path          | Auth   | Role  | Description                   |
|--------|---------------|--------|-------|-------------------------------|
| GET    | /api/audit    | Bearer | Any   | Audit trail entries           |
| GET    | /api/sessions | Bearer | Admin | Active sessions               |

---

## WebSocket (1 endpoint)

| Protocol | Path      | Auth   | Role  | Description                     |
|----------|-----------|--------|-------|---------------------------------|
| WS       | /ws/logs  | Bearer | Admin | Real-time log stream (RFC 6455) |

---

## HTTP status codes

| Code | Meaning                                      |
|------|----------------------------------------------|
| 200  | OK                                           |
| 201  | Created                                      |
| 400  | Bad request                                  |
| 401  | Unauthorized                                 |
| 403  | Forbidden (wrong role)                       |
| 404  | Not found                                    |
| 409  | Conflict                                     |
| 500  | Internal server error                        |
| 503  | Service unavailable (email disabled)         |
