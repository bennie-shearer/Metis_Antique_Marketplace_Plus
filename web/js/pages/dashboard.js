// Metis Antique Marketplace Plus v1.2.48 - Dashboard page
'use strict';

Pages.renderDashboard = async function(role) {
    const el = document.getElementById('page-dashboard');
    if (!el) return;
    el.innerHTML = '<div class="dash-loading">Loading dashboard\u2026</div>';

    function usd(v) {
        const n = parseFloat(v) || 0;
        try {
            return n.toLocaleString(CurrencyLocale, {
                style: 'currency', currency: CurrencyCode,
                minimumFractionDigits: 0, maximumFractionDigits: 0
            });
        } catch (e) {
            return CurrencySymbol + Math.abs(n).toLocaleString(undefined, {minimumFractionDigits:0,maximumFractionDigits:0});
        }
    }
    function pct(v) { return (parseFloat(v) || 0).toFixed(1) + '%'; }
    function esc(s) {
        return String(s ?? '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    }
    function fdate(s) {
        if (!s) return '';
        return String(s).slice(0, 10);
    }

    try {
        const [stats, sales, expenses, inquiries, recentSales, recentItems] = await Promise.all([
            Api.get('/api/items/stats'),
            Api.get('/api/sales/summary'),
            Api.get('/api/expenses/summary'),
            Api.get('/api/inquiries/summary'),
            Api.get('/api/sales?limit=6'),
            Api.get('/api/items?limit=6&status=inventory'),
        ]);

        const invValue   = parseFloat(stats.inventory_value) || 0;
        const invCost    = parseFloat(stats.inventory_cost)  || 0;
        const unrealised = invValue - invCost;
        const unrealPct  = invCost > 0 ? ((unrealised / invCost) * 100).toFixed(1) : '0.0';
        const monthExp   = parseFloat(expenses.month_total || expenses.total || 0);
        const monthNet   = parseFloat(sales.month_net) || 0;
        const monthProfit = monthNet - monthExp;

        const kpiColor = v => parseFloat(v) >= 0 ? 'var(--green)' : 'var(--red)';

        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F4CA; Dashboard</h2>
    <span class="dash-subtitle" id="dash-app-subtitle"></span>
  </div>

  <!-- KPI row 1: Inventory -->
  <div class="dash-section-label">Inventory</div>
  <div class="kpi-row">
    <div class="kpi-card">
      <div class="kpi-value">${stats.inventory ?? 0}</div>
      <div class="kpi-label">Items in stock</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value">${stats.listed ?? 0}</div>
      <div class="kpi-label">Listed</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value">${stats.sold ?? 0}</div>
      <div class="kpi-label">Sold (all time)</div>
    </div>
    <div class="kpi-card accent">
      <div class="kpi-value">${usd(invValue)}</div>
      <div class="kpi-label">Asking value (stock)</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value" style="color:${kpiColor(unrealised)}">${usd(unrealised)}</div>
      <div class="kpi-label">Unrealised gain <span class="kpi-sub">${unrealPct}%</span></div>
    </div>
  </div>

  <!-- KPI row 2: This month -->
  <div class="dash-section-label">This month</div>
  <div class="kpi-row">
    <div class="kpi-card">
      <div class="kpi-value">${sales.month_count ?? 0}</div>
      <div class="kpi-label">Sales</div>
    </div>
    <div class="kpi-card accent">
      <div class="kpi-value">${usd(sales.month_revenue)}</div>
      <div class="kpi-label">Revenue</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value">${usd(monthNet)}</div>
      <div class="kpi-label">Net proceeds</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value">${usd(monthExp)}</div>
      <div class="kpi-label">Expenses</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value" style="color:${kpiColor(monthProfit)}">${usd(monthProfit)}</div>
      <div class="kpi-label">Est. profit</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value">${pct(sales.avg_margin_pct)}</div>
      <div class="kpi-label">Avg margin (all time)</div>
    </div>
  </div>

  <!-- KPI row 3: Inquiries -->
  <div class="dash-section-label">Inquiries</div>
  <div class="kpi-row">
    <div class="kpi-card${(inquiries.open > 0) ? ' kpi-alert' : ''}">
      <div class="kpi-value">${inquiries.open ?? 0}</div>
      <div class="kpi-label">Open</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value">${inquiries.closed ?? 0}</div>
      <div class="kpi-label">Closed</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-value">${inquiries.total ?? 0}</div>
      <div class="kpi-label">Total</div>
    </div>
  </div>

  <!-- Bottom panels -->
  <div class="dash-panels">
    <!-- Recent sales -->
    <div class="dash-panel">
      <div class="dash-panel-title">&#x1F4B0; Recent sales</div>
      ${Array.isArray(recentSales) && recentSales.length ? `
      <table class="dash-table">
        <thead><tr><th>Item</th><th>Date</th><th>Price</th><th>Net</th></tr></thead>
        <tbody>
          ${recentSales.slice(0, 6).map(s => `
          <tr>
            <td class="dash-td-item">${esc(s.title || s.item_title || '')}</td>
            <td>${fdate(s.sale_date)}</td>
            <td>${usd(s.sale_price)}</td>
            <td style="color:var(--green)">${usd(s.net_proceeds)}</td>
          </tr>`).join('')}
        </tbody>
      </table>` : '<div class="dash-empty">No sales yet</div>'}
      <button class="dash-link-btn" onclick="app.nav('sales')">View all sales \u2192</button>
    </div>

    <!-- Recent inventory -->
    <div class="dash-panel">
      <div class="dash-panel-title">&#x1F4E6; Recent acquisitions</div>
      ${Array.isArray(recentItems) && recentItems.length ? `
      <table class="dash-table">
        <thead><tr><th>Item</th><th>Category</th><th>Cost</th><th>Ask</th></tr></thead>
        <tbody>
          ${recentItems.slice(0, 6).map(it => `
          <tr>
            <td class="dash-td-item">${esc(it.title)}</td>
            <td><span class="badge badge-gray">${esc(it.category)}</span></td>
            <td>${usd(it.cost_price)}</td>
            <td>${usd(it.asking_price)}</td>
          </tr>`).join('')}
        </tbody>
      </table>` : '<div class="dash-empty">No inventory yet</div>'}
      <button class="dash-link-btn" onclick="app.nav('inventory')">View inventory \u2192</button>
    </div>
  </div>

</div>`;
    } catch (err) {
        el.innerHTML = `<div class="dash-error">Dashboard load failed: ${esc(String(err))}</div>`;
    }
};
