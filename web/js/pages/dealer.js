// Metis Antique Marketplace Plus v1.2.48 - Dealer dashboard and settlements
'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// DEALER DASHBOARD
// ─────────────────────────────────────────────────────────────────────────────
Pages.renderDealerDashboard = async function(el) {
    el.innerHTML = '<div class="dash-loading">Loading your dashboard\u2026</div>';
    function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:0,maximumFractionDigits:0}); } catch(e) { return CurrencySymbol+n.toFixed(0); } }
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function fdate(s) { return s ? String(s).slice(0,10) : ''; }

    try {
        const [stats, salesData, recentItems, recentSales] = await Promise.all([
            Api.get('/api/items/stats'),
            Api.get('/api/sales/summary'),
            Api.get('/api/items?limit=8'),
            Api.get('/api/sales?limit=8')
        ]);

        const items = Array.isArray(recentItems) ? recentItems
                    : (recentItems.items || []);
        const sales = Array.isArray(recentSales) ? recentSales : [];

        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F4CA; My Dealer Dashboard</h2>
    <span class="dash-subtitle" style="color:var(--text2);font-size:12px">Real-time view of your inventory and sales</span>
  </div>

  <div class="dash-section-label">My Inventory</div>
  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-value">${stats.inventory??0}</div><div class="kpi-label">In stock</div></div>
    <div class="kpi-card"><div class="kpi-value">${stats.listed??0}</div><div class="kpi-label">Listed</div></div>
    <div class="kpi-card"><div class="kpi-value">${stats.sold??0}</div><div class="kpi-label">Sold</div></div>
    <div class="kpi-card accent"><div class="kpi-value">${usd(stats.inventory_value)}</div><div class="kpi-label">Asking value</div></div>
  </div>

  <div class="dash-section-label">My Sales (this month)</div>
  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-value">${salesData.month_count??0}</div><div class="kpi-label">Sales</div></div>
    <div class="kpi-card accent"><div class="kpi-value">${usd(salesData.month_revenue)}</div><div class="kpi-label">Revenue</div></div>
    <div class="kpi-card"><div class="kpi-value">${usd(salesData.month_net)}</div><div class="kpi-label">Net proceeds</div></div>
    <div class="kpi-card"><div class="kpi-value">${(parseFloat(salesData.avg_margin_pct)||0).toFixed(1)}%</div><div class="kpi-label">Avg margin</div></div>
  </div>

  <div class="dash-panels">
    <div class="dash-panel">
      <div class="dash-panel-title">&#x1F4E6; My recent items</div>
      ${items.length ? `<table class="dash-table">
        <thead><tr><th>SKU</th><th>Item</th><th>Status</th><th>Ask</th></tr></thead>
        <tbody>${items.slice(0,8).map(it => `<tr>
          <td style="font-family:monospace;font-size:11px;color:var(--text2)">${esc(it.sku||'—')}</td>
          <td class="dash-td-item">${esc(it.title)}</td>
          <td><span class="badge badge-${it.status==='sold'?'green':it.status==='listed'?'blue':'gray'}">${esc(it.status)}</span></td>
          <td>${usd(it.asking_price)}</td>
        </tr>`).join('')}</tbody>
      </table>` : '<div class="dash-empty">No items yet</div>'}
      <button class="dash-link-btn" onclick="app.nav('inventory')">View my inventory \u2192</button>
    </div>

    <div class="dash-panel">
      <div class="dash-panel-title">&#x1F4B0; My recent sales</div>
      ${sales.length ? `<table class="dash-table">
        <thead><tr><th>Item</th><th>Date</th><th>Net</th></tr></thead>
        <tbody>${sales.slice(0,8).map(s => `<tr>
          <td class="dash-td-item">${esc(s.title||s.item_title||'')}</td>
          <td>${fdate(s.sale_date)}</td>
          <td style="color:var(--green)">${usd(s.net_proceeds)}</td>
        </tr>`).join('')}</tbody>
      </table>` : '<div class="dash-empty">No sales yet</div>'}
      <button class="dash-link-btn" onclick="app.nav('sales')">View my sales \u2192</button>
    </div>
  </div>
</div>`;
    } catch(e) { el.innerHTML = `<div class="dash-error">Error: ${e}</div>`; }
};

// ─────────────────────────────────────────────────────────────────────────────
// SETTLEMENTS
// ─────────────────────────────────────────────────────────────────────────────
Pages.renderSettlements = async function(el, role) {
    el.innerHTML = '<div class="dash-loading">Loading settlements\u2026</div>';
    function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+n.toFixed(2); } }
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function fdate(s) { return s ? String(s).slice(0,10) : '—'; }

    try {
        const r = await Api.get('/api/settlements');
        const rows = Array.isArray(r) ? r : [];
        const statusMap = { pending:'badge-amber', sent:'badge-blue', paid:'badge-green', cancelled:'badge-gray' };

        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F4CB; Dealer Settlements</h2>
    <span class="dash-subtitle" style="color:var(--text2);font-size:12px">
      Auto-generated after ${role==='admin' ? 'grace period' : 'each settlement period'}.
      Enable in config.pson: settlements.enabled = true
    </span>
  </div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>Dealer</th><th>Period</th><th>Sales</th><th>Fees</th><th>Net due</th><th>Status</th><th>Sent</th>${role==='admin'?'<th></th>':''}</tr></thead>
      <tbody>
        ${rows.length ? rows.map(s => `<tr>
          <td>${esc(s.username||s.owner_id)}</td>
          <td style="font-size:12px">${fdate(s.period_start)} → ${fdate(s.period_end)}</td>
          <td>${usd(s.total_sales)}</td>
          <td style="color:var(--red)">${usd(s.total_fees)}</td>
          <td style="font-weight:600;color:var(--green)">${usd(s.net_due)}</td>
          <td><span class="badge ${statusMap[s.status]||'badge-gray'}">${esc(s.status)}</span></td>
          <td style="font-size:11px;color:var(--text2)">${s.sent_at ? fdate(s.sent_at) : '—'}</td>
          ${role==='admin'?`<td>
            <button class="btn-ghost" onclick="app.markSettlementPaid(${s.id})">&#x2705; Mark paid</button>
            <button class="btn-ghost" onclick="app.emailSettlement(${s.id})">&#x2709; Email</button>
          </td>`:''}
        </tr>`).join('') : `<tr><td colspan="8" class="empty-state" style="padding:24px;text-align:center">No settlements yet. Enable in config.pson and wait for grace period to elapse.</td></tr>`}
      </tbody>
    </table>
  </div>
</div>`;
    } catch(e) { el.innerHTML = `<div class="dash-error">Error: ${e}</div>`; }
};
