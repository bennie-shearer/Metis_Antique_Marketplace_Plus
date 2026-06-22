# Metis Antique Marketplace Plus -- Configuration Reference

**Version 1.2.48**

All operational parameters are in `config.pson`. No hardcoded operational
values exist in source. Changes take effect on server restart unless noted.

---

## Table of Contents

1. [server](#server)
2. [auth](#auth)
3. [database](#database)
4. [app](#app)
5. [email](#email)
6. [cc_fees](#cc_fees)
7. [settlements](#settlements)
8. [sku](#sku)
9. [booth_rent](#booth_rent)
10. [pos](#pos)
11. [sync](#sync)
12. [sync_hosts](#sync_hosts)
13. [compute](#compute)

---

## server

```pson
server {
    host       = "0.0.0.0"
    port       = 8480
    tls_port   = 8481
    cert_file  = "certs/server.crt"
    key_file   = "certs/server.key"
    web_root   = "web"
    log_dir    = "logs"
    log_max_mb = 10
}
```

| Key          | Default             | Description                                       |
|--------------|---------------------|---------------------------------------------------|
| `host`       | `0.0.0.0`           | Bind address. Use `127.0.0.1` for localhost-only. |
| `port`       | `8480`              | HTTP listener port.                               |
| `tls_port`   | `8481`              | HTTPS/TLS listener port. Set to 0 to disable.    |
| `cert_file`  | `certs/server.crt`  | TLS certificate (PEM). Exe-relative path.         |
| `key_file`   | `certs/server.key`  | TLS private key (PEM). Exe-relative path.         |
| `web_root`   | `web`               | Static file directory. Exe-relative path.         |
| `log_dir`    | `logs`              | Log output directory. Exe-relative path.          |
| `log_max_mb` | `10`                | Log rotation size threshold in megabytes.         |

---

## auth

```pson
auth {
    enabled                  = true
    session_hours            = 8
    bcrypt_cost              = 12
    rate_limit_max_attempts  = 5
    rate_limit_window_sec    = 60

    admin_user    = "admin"
    admin_pass    = "Antique#2026"

    dealer_user   = "dealer"
    dealer_pass   = "Dealer#2026"

    viewer_user   = "viewer"
    viewer_pass   = "Viewer#2026"
}
```

| Key                        | Default        | Description                                           |
|----------------------------|----------------|-------------------------------------------------------|
| `enabled`                  | `true`         | Enable authentication. Always set true in production. |
| `session_hours`            | `8`            | Session lifetime before automatic expiry.             |
| `bcrypt_cost`              | `12`           | bcrypt work factor. Higher = slower but more secure.  |
| `rate_limit_max_attempts`  | `5`            | Failed login attempts before temporary lockout.       |
| `rate_limit_window_sec`    | `60`           | Window in seconds for rate limit counting.            |
| `admin_user` / `admin_pass`| `admin` / ...  | Default admin credentials. Change before deployment.  |
| `dealer_user` / `dealer_pass` | `dealer` / ... | Default dealer credentials.                        |
| `viewer_user` / `viewer_pass` | `viewer` / ... | Default viewer credentials.                        |

**Security note:** Change all default passwords immediately after first login.
Navigate to Users (admin) and use Change Password for your own account.

---

## database

```pson
database {
    path = "data/antique.db"
}
```

| Key    | Default          | Description                                          |
|--------|------------------|------------------------------------------------------|
| `path` | `data/antique.db`| SQLite database path. Exe-relative. Auto-created.   |

---

## app

```pson
app {
    name              = "Metis Antique Marketplace Plus"
    version           = "1.2.48"
    currency          = "USD"
    currency_symbol   = "$"
    currency_code     = "USD"
    currency_locale   = "en-US"
    items_per_page    = 20
    photos_dir        = "photos"
    backup_dir        = "backups"
    backup_keep       = 7
    backup_hour       = 2
}
```

| Key               | Default                          | Description                                         |
|-------------------|----------------------------------|-----------------------------------------------------|
| `name`            | `Metis Antique Marketplace Plus` | Application name shown in UI title and sidebar.     |
| `version`         | `1.2.48`                         | Version returned by `/api/config`.                  |
| `currency`        | `USD`                            | ISO 4217 currency code.                             |
| `currency_symbol` | `$`                              | Currency symbol for display.                        |
| `currency_code`   | `USD`                            | BCP 47 currency code for `Intl.NumberFormat`.       |
| `currency_locale` | `en-US`                          | BCP 47 locale for number formatting.                |
| `items_per_page`  | `20`                             | Default page size for inventory lists.              |
| `photos_dir`      | `photos`                         | Photo upload directory. Exe-relative.               |
| `backup_dir`      | `backups`                        | Backup output directory. Exe-relative.              |
| `backup_keep`     | `7`                              | Number of daily backups to retain.                  |
| `backup_hour`     | `2`                              | Hour (0-23) to run the nightly backup.              |

**Currency examples:**

| Region              | currency | currency_symbol | currency_code | currency_locale |
|---------------------|----------|-----------------|---------------|-----------------|
| US Dollar           | USD      | $               | USD           | en-US           |
| Euro (Germany)      | EUR      | EUR             | EUR           | de-DE           |
| British Pound       | GBP      | GBP             | GBP           | en-GB           |
| Canadian Dollar     | CAD      | $               | CAD           | en-CA           |
| Japanese Yen        | JPY      | JPY             | JPY           | ja-JP           |

---

## email

```pson
email {
    enabled      = false
    smtp_host    = "smtp.gmail.com"
    smtp_port    = 587
    smtp_user    = ""
    smtp_pass    = ""
    from_address = ""
    from_name    = "Metis Antique Marketplace"
}
```

| Key            | Default                        | Description                                       |
|----------------|--------------------------------|---------------------------------------------------|
| `enabled`      | `false`                        | Set `true` to enable SMTP email (sale receipts).  |
| `smtp_host`    | `smtp.gmail.com`               | SMTP server hostname.                             |
| `smtp_port`    | `587`                          | SMTP port. 587 = STARTTLS. 465 = implicit TLS.   |
| `smtp_user`    | (empty)                        | SMTP authentication username.                     |
| `smtp_pass`    | (empty)                        | SMTP authentication password / app password.      |
| `from_address` | (empty)                        | Sender email address.                             |
| `from_name`    | `Metis Antique Marketplace`    | Sender display name.                              |

---

## cc_fees

Credit card fee defaults used by `GET /api/cc-fees/calculate`. Add custom
processors by following the `<name>_pct` / `<name>_flat` pattern.

```pson
cc_fees {
    stripe_pct   = 2.9
    stripe_flat  = 0.30
    square_pct   = 2.6
    square_flat  = 0.10
    paypal_pct   = 3.49
    paypal_flat  = 0.49
    venmo_pct    = 1.9
    venmo_flat   = 0.10
    cash_pct     = 0.0
    cash_flat    = 0.0
    check_pct    = 0.0
    check_flat   = 0.0
}
```

---

## settlements

```pson
settlements {
    enabled    = false
    grace_days = 7
    check_hour = 3
}
```

| Key          | Default | Description                                              |
|--------------|---------|----------------------------------------------------------|
| `enabled`    | `false` | Enable nightly settlement generation for dealers.        |
| `grace_days` | `7`     | Days after sale before a settlement is generated.        |
| `check_hour` | `3`     | Hour (0-23) for the nightly settlement check thread.    |

---

## sku

```pson
sku {
    prefix = "AMP"
}
```

| Key      | Default | Description                                                       |
|----------|---------|-------------------------------------------------------------------|
| `prefix` | `AMP`   | Prefix for auto-generated SKUs: `PREFIX-YYYY-NNNNN` (AMP-2026-00042). |

---

## booth_rent

```pson
booth_rent {
    collection_enabled      = false
    collection_hour         = 8
    invoice_day             = 1
    due_day                 = 5
    grace_days              = 5
    early_pay_discount_pct  = 5.0
    early_pay_before_day    = 3
    loyalty_discount_pct    = 10.0
    loyalty_months          = 12
    multi_booth_discount_pct = 7.5
    multi_booth_count       = 3
    late_fee_pct            = 5.0
    send_invoice_email      = false
    send_reminder_email     = false
    send_overdue_email      = false
}
```

| Key                       | Default | Description                                                |
|---------------------------|---------|------------------------------------------------------------|
| `collection_enabled`      | `false` | Enable automated monthly rent invoice generation.          |
| `collection_hour`         | `8`     | Hour to run daily collection check.                        |
| `invoice_day`             | `1`     | Day of month to generate rent invoices.                    |
| `due_day`                 | `5`     | Day of month rent is due.                                  |
| `grace_days`              | `5`     | Days after due before late fee applies.                    |
| `early_pay_discount_pct`  | `5.0`   | Percentage discount for early payment.                     |
| `early_pay_before_day`    | `3`     | Payment before this day of month qualifies for discount.   |
| `loyalty_discount_pct`    | `10.0`  | Discount after consecutive loyalty_months payments.        |
| `loyalty_months`          | `12`    | Months of consecutive payment to earn loyalty discount.    |
| `multi_booth_discount_pct`| `7.5`   | Discount for vendors with multiple booths.                 |
| `multi_booth_count`       | `3`     | Minimum booths to qualify for multi-booth discount.        |
| `late_fee_pct`            | `5.0`   | Late fee as percentage of monthly rent rate.               |
| `send_invoice_email`      | `false` | Email invoice to dealer on generation.                     |
| `send_reminder_email`     | `false` | Email reminder 2 days before due date.                     |
| `send_overdue_email`      | `false` | Email overdue notice after grace period.                   |

---

## pos

```pson
pos {
    enabled              = false
    receipt_printer_ip   = "192.168.1.100"
    receipt_printer_port = 9100
    receipt_printer_type = "network"
    cash_drawer_enabled  = false
    receipt_header       = "Metis Antique Marketplace"
    receipt_footer       = "Thank you for your purchase!"
    receipt_show_logo    = false
    barcode_scanner_enabled = true
}
```

| Key                      | Default                    | Description                                         |
|--------------------------|----------------------------|-----------------------------------------------------|
| `enabled`                | `false`                    | Enable POS hardware integration.                    |
| `receipt_printer_ip`     | `192.168.1.100`            | ESC/POS network receipt printer IP address.         |
| `receipt_printer_port`   | `9100`                     | ESC/POS standard TCP port.                          |
| `receipt_printer_type`   | `network`                  | `network` / `usb_serial` / `none`                  |
| `cash_drawer_enabled`    | `false`                    | Trigger cash drawer via printer pulse on sale.      |
| `receipt_header`         | `Metis Antique Marketplace`| Text printed at top of receipt.                     |
| `receipt_footer`         | `Thank you...`             | Text printed at bottom of receipt.                  |
| `receipt_show_logo`      | `false`                    | Print logo bitmap on receipt.                       |
| `barcode_scanner_enabled`| `true`                     | Enable HID keyboard-wedge barcode scanner support.  |

---

## sync

eBay, Etsy, Chairish, and 1stDibs API sync. Each channel is independently
configurable. Changes take effect on restart.

```pson
sync {
    ebay {
        enabled            = false
        client_id          = ""
        client_secret      = ""
        refresh_token      = ""
        sandbox            = true
        sync_interval_min  = 30
        auto_mark_sold     = true
    }
    etsy {
        enabled            = false
        api_key            = ""
        api_secret         = ""
        access_token       = ""
        shop_id            = ""
        sync_interval_min  = 30
        auto_mark_sold     = true
    }
    chairish {
        enabled            = false
        api_key            = ""
        sync_interval_min  = 60
        auto_mark_sold     = true
    }
    onedibs {
        enabled            = false
        api_key            = ""
        dealer_id          = ""
        sync_interval_min  = 60
        auto_mark_sold     = false
    }
}
```

---

## sync_hosts

Override API endpoint URLs for sandbox or custom domains.

```pson
sync_hosts {
    ebay_sandbox    = "api.sandbox.ebay.com"
    ebay_production = "api.ebay.com"
    etsy            = "openapi.etsy.com"
    onedibs         = "api.1stdibs.com"
    chairish        = "api.chairish.com"
}
```

---

## compute

Future compute infrastructure. All settings are currently inactive (`enabled = false`).
GPU acceleration, Kubernetes orchestration, and container management will be
implemented as native C++20 for Windows, Linux, and macOS -- no Docker, no
external tools required.

```pson
compute {
    gpu_enabled          = false
    gpu_backend          = "opencl"
    gpu_device_index     = 0
    kubernetes_enabled   = false
    kubernetes_api_host  = "localhost"
    kubernetes_api_port  = 6443
    kubernetes_namespace = "metis"
    containers_enabled   = false
    container_runtime    = "none"
}
```

| Key                    | Default     | Description                                              |
|------------------------|-------------|----------------------------------------------------------|
| `gpu_enabled`          | `false`     | Enable GPU-accelerated image processing / analytics.     |
| `gpu_backend`          | `opencl`    | GPU API: `opencl` (cross-platform) or `cuda` (NVIDIA).  |
| `gpu_device_index`     | `0`         | GPU device index (0 = first available).                  |
| `kubernetes_enabled`   | `false`     | Enable native C++20 Kubernetes-style orchestration.      |
| `kubernetes_api_host`  | `localhost` | Kubernetes API server hostname.                          |
| `kubernetes_api_port`  | `6443`      | Kubernetes API server port.                              |
| `kubernetes_namespace` | `metis`     | Kubernetes namespace for Metis workloads.                |
| `containers_enabled`   | `false`     | Enable native OCI container management.                  |
| `container_runtime`    | `none`      | Container runtime: `none` / `oci` / `custom`.           |
