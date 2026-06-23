# Metis Antique Marketplace Plus -- Billing Reference

**Version 1.2.50**

Financial calculations, fee structures, and revenue reporting.

---

## Table of Contents

1. [Credit Card Fee Calculator](#1-credit-card-fee-calculator)
2. [Sale Recording and Net Proceeds](#2-sale-recording-and-net-proceeds)
3. [Consignment Settlements](#3-consignment-settlements)
4. [Booth Rent and Dealer Settlements](#4-booth-rent-and-dealer-settlements)
5. [P&L Report](#5-pl-report)
6. [Expense Categories](#6-expense-categories)

---

## 1. Credit Card Fee Calculator

### Endpoint

`GET /api/cc-fees/calculate?amount=<n>&processor=<name>`

### Fee Formula

```
fee = (amount * rate_pct / 100) + flat_fee
net = amount - fee
```

### Default Processors (configured in config.pson cc_fees block)

| Processor | Rate (%) | Flat Fee | Notes                            |
|-----------|----------|----------|----------------------------------|
| stripe    | 2.9      | $0.30    | Standard card-present rate       |
| square    | 2.6      | $0.10    | In-person tap/chip rate          |
| paypal    | 3.49     | $0.49    | Standard goods and services rate |
| venmo     | 1.9      | $0.10    | Business profile rate            |
| cash      | 0.0      | $0.00    | No fees                          |
| check     | 0.0      | $0.00    | No fees                          |

Add custom processors in `config.pson` using the `<name>_pct` / `<name>_flat`
pattern. All monetary values use the currency configured in `app.currency`.

---

## 2. Sale Recording and Net Proceeds

### Fields recorded per sale

| Field           | Description                                     |
|-----------------|-------------------------------------------------|
| `sale_price`    | Gross sale price received from buyer            |
| `platform_fee`  | Marketplace commission (eBay, 1stDibs, etc.)   |
| `shipping_cost` | Shipping amount charged to buyer                |
| `payment_method`| cash / check / card / venmo / paypal / other    |
| `buyer_name`    | Optional buyer name for records                 |
| `buyer_email`   | Optional buyer email for receipt delivery       |
| `notes`         | Free text sale notes                            |

### Net proceeds calculation

```
net_proceeds = sale_price - platform_fee
```

Shipping cost is recorded separately and not deducted from net proceeds by
default (shipping is typically a pass-through cost). Adjust in the P&L report
as needed for your accounting treatment.

### Margin calculation

```
margin = sale_price - cost_price
margin_pct = (margin / sale_price) * 100
```

`cost_price` comes from the item record (set on acquisition). Both values
appear in the sales summary and portfolio reports.

---

## 3. Consignment Settlements

### Consignor percentage

Set `consignor_pct` on the item record (0-100). When the item sells:

```
consignor_due = sale_price * (consignor_pct / 100)
dealer_net    = sale_price - consignor_due - platform_fee
```

### Consignor statement

`GET /api/consignors/:name/statement` returns a printable HTML statement
for all items held on consignment from a named consignor, including sold
items with calculated payout amounts.

---

## 4. Booth Rent and Dealer Settlements

### Booth rent invoicing

Rent invoices are generated on `booth_rent.invoice_day` each month.
The `monthly_rate` is set per vendor in the Vendors table.

### Discount stacking (all applicable discounts are combined)

| Discount Type   | Trigger                                        | Rate (default) |
|-----------------|------------------------------------------------|----------------|
| Early payment   | Paid before `early_pay_before_day` of month   | 5%             |
| Loyalty         | `loyalty_months` consecutive on-time payments | 10%            |
| Multi-booth     | Vendor holds `multi_booth_count`+ booths      | 7.5%           |

```
total_discount = early_pay_pct + loyalty_pct + multi_booth_pct
rent_due = monthly_rate * (1 - total_discount / 100)
```

### Late fee

Applied after `booth_rent.grace_days` past the `due_day`:

```
late_fee = monthly_rate * (late_fee_pct / 100)
```

### Dealer settlements

When `settlements.enabled = true`, a nightly thread generates settlement
records for each dealer at `settlements.grace_days` after each sale.

`GET /api/settlements` returns all settlements. Dealers see only their own.

---

## 5. P&L Report

`GET /api/pnl?from=<date>&to=<date>`

### Revenue sources included

- Gross sales revenue
- Less: platform fees
- Less: shipping costs (as applicable)
- Less: cost of goods sold (cost_price from item records)
- Less: operating expenses by category

### Expense categories (default)

Rent / Shipping / Insurance / Advertising / Repairs / Travel / Supplies /
Other

Add custom categories via `POST /api/expenses` with any category string.

---

## 6. Expense Categories

Expenses are free-form strings with no enforced category list. Default
categories suggested in the UI:

```
Rent
Shipping
Insurance
Advertising
Repairs
Travel
Supplies
Other
```

Category filters appear on the P&L report for per-category subtotals.
