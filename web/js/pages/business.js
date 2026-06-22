// Metis Antique Marketplace Plus v1.2.48 - Business pages
// Consignments, Rentals, Advertising, Credit Card Fees
'use strict';

function escB(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }
function fmtB(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+n.toFixed(2); } }
function fdateB(s) { return s ? String(s).slice(0,10) : ''; }
function statusPill(s, map) { const c = map[s]||'badge-gray'; return `<span class="badge ${c}">${escB(s)}</span>`; }

// ─────────────────────────────────────────────────────────────────────────────
// CONSIGNMENTS
// ─────────────────────────────────────────────────────────────────────────────
Pages.renderConsignments = async function(el, role) {
    el.innerHTML = '<div class="dash-loading">Loading consignments\u2026</div>';
    const canEdit = role !== 'viewer';
    try {
        const [data, sum] = await Promise.all([
            Api.get('/api/consignments'),
            Api.get('/api/consignments/summary')
        ]);
        const rows = Array.isArray(data) ? data : [];
        const statusMap = { active:'badge-blue', sold:'badge-green', paid:'badge-gray', returned:'badge-amber' };
        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F91D; Consignments</h2>
    ${canEdit ? `<button class="btn-primary" onclick="app.openConsignment()">+ Add consignment</button>` : ''}
  </div>
  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-value">${sum.active??0}</div><div class="kpi-label">Active</div></div>
    <div class="kpi-card"><div class="kpi-value">${sum.sold??0}</div><div class="kpi-label">Sold</div></div>
    <div class="kpi-card kpi-alert"><div class="kpi-value">${sum.unpaid??0}</div><div class="kpi-label">Unpaid payouts</div></div>
    <div class="kpi-card accent"><div class="kpi-value">${fmtB(sum.owing)}</div><div class="kpi-label">Owing to consignors</div></div>
  </div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>Consignor</th><th>Description</th><th>Pct %</th><th>Floor</th><th>Received</th><th>Status</th><th>Sale price</th><th>Payout</th>${canEdit?'<th></th>':''}</tr></thead>
      <tbody>
        ${rows.length ? rows.map(r => `<tr>
          <td><strong>${escB(r.consignor_name)}</strong><br><small style="color:var(--text2)">${escB(r.consignor_email)}</small></td>
          <td>${escB(r.description)}</td>
          <td>${parseFloat(r.agreed_pct)||0}%</td>
          <td>${fmtB(r.agreed_floor)}</td>
          <td>${fdateB(r.received_date)}</td>
          <td>${statusPill(r.status, statusMap)}</td>
          <td>${r.sale_price ? fmtB(r.sale_price) : '—'}</td>
          <td>${r.payout_amount ? fmtB(r.payout_amount) : '—'}${r.payout_date?'<br><small style="color:var(--green)">Paid '+fdateB(r.payout_date)+'</small>':''}</td>
          ${canEdit?`<td><button class="btn-ghost" onclick="app.openConsignment(${r.id})">Edit</button> <button class="btn-ghost" title="Print consignor statement" onclick="Pages.renderConsignorStatement('${escB(r.consignor_name)}')">&#x1F4C4; Statement</button> <button class="btn-danger" onclick="app.deleteConsignment(${r.id})">&#x2715;</button></td>`:''}
        </tr>`).join('') : `<tr><td colspan="9" class="empty-state">No consignments yet</td></tr>`}
      </tbody>
    </table>
  </div>
</div>`;
    } catch(e) { el.innerHTML = `<div class="dash-error">Error: ${escB(String(e))}</div>`; }
};

// ─────────────────────────────────────────────────────────────────────────────
// RENTALS
// ─────────────────────────────────────────────────────────────────────────────
Pages.renderRentals = async function(el, role) {
    el.innerHTML = '<div class="dash-loading">Loading rentals\u2026</div>';
    const canEdit = role !== 'viewer';
    try {
        const [data, sum, invoices] = await Promise.all([
            Api.get('/api/rentals'),
            Api.get('/api/rentals/summary'),
            Api.get('/api/booth-invoices')
        ]);
        const rows = Array.isArray(data) ? data : [];
        const invRows = Array.isArray(invoices) ? invoices : [];
        const overdueCount = parseInt(sum.overdue_count)||0;

        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F3E0; Rentals &amp; Booth Fees</h2>
    ${canEdit ? `<button class="btn-primary" onclick="app.openRental()">+ Add rental</button>` : ''}
  </div>
  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-value">${sum.active_count??0}</div><div class="kpi-label">Active rentals</div></div>
    <div class="kpi-card accent"><div class="kpi-value">${fmtB(sum.monthly_total)}</div><div class="kpi-label">Monthly commitment</div></div>
    <div class="kpi-card"><div class="kpi-value">${fmtB(sum.ytd_paid)}</div><div class="kpi-label">YTD collected</div></div>
    <div class="kpi-card${sum.pending_count>0?' kpi-alert':''}"><div class="kpi-value">${sum.pending_count??0}</div><div class="kpi-label">Pending</div></div>
    <div class="kpi-card${overdueCount>0?' kpi-alert':''}"><div class="kpi-value" style="${overdueCount>0?'color:var(--red)':''}">${overdueCount}</div><div class="kpi-label">Overdue &#x26A0;</div></div>
    <div class="kpi-card"><div class="kpi-value" style="color:var(--red)">${fmtB(sum.overdue_total)}</div><div class="kpi-label">Overdue amount</div></div>
  </div>

  <div class="dash-section-label">Active Rentals</div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>Type</th><th>Vendor</th><th>Location</th><th>Rate</th><th>Discount</th><th>Due day</th><th>Late fee</th><th>Start</th><th>Auto-renew</th>${canEdit?'<th></th>':''}</tr></thead>
      <tbody>
        ${rows.length ? rows.map(r => `<tr>
          <td><span class="badge badge-blue">${escB(r.rental_type)}</span></td>
          <td><strong>${escB(r.vendor)}</strong></td>
          <td>${escB(r.location)}</td>
          <td>${fmtB(r.monthly_rate)}</td>
          <td>${parseFloat(r.discount_pct)>0
              ? `<span class="badge badge-green">${parseFloat(r.discount_pct).toFixed(1)}% off${r.discount_label?' — '+escB(r.discount_label):''}</span>`
              : '<span style="color:var(--text2)">None</span>'}</td>
          <td>Day ${r.due_day||1} (${r.grace_days||5} grace)</td>
          <td>${parseFloat(r.late_fee_pct)>0?parseFloat(r.late_fee_pct).toFixed(1)+'%':'—'}</td>
          <td>${fdateB(r.start_date)}</td>
          <td>${r.auto_renew=='1'?'&#x2705;':'&#x274C;'}</td>
          ${canEdit?`<td style="white-space:nowrap">
            <button class="btn-ghost" onclick="app.openRentalPayment(${r.id})">&#x1F4B3; Collect</button>
            <button class="btn-ghost" onclick="app.generateBoothInvoice(${r.id})">&#x1F4CB; Invoice</button>
            <button class="btn-ghost" onclick="app.openRental(${r.id})">Edit</button>
            <button class="btn-danger" onclick="app.deleteRental(${r.id})">&#x2715;</button>
          </td>`:''}
        </tr>`).join('') : `<tr><td colspan="10" class="empty-state">No rentals recorded</td></tr>`}
      </tbody>
    </table>
  </div>

  ${invRows.length ? `
  <div class="dash-section-label" style="margin-top:24px">Booth Invoices</div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>Vendor</th><th>Location</th><th>Period</th><th>Base</th><th>Discount</th><th>Late fee</th><th>Total due</th><th>Status</th><th>Sent</th><th>Paid</th>${canEdit?'<th></th>':''}</tr></thead>
      <tbody>
        ${invRows.map(i => {
          const smap = {sent:'badge-blue',paid:'badge-green',overdue:'badge-amber',cancelled:'badge-gray'};
          return `<tr>
            <td>${escB(i.vendor)}</td>
            <td>${escB(i.location)}</td>
            <td style="font-family:monospace">${escB(i.period)}</td>
            <td>${fmtB(i.amount_due)}</td>
            <td style="color:var(--green)">${parseFloat(i.discount_amt)>0?'-'+fmtB(i.discount_amt):'—'}</td>
            <td style="color:var(--red)">${parseFloat(i.late_fee)>0?'+'+fmtB(i.late_fee):'—'}</td>
            <td style="font-weight:600">${fmtB(i.total_due)}</td>
            <td><span class="badge ${smap[i.status]||'badge-gray'}">${escB(i.status)}</span></td>
            <td style="font-size:11px">${fdateB(i.sent_at)}</td>
            <td style="font-size:11px;color:var(--green)">${i.paid_at?fdateB(i.paid_at):'—'}</td>
            ${canEdit?`<td>${i.status!=='paid'?`<button class="btn-ghost" onclick="app.markInvoicePaid(${i.id})">&#x2705; Paid</button>`:''}</td>`:''}
          </tr>`;
        }).join('')}
      </tbody>
    </table>
  </div>` : ''}
</div>`;
    } catch(e) { el.innerHTML = `<div class="dash-error">Error: ${escB(String(e))}</div>`; }
};

// ─────────────────────────────────────────────────────────────────────────────
// ADVERTISING
// ─────────────────────────────────────────────────────────────────────────────
Pages.renderAdvertising = async function(el, role) {
    el.innerHTML = '<div class="dash-loading">Loading advertising\u2026</div>';
    const canEdit = role !== 'viewer';
    try {
        const [data, sum] = await Promise.all([
            Api.get('/api/advertising'),
            Api.get('/api/advertising/summary')
        ]);
        const rows = Array.isArray(data) ? data : [];
        const ctr = sum.ytd_clicks > 0 && sum.ytd_impressions > 0
            ? ((sum.ytd_clicks / sum.ytd_impressions) * 100).toFixed(2) + '%'
            : '—';
        const cpc = sum.ytd_clicks > 0
            ? fmtB((parseFloat(sum.ytd_spent)||0) / sum.ytd_clicks)
            : '—';
        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F4E3; Advertising</h2>
    ${canEdit ? `<button class="btn-primary" onclick="app.openAd()">+ Add campaign</button>` : ''}
  </div>
  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-value">${sum.active_campaigns??0}</div><div class="kpi-label">Active campaigns</div></div>
    <div class="kpi-card accent"><div class="kpi-value">${fmtB(sum.ytd_spent)}</div><div class="kpi-label">YTD spend</div></div>
    <div class="kpi-card"><div class="kpi-value">${(sum.ytd_impressions||0).toLocaleString()}</div><div class="kpi-label">YTD impressions</div></div>
    <div class="kpi-card"><div class="kpi-value">${(sum.ytd_clicks||0).toLocaleString()}</div><div class="kpi-label">YTD clicks</div></div>
    <div class="kpi-card"><div class="kpi-value">${ctr}</div><div class="kpi-label">CTR</div></div>
    <div class="kpi-card"><div class="kpi-value">${cpc}</div><div class="kpi-label">Cost per click</div></div>
  </div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>Platform</th><th>Campaign</th><th>Type</th><th>Start</th><th>End</th><th>Budget</th><th>Spent</th><th>Impressions</th><th>Clicks</th><th>Conv.</th><th>Status</th>${canEdit?'<th></th>':''}</tr></thead>
      <tbody>
        ${rows.length ? rows.map(r => `<tr>
          <td><strong>${escB(r.platform)}</strong></td>
          <td>${escB(r.campaign)||'—'}</td>
          <td><span class="badge badge-gray">${escB(r.ad_type)}</span></td>
          <td>${fdateB(r.start_date)}</td>
          <td>${r.end_date ? fdateB(r.end_date) : '—'}</td>
          <td>${fmtB(r.budget)}</td>
          <td>${fmtB(r.spent)}</td>
          <td>${(parseInt(r.impressions)||0).toLocaleString()}</td>
          <td>${(parseInt(r.clicks)||0).toLocaleString()}</td>
          <td>${r.conversions||0}</td>
          <td><span class="badge ${r.status==='active'?'badge-green':'badge-gray'}">${escB(r.status)}</span></td>
          ${canEdit?`<td><button class="btn-ghost" onclick="app.openAd(${r.id})">Edit</button> <button class="btn-danger" onclick="app.deleteAd(${r.id})">&#x2715;</button></td>`:''}
        </tr>`).join('') : `<tr><td colspan="12" class="empty-state">No advertising campaigns yet</td></tr>`}
      </tbody>
    </table>
  </div>
</div>`;
    } catch(e) { el.innerHTML = `<div class="dash-error">Error: ${escB(String(e))}</div>`; }
};

// ─────────────────────────────────────────────────────────────────────────────
// CREDIT CARD FEES
// ─────────────────────────────────────────────────────────────────────────────
Pages.renderCcFees = async function(el, role) {
    el.innerHTML = '<div class="dash-loading">Loading CC fees\u2026</div>';
    const canEdit = role !== 'viewer';
    try {
        const [data, sum] = await Promise.all([
            Api.get('/api/cc-fees'),
            Api.get('/api/cc-fees/summary')
        ]);
        const rows = Array.isArray(data) ? data : [];
        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F4B3; Credit Card Processing Fees</h2>
    ${canEdit ? `<button class="btn-primary" onclick="app.openCcFee()">+ Record fee</button>` : ''}
  </div>
  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-value">${fmtB(sum.month_fees)}</div><div class="kpi-label">This month</div></div>
    <div class="kpi-card"><div class="kpi-value">${fmtB(sum.ytd_fees)}</div><div class="kpi-label">YTD fees</div></div>
    <div class="kpi-card accent"><div class="kpi-value">${fmtB(sum.total_fees)}</div><div class="kpi-label">All time</div></div>
  </div>
  <div style="margin:12px 0;padding:12px;background:var(--bg2);border:1px solid var(--border);border-radius:6px">
    <div style="font-size:12px;font-weight:600;color:var(--text2);margin-bottom:8px">&#x1F9EE; Fee calculator</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center">
      <input id="cc-calc-amount" type="number" placeholder="Sale amount" style="width:130px" oninput="app.calcCcFee()">
      <select id="cc-calc-proc" onchange="app.calcCcFee()" style="width:130px">
        <option value="stripe">Stripe (2.9% + $0.30)</option>
        <option value="square">Square (2.6% + $0.10)</option>
        <option value="paypal">PayPal (3.49% + $0.49)</option>
        <option value="venmo">Venmo (1.9% + $0.10)</option>
      </select>
      <span id="cc-calc-result" style="font-weight:600;color:var(--accent)"></span>
    </div>
  </div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>Date</th><th>Processor</th><th>Sale ID</th><th>Rate</th><th>Fee</th><th>Ref</th>${canEdit?'<th></th>':''}</tr></thead>
      <tbody>
        ${rows.length ? rows.map(r => `<tr>
          <td>${fdateB(r.fee_date)}</td>
          <td>${escB(r.processor)}</td>
          <td>${r.sale_id||'—'}</td>
          <td>${parseFloat(r.fee_pct)||0}% + ${fmtB(r.fee_flat)}</td>
          <td style="font-weight:600;color:var(--red)">${fmtB(r.fee_amount)}</td>
          <td>${escB(r.transaction_ref)||'—'}</td>
          ${canEdit?`<td><button class="btn-danger" onclick="app.deleteCcFee(${r.id})">&#x2715;</button></td>`:''}
        </tr>`).join('') : `<tr><td colspan="7" class="empty-state">No fees recorded yet</td></tr>`}
      </tbody>
    </table>
  </div>
</div>`;
    } catch(e) { el.innerHTML = `<div class="dash-error">Error: ${escB(String(e))}</div>`; }
};
