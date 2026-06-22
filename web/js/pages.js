// Metis Antique Marketplace Plus v1.2.48 - Page renderers
var Pages = (() => {

function esc(s) {
    return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
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
function statusBadge(s) {
    const colors={inventory:'badge-gray',listed:'badge-blue',sold:'badge-green',
        pending:'badge-amber','in review':'badge-amber',active:'badge-green',
        removed:'badge-red',complete:'badge-green'};
    return `<span class="badge ${colors[s]||'badge-gray'}">${esc(s)}</span>`;
}

// -----------------------------------------------------------------------
// Inventory
// -----------------------------------------------------------------------
function renderItemsTable(items, total, page, limit) {
    const pages = Math.ceil(total / limit);
    if (!items.length) return '<div class="empty-state"><div class="empty-icon">&#x1F4E6;</div><p>No items found</p></div>';
    return `
<div class="table-wrap">
  <table>
    <thead><tr>
      <th>Title / Maker</th><th>Category</th><th>Era</th><th>Cond.</th>
      <th>Cost</th><th>Asking</th><th>Status</th><th></th>
    </tr></thead>
    <tbody>${items.map(it => `
<tr>
  <td>
    ${it.photo_ref ? `<img src="/photos/${esc(it.photo_ref)}" style="width:32px;height:32px;object-fit:cover;border-radius:3px;vertical-align:middle;margin-right:6px">` : ''}
    <strong>${esc(it.title)}</strong>${it.maker?`<br><span style="color:var(--text2);font-size:12px">${esc(it.maker)}</span>`:''}
  </td>
  <td>${esc(it.category)}</td>
  <td>${esc(it.era||'')}</td>
  <td>${esc(it.condition)}</td>
  <td>${usd(it.cost_price)}</td>
  <td>${usd(it.asking_price)}</td>
  <td>${statusBadge(it.status)}</td>
  <td style="white-space:nowrap">
    <button class="btn-ghost" onclick="app.openItemDetail(${it.id})">&#x1F50D;</button>
    <button class="btn-ghost" onclick="app.openEditItem(${it.id})">Edit</button>
    <button class="btn-ghost" onclick="LabelUI.printLabel(${it.id})" title="Print label">&#x1F3F7;</button>
    <button class="btn-ghost" onclick="app.duplicateItem(${it.id})">&#x2398;</button>
    <button class="btn-danger" onclick="app.deleteItem(${it.id})">&#x2715;</button>
  </td>
</tr>`).join('')}</tbody>
  </table>
</div>
${pages > 1 ? `<div class="pagination">
  ${page>1?`<button class="btn-ghost" onclick="Pages.loadInventory(${page-1})">&#x2190; Prev</button>`:''}
  <span style="color:var(--text2);font-size:12px">Page ${page} of ${pages} (${total} items)</span>
  ${page<pages?`<button class="btn-ghost" onclick="Pages.loadInventory(${page+1})">Next &#x2192;</button>`:''}
</div>` : `<p style="font-size:12px;color:var(--text2);padding:10px 14px">${total} item${total!==1?'s':''}</p>`}`;
}

async function renderInventory(el) {
    const [statsR, itemsR] = await Promise.all([
        Api.get('/api/items/stats'),
        Api.get('/api/items?limit=20&page=1')
    ]);
    const s = statsR.data;
    const res = itemsR.data;
    const items = res.items || [];
    const total = res.total || 0;

    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Inventory</h1>
  <div style="display:flex;gap:8px">
    <button class="btn-ghost" onclick="app.openImport()">&#x1F4C1; Import CSV</button>
    <button class="btn" onclick="app.openAddItem()">+ Add item</button>
  </div>
</div>
<div class="stat-grid">
  <div class="stat-card"><div class="stat-label">In stock</div><div class="stat-value">${esc(s.inventory)}</div></div>
  <div class="stat-card"><div class="stat-label">Listed</div><div class="stat-value amber">${esc(s.listed)}</div></div>
  <div class="stat-card"><div class="stat-label">Sold</div><div class="stat-value green">${esc(s.sold)}</div></div>
  <div class="stat-card"><div class="stat-label">Portfolio value</div><div class="stat-value">${usd(s.inventory_value)}</div></div>
  <div class="stat-card"><div class="stat-label">Cost basis</div><div class="stat-value">${usd(s.inventory_cost)}</div></div>
</div>
<div class="toolbar">
  <input type="text" id="inv-q" placeholder="Search title, maker, era, description..." oninput="Pages.loadInventory(1)">
  <select id="inv-cat" onchange="Pages.loadInventory(1)">
    <option value="">All categories</option>
    ${['Furniture','Ceramics','Jewelry','Art & Prints','Silver & Metals','Clocks & Watches','Books & Maps','Other'].map(c=>`<option>${c}</option>`).join('')}
  </select>
  <select id="inv-status" onchange="Pages.loadInventory(1)">
    <option value="">All statuses</option>
    <option value="inventory">In stock</option>
    <option value="listed">Listed</option>
    <option value="sold">Sold</option>
  </select>
</div>
<div id="inv-table-wrap">
  ${renderItemsTable(items, total, 1, 20)}
</div>`;
}

async function loadInventory(page) {
    const q    = document.getElementById('inv-q')?.value||'';
    const cat  = document.getElementById('inv-cat')?.value||'';
    const stat = document.getElementById('inv-status')?.value||'';
    const limit = 20;
    let url = `/api/items?page=${page}&limit=${limit}`;
    if (q)    url += '&q=' + encodeURIComponent(q);
    if (cat)  url += '&category=' + encodeURIComponent(cat);
    if (stat) url += '&status=' + encodeURIComponent(stat);
    const r = await Api.get(url);
    const res = r.data;
    const wrap = document.getElementById('inv-table-wrap');
    if (wrap) wrap.innerHTML = renderItemsTable(res.items||[], res.total||0, page, limit);
}

// -----------------------------------------------------------------------
// Listings
// -----------------------------------------------------------------------
async function renderListings(el) {
    const r = await Api.get('/api/listings');
    const listings = Array.isArray(r.data) ? r.data : [];
    const expiring = listings.filter(l=>l.expiring_soon && l.status==='active');
    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Active listings</h1>
  <button class="btn" onclick="app.openAddListing()">+ New listing</button>
</div>
${expiring.length ? `<div class="readonly-banner" style="background:#3a2e10;border-color:var(--amber);color:var(--amber)">
  &#x26A0; ${expiring.length} auction${expiring.length>1?'s':''} ending within 48 hours: ${expiring.map(l=>esc(l.title)).join(', ')}
</div>` : ''}
<div class="table-wrap">
  <table>
    <thead><tr><th>Item</th><th>Channel</th><th>Type</th><th>Price</th><th>Views</th><th>Watchers</th><th>Status</th><th>URL</th><th></th></tr></thead>
    <tbody>${listings.length ? listings.map(l => `
<tr${l.expiring_soon&&l.status==='active'?' style="background:rgba(212,160,48,0.07)"':''}>
  <td>${esc(l.title)}</td>
  <td>${esc(l.channel)}</td>
  <td>${esc(l.list_type)}</td>
  <td>${usd(l.list_price)}</td>
  <td>${esc(l.views)}</td>
  <td><input type="number" value="${esc(l.watchers)}" style="width:60px" onchange="app.updateWatchers(${l.id},this.value)"></td>
  <td>${statusBadge(l.status)}</td>
  <td>${l.listing_url?`<a href="${esc(l.listing_url)}" target="_blank" style="color:var(--blue)">&#x1F517;</a>`:'—'}</td>
  <td style="white-space:nowrap">
    <button class="btn-ghost" onclick="app.openEditListing(${l.id})">Edit</button>
    <button class="btn-danger" onclick="app.removeListing(${l.id})">Remove</button>
  </td>
</tr>`).join('') : '<tr><td colspan="9" style="text-align:center;color:var(--text2);padding:32px">No active listings</td></tr>'}</tbody>
  </table>
</div>`;
}

// -----------------------------------------------------------------------
// Sales
// -----------------------------------------------------------------------
function renderSalesTable(sales) {
    if (!sales.length) return '<div class="empty-state"><div class="empty-icon">&#x1F4B0;</div><p>No sales recorded</p></div>';
    return `<div class="table-wrap"><table>
  <thead><tr><th>Item</th><th>Sale</th><th>Fees</th><th>Ship</th><th>Net</th><th>Margin</th><th>Method</th><th>Buyer</th><th>Channel</th><th>Date</th><th></th></tr></thead>
  <tbody>${sales.map(s=>`
<tr>
  <td>${esc(s.title)}</td>
  <td>${usd(s.sale_price)}</td>
  <td>${usd(s.platform_fee)}</td>
  <td>${usd(s.shipping_cost)}</td>
  <td><strong>${usd(s.net_proceeds)}</strong></td>
  <td><span class="badge ${parseInt(s.margin_pct)>=0?'badge-green':'badge-red'}">${esc(s.margin_pct)}%</span></td>
  <td>${esc(s.payment_method||'')}</td>
  <td>${esc(s.buyer_name||'')}</td>
  <td>${esc(s.channel||'')}</td>
  <td>${esc((s.sale_date||'').slice(0,10))}</td>
  <td style="white-space:nowrap"><button class="btn-ghost" title="Invoice" onclick="window.open('/api/sales/${s.id}/invoice','_blank')">&#x1F9FE;</button>${s.buyer_email ? `<button class="btn-ghost" title="Email receipt to ${esc(s.buyer_email)}" onclick="app.emailReceipt(${s.id},'${esc(s.buyer_email)}')">&#x2709;</button>` : ''}<button class="btn-ghost" title="Print receipt" onclick="app.posPrintReceipt(${s.id})">&#x1F5A8;</button><button class="btn-ghost" onclick="app.openEditSale(${s.id})">Edit</button><button class="btn-danger" onclick="app.deleteSale(${s.id})">&#x2715;</button></td>
</tr>`).join('')}</tbody>
</table></div>`;
}

async function renderSales(el) {
    const [sumR, salesR] = await Promise.all([
        Api.get('/api/sales/summary'),
        Api.get('/api/sales')
    ]);
    const sum   = sumR.data;
    const sales = Array.isArray(salesR.data) ? salesR.data : [];
    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Sales</h1>
  <div style="display:flex;gap:8px">
    <button class="btn-ghost" onclick="window.open('/api/sales/export?year=${new Date().getFullYear()}','_blank')">&#x1F4CA; Export CSV</button>
    <button class="btn" onclick="app.openRecordSale()">+ Record sale</button>
  </div>
</div>
<div class="stat-grid">
  <div class="stat-card"><div class="stat-label">This month (gross)</div><div class="stat-value">${usd(sum.month_revenue)}</div></div>
  <div class="stat-card"><div class="stat-label">This month (net)</div><div class="stat-value">${usd(sum.month_net)}</div></div>
  <div class="stat-card"><div class="stat-label">Items sold (month)</div><div class="stat-value">${esc(sum.month_count)}</div></div>
  <div class="stat-card"><div class="stat-label">Rolling 30d net</div><div class="stat-value">${usd(sum.rolling30_net)}</div></div>
  <div class="stat-card"><div class="stat-label">Avg margin</div><div class="stat-value green">${esc(sum.avg_margin_pct)}%</div></div>
</div>
<div class="toolbar">
  <input type="date" id="sales-from" onchange="Pages.loadSales()">
  <span style="color:var(--text2);font-size:13px">to</span>
  <input type="date" id="sales-to" onchange="Pages.loadSales()">
  <button class="btn-ghost" onclick="document.getElementById('sales-from').value='';document.getElementById('sales-to').value='';Pages.loadSales()">Clear</button>
</div>
<div id="sales-table-wrap">
  ${renderSalesTable(sales)}
</div>`;
}

async function loadSales() {
    const from = document.getElementById('sales-from')?.value||'';
    const to   = document.getElementById('sales-to')?.value||'';
    let url = '/api/sales';
    if (from||to) url += '?' + (from?'from='+from:'') + (from&&to?'&':'') + (to?'to='+to:'');
    const r = await Api.get(url);
    const wrap = document.getElementById('sales-table-wrap');
    if (wrap) wrap.innerHTML = renderSalesTable(Array.isArray(r.data)?r.data:[]);
}

// -----------------------------------------------------------------------
// Appraisals
// -----------------------------------------------------------------------
async function renderAppraisals(el) {
    const r = await Api.get('/api/appraisals');
    const list = Array.isArray(r.data) ? r.data : [];
    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Appraisals</h1>
  <button class="btn" onclick="app.openAddAppraisal()">+ Request appraisal</button>
</div>
<div class="table-wrap"><table>
  <thead><tr><th>Item</th><th>Appraiser</th><th>Value range</th><th>Condition</th><th>Date</th><th>Status</th><th></th></tr></thead>
  <tbody>${list.length ? list.map(a=>`
<tr>
  <td>${esc(a.title)}</td>
  <td>${esc(a.appraiser||'')}</td>
  <td>${a.value_low!==null ? usd(a.value_low)+' – '+usd(a.value_high) : '—'}</td>
  <td>${esc(a.condition||'')}</td>
  <td>${esc((a.appraised_at||'').slice(0,10))}</td>
  <td>${statusBadge(a.status)}</td>
  <td><button class="btn-ghost" onclick="app.openEditAppraisal(${a.id})">Update</button><button class="btn-danger" onclick="app.deleteAppraisal(${a.id})">&#x2715;</button></td>
</tr>`).join('') : '<tr><td colspan="7" style="text-align:center;color:var(--text2);padding:32px">No appraisals yet</td></tr>'}</tbody>
</table></div>`;
}

// -----------------------------------------------------------------------
// Market
// -----------------------------------------------------------------------
async function renderMarket(el) {
    el.innerHTML = `
<div class="page-header"><h1 class="page-title">Market &amp; Performance</h1>
  <div style="display:flex;gap:8px;align-items:center">
    <input type="date" id="mkt-from" placeholder="From">
    <span style="color:var(--text2);font-size:13px">to</span>
    <input type="date" id="mkt-to">
    <button class="btn" onclick="Pages.loadMarket()">Apply</button>
    <button class="btn-ghost" onclick="document.getElementById('mkt-from').value='';document.getElementById('mkt-to').value='';Pages.loadMarket()">All time</button>
  </div>
</div>
<div id="mkt-content"><p style="color:var(--text2);padding:32px">Loading...</p></div>`;
    await loadMarket();
}

async function loadMarket() {
    const from = document.getElementById('mkt-from')?.value||'';
    const to   = document.getElementById('mkt-to')?.value||'';
    let qs = (from||to) ? '?'+(from?'from='+from:'')+(from&&to?'&':'')+(to?'to='+to:'') : '';
    const [catR, trendR, topR, portR, roiR] = await Promise.all([
        Api.get('/api/market/categories'+qs),
        Api.get('/api/market/trend'+qs),
        Api.get('/api/market/top'+qs),
        Api.get('/api/reports/portfolio'+qs),
        Api.get('/api/reports/roi_by_source'+qs)
    ]);
    const cats  = Array.isArray(catR.data)  ? catR.data  : [];
    const trend = Array.isArray(trendR.data) ? trendR.data : [];
    const top   = Array.isArray(topR.data)   ? topR.data   : [];
    const port  = Array.isArray(portR.data)  ? portR.data  : [];
    const roi   = Array.isArray(roiR.data)   ? roiR.data   : [];

    const maxRev   = Math.max(...cats.map(c=>parseFloat(c.revenue)||0), 1);
    const maxTrend = Math.max(...trend.map(t=>parseFloat(t.revenue)||0), 1);
    const maxPort  = Math.max(...port.map(p=>parseFloat(p.asking_value)||0), 1);

    const content = document.getElementById('mkt-content');
    if (!content) return;
    content.innerHTML = `
<div class="chart-grid">
  <div class="chart-card">
    <h3>Revenue by category</h3>
    ${cats.length ? cats.map(c=>{
        const pct=Math.round((parseFloat(c.revenue)||0)/maxRev*100);
        return `<div class="bar-row"><div class="bar-label" title="${esc(c.category)}">${esc(c.category)}</div>
        <div class="bar-track"><div class="bar-fill" style="width:${pct}%"></div></div>
        <div class="bar-val">${usd(c.revenue)} <span style="color:var(--text2);font-size:11px">(${esc(c.sold_count)} sold)</span></div></div>`;
    }).join('') : '<p style="color:var(--text2);font-size:13px">No sales data yet</p>'}
  </div>
  <div class="chart-card">
    <h3>Monthly revenue trend</h3>
    ${trend.length ? trend.slice(0,6).map(t=>{
        const pct=Math.round((parseFloat(t.revenue)||0)/maxTrend*100);
        return `<div class="bar-row"><div class="bar-label">${esc(t.month)}</div>
        <div class="bar-track"><div class="bar-fill" style="width:${pct}%"></div></div>
        <div class="bar-val">${usd(t.net_revenue)}</div></div>`;
    }).join('') : '<p style="color:var(--text2);font-size:13px">No trend data yet</p>'}
  </div>
</div>
<div class="chart-grid" style="margin-top:18px">
  <div class="chart-card">
    <h3>Portfolio (in stock)</h3>
    ${port.length ? port.map(p=>{
        const pct=Math.round((parseFloat(p.asking_value)||0)/maxPort*100);
        return `<div class="bar-row"><div class="bar-label" title="${esc(p.category)}">${esc(p.category)}</div>
        <div class="bar-track"><div class="bar-fill" style="background:var(--blue);width:${pct}%"></div></div>
        <div class="bar-val">${usd(p.asking_value)} <span style="color:var(--green);font-size:11px">+${usd(p.unrealized_gain)}</span></div></div>`;
    }).join('') : '<p style="color:var(--text2);font-size:13px">No inventory</p>'}
  </div>
  <div class="chart-card">
    <h3>ROI by acquisition source</h3>
    ${roi.length ? roi.map(r=>`
    <div class="bar-row">
      <div class="bar-label" title="${esc(r.source)}">${esc(r.source)}</div>
      <div class="bar-track"><div class="bar-fill" style="background:var(--green);width:${Math.min(100,Math.round(parseFloat(r.avg_roi_pct)||0))}%"></div></div>
      <div class="bar-val">${Math.round(parseFloat(r.avg_roi_pct)||0)}% avg ROI</div>
    </div>`).join('') : '<p style="color:var(--text2);font-size:13px">No source data — add source field to items</p>'}
  </div>
</div>
<div style="margin-top:18px">
  <div class="chart-card">
    <h3>Top performers by profit</h3>
    <div class="table-wrap" style="border:none">
      <table>
        <thead><tr><th>Title</th><th>Category</th><th>Date</th><th>Sale</th><th>Cost</th><th>Net</th><th>Profit</th></tr></thead>
        <tbody>${top.length ? top.map(t=>`
<tr>
  <td>${esc(t.title)}</td><td>${esc(t.category)}</td>
  <td>${esc((t.sale_date||'').slice(0,10))}</td>
  <td>${usd(t.sale_price)}</td><td>${usd(t.cost_price)}</td>
  <td>${usd(t.net_proceeds)}</td>
  <td style="color:var(--green)">${usd(t.profit)}</td>
</tr>`).join('') : '<tr><td colspan="7" style="text-align:center;color:var(--text2);padding:24px">No data yet</td></tr>'}</tbody>
      </table>
    </div>
  </div>
</div>`;
}

// -----------------------------------------------------------------------
// Users
// -----------------------------------------------------------------------
async function renderUsers(el) {
    const [ur, sr] = await Promise.all([Api.get('/api/users'), Api.get('/api/audit/sessions')]);
    if (ur.status === 403) {
        el.innerHTML = '<div class="empty-state"><div class="empty-icon">&#x1F512;</div><p>Admin access required</p></div>';
        return;
    }
    const users    = Array.isArray(ur.data) ? ur.data : [];
    const sessions = Array.isArray(sr.data) ? sr.data : [];
    const roleBadge = role => {
        const colors={admin:'#a080e0',dealer:'#60c080',viewer:'#6090c0'};
        const bg={admin:'#2a1f40',dealer:'#1a2f1f',viewer:'#1a2a3f'};
        return `<span class="badge" style="background:${bg[role]||'var(--bg3)'};color:${colors[role]||'var(--text2)'}">${esc(role)}</span>`;
    };
    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">User management</h1>
  <button class="btn" onclick="app.openAddUser()">+ Add user</button>
</div>
<div class="table-wrap"><table>
  <thead><tr><th>ID</th><th>Username</th><th>Display name</th><th>Email</th><th>Role</th><th>Created</th><th></th></tr></thead>
  <tbody>${users.length ? users.map(u=>`
<tr>
  <td style="color:var(--text2)">${u.id}</td>
  <td><strong>${esc(u.username)}</strong></td>
  <td>${esc(u.display_name||'')}</td>
  <td><a href="mailto:${esc(u.email||'')}" style="color:var(--accent)">${esc(u.email||'—')}</a></td>
  <td>${roleBadge(u.role)}</td>
  <td>${(u.created_at||'').slice(0,10)}</td>
  <td style="display:flex;gap:6px">
    <button class="btn-ghost" onclick="app.openEditUser(${u.id},'${esc(u.username)}','${u.role}','${esc(u.email||'')}','${esc(u.display_name||'')}')">Edit</button>
    <button class="btn-danger" onclick="app.deleteUser(${u.id},'${esc(u.username)}')">Delete</button>
  </td>
</tr>`).join('') : '<tr><td colspan="7" style="text-align:center;color:var(--text2);padding:32px">No users found</td></tr>'}</tbody>
</table></div>
${sessions.length ? `
<div style="margin-top:24px">
  <div class="dash-section-label">Active sessions</div>
  <div class="table-wrap"><table>
    <thead><tr><th>User</th><th>Created</th><th>Expires</th><th></th></tr></thead>
    <tbody>${sessions.map(s=>`<tr>
      <td><strong>${esc(s.username||'')}</strong></td>
      <td style="font-size:12px">${(s.created_at||'').slice(0,16).replace('T',' ')}</td>
      <td style="font-size:12px">${(s.expires_at||'').slice(0,16).replace('T',' ')}</td>
      <td><button class="btn-danger" onclick="app.revokeSession('${esc(s.token||'')}')">&#x2715; Revoke</button></td>
    </tr>`).join('')}</tbody>
  </table></div>
</div>` : ''}
<div style="margin-top:18px;background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:16px">
  <p style="font-size:13px;font-weight:600;margin-bottom:10px">Role permissions</p>
  <table style="width:100%;font-size:12px">
    <thead><tr><th>Permission</th><th style="color:#a080e0">Admin</th><th style="color:#60c080">Dealer</th><th style="color:#6090c0">Viewer</th></tr></thead>
    <tbody>
      <tr><td>View all pages</td><td>&#x2713;</td><td>&#x2713;</td><td>&#x2713;</td></tr>
      <tr><td>Add / edit / delete items and records</td><td>&#x2713;</td><td>&#x2713;</td><td>&#x2717;</td></tr>
      <tr><td>Photo upload / CSV import</td><td>&#x2713;</td><td>&#x2713;</td><td>&#x2717;</td></tr>
      <tr><td>Manage users</td><td>&#x2713;</td><td>&#x2717;</td><td>&#x2717;</td></tr>
      <tr><td>Change own password</td><td>&#x2713;</td><td>&#x2713;</td><td>&#x2713;</td></tr>
    </tbody>
  </table>
</div>`;
}

// -----------------------------------------------------------------------
// P&L report
// -----------------------------------------------------------------------
async function renderPnl(el) {
    function usd2(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return (n<0?'-':'')+CurrencySymbol+Math.abs(n).toFixed(2); } }
    function pct(v) { return (parseFloat(v)||0).toFixed(1)+'%'; }
    function clr(v) { return parseFloat(v||0)>=0?'var(--green)':'var(--red)'; }

    el.innerHTML = `
<div class="page-header"><h1 class="page-title">Profit &amp; Loss</h1>
  <div style="display:flex;gap:8px;align-items:center">
    <input type="date" id="pnl-from" placeholder="From">
    <span style="color:var(--text2);font-size:13px">to</span>
    <input type="date" id="pnl-to">
    <button class="btn" onclick="Pages.loadPnl()">Apply</button>
    <button class="btn-ghost" onclick="document.getElementById('pnl-from').value='';document.getElementById('pnl-to').value='';Pages.loadPnl()">All time</button>
  </div>
</div>
<div id="pnl-content"><p style="color:var(--text2);padding:32px">Loading...</p></div>`;
    await loadPnl();
}

async function loadPnl() {
    function usd2(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return (n<0?'-':'')+CurrencySymbol+Math.abs(n).toFixed(2); } }
    function pct(v) { return (parseFloat(v)||0).toFixed(1)+'%'; }
    function clr(v) { return parseFloat(v||0)>=0?'var(--green)':'var(--red)'; }

    const from = document.getElementById('pnl-from')?.value||'';
    const to   = document.getElementById('pnl-to')?.value||'';
    let qs = (from||to) ? '?'+(from?'from='+from:'')+(from&&to?'&':'')+(to?'to='+to:'') : '';

    const [pnlR, expSumR, purchSumR] = await Promise.all([
        Api.get('/api/reports/pnl'+qs),
        Api.get('/api/expenses/summary'+qs),
        Api.get('/api/purchases/summary'+qs)
    ]);
    const p = pnlR.data || {};
    const bycat = p.by_category || [];
    const expcats = p.expenses_by_category || [];

    const content = document.getElementById('pnl-content');
    if (!content) return;

    const rows = [
        ['REVENUE',                '',                     ''],
        ['Gross revenue',          usd2(p.gross_revenue),  ''],
        ['Platform fees',         '-'+usd2(p.platform_fees),'text2'],
        ['Outbound shipping',     '-'+usd2(p.shipping_out), 'text2'],
        ['Net revenue',            usd2(p.net_revenue),    'bold'],
        ['',                       '',                     ''],
        ['COST OF GOODS SOLD',     '',                     ''],
        ['Cost of items sold',    '-'+usd2(p.cogs),        ''],
        ['Gross profit',           usd2(p.gross_profit),   `bold;color:${clr(p.gross_profit)}`],
        ['Gross margin',           pct(p.margin_pct),      `color:${clr(p.gross_profit)}`],
        ['',                       '',                     ''],
        ['OVERHEAD EXPENSES',      '',                     ''],
        ...expcats.map(e => [esc(e.category), '-'+usd2(e.amount), 'text2']),
        ['Total expenses',        '-'+usd2(p.overhead_expenses), 'bold'],
        ['',                       '',                     ''],
        ['NET PROFIT',             usd2(p.net_profit),
         `bold;font-size:18px;color:${clr(p.net_profit)}`],
    ];

    content.innerHTML = `
<div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">
  <div class="chart-card" style="grid-column:1/-1">
    <h3>Statement ${from||to ? (from?from.slice(0,7):'')+' – '+(to?to.slice(0,7):'') : '(all time)'}</h3>
    <table style="width:100%;border-collapse:collapse">
      ${rows.map(([label, value, style]) => {
          if (!label) return '<tr><td colspan="2" style="padding:4px 0;border:none"></td></tr>';
          const isHeader = !value;
          return `<tr style="border-bottom:1px solid var(--border)">
  <td style="padding:8px 4px;font-size:${isHeader?'11px':'13px'};
      color:${isHeader?'var(--text2)':'var(--text)'};
      font-weight:${isHeader||style.includes('bold')?'600':'400'};
      text-transform:${isHeader?'uppercase':''};
      letter-spacing:${isHeader?'.06em':''}">${label}</td>
  <td style="padding:8px 4px;font-size:13px;text-align:right;
      ${style.includes('text2')?'color:var(--text2)':''}
      ${style.includes('color:')?style.split(';').find(s=>s.includes('color'))||'':''};
      font-weight:${style.includes('bold')?'600':'400'};
      font-size:${style.includes('font-size')?style.split(';').find(s=>s.includes('font-size'))?.split(':')[1]||'13px':'13px'}">${value}</td>
</tr>`;
      }).join('')}
    </table>
  </div>
  <div class="chart-card">
    <h3>Cash flow</h3>
    ${[
      ['Items purchased', p.items_purchased, ''],
      ['Purchase spend', usd2(p.purchases_spent), 'red'],
      ['Items sold', p.items_sold, ''],
      ['Net proceeds received', usd2(p.net_revenue), 'green'],
      ['Overhead paid', usd2(p.overhead_expenses), 'red'],
      ['Net cash (approx)', usd2((parseFloat(p.net_revenue)||0)-(parseFloat(p.purchases_spent)||0)-(parseFloat(p.overhead_expenses)||0)), ''],
    ].map(([k,v,c]) => `
    <div style="display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--border)">
      <span style="font-size:13px;color:var(--text2)">${k}</span>
      <span style="font-size:13px;font-weight:500;color:${c==='red'?'var(--red)':c==='green'?'var(--green)':'var(--text)'}">${v}</span>
    </div>`).join('')}
  </div>
  <div class="chart-card">
    <h3>Profit by category</h3>
    ${bycat.length ? `<table style="width:100%;border-collapse:collapse;font-size:12px">
      <thead><tr style="border-bottom:1px solid var(--border)">
        <th style="text-align:left;padding:4px 6px;color:var(--text2)">Category</th>
        <th style="text-align:right;padding:4px 6px;color:var(--text2)">Units</th>
        <th style="text-align:right;padding:4px 6px;color:var(--text2)">Revenue</th>
        <th style="text-align:right;padding:4px 6px;color:var(--text2)">COGS</th>
        <th style="text-align:right;padding:4px 6px;color:var(--text2)">Profit</th>
        <th style="text-align:right;padding:4px 6px;color:var(--text2)">Margin</th>
      </tr></thead>
      <tbody>${bycat.map(c => `
      <tr style="border-bottom:1px solid var(--border)">
        <td style="padding:6px">${esc(c.category)}</td>
        <td style="text-align:right;padding:6px">${esc(c.units)}</td>
        <td style="text-align:right;padding:6px">${usd2(c.revenue)}</td>
        <td style="text-align:right;padding:6px;color:var(--text2)">${usd2(c.cogs)}</td>
        <td style="text-align:right;padding:6px;font-weight:600;color:${clr(c.gross_profit)}">${usd2(c.gross_profit)}</td>
        <td style="text-align:right;padding:6px;color:${clr(c.margin_pct)}">${pct(c.margin_pct)}</td>
      </tr>`).join('')}</tbody>
    </table>` : '<p style="color:var(--text2);font-size:13px;padding:16px">No sales data yet</p>'}
  </div>
</div>`;
}

return {
    renderInventory,  loadInventory,
    renderListings,
    renderSales,      loadSales,
    renderAppraisals,
    renderMarket,     loadMarket,
    renderUsers,
    renderPnl,        loadPnl
};
})();

// -----------------------------------------------------------------------
// Portfolio -- inventory value by category
// -----------------------------------------------------------------------
async function renderPortfolio(el) {
    el.innerHTML = '<div class="dash-loading">Loading portfolio\u2026</div>';
    function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:0,maximumFractionDigits:0}); } catch(e) { return CurrencySymbol+n.toFixed(0); } }
    function usd2(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+n.toFixed(2); } }
    try {
        const [portR, statsR, roiR] = await Promise.all([
            Api.get('/api/reports/portfolio'),
            Api.get('/api/items/stats'),
            Api.get('/api/reports/roi_by_source')
        ]);
        const rows  = Array.isArray(portR.data) ? portR.data : (portR.data||[]);
        const stats = statsR.data || {};
        const roi   = Array.isArray(roiR.data) ? roiR.data : [];

        const totalAsk  = rows.reduce((s,r)=>s+(parseFloat(r.asking_value)||0), 0);
        const totalCost = rows.reduce((s,r)=>s+(parseFloat(r.cost_value)||0), 0);
        const totalGain = totalAsk - totalCost;
        const gainPct   = totalCost > 0 ? ((totalGain/totalCost)*100).toFixed(1) : '—';

        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header"><h2 class="dash-title">&#x1F4BC; Inventory Portfolio</h2></div>
  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-value">${stats.inventory||0}</div><div class="kpi-label">Items in stock</div></div>
    <div class="kpi-card accent"><div class="kpi-value">${usd(totalAsk)}</div><div class="kpi-label">Total asking value</div></div>
    <div class="kpi-card"><div class="kpi-value">${usd(totalCost)}</div><div class="kpi-label">Total cost basis</div></div>
    <div class="kpi-card"><div class="kpi-value" style="color:var(--green)">${usd(totalGain)}</div><div class="kpi-label">Unrealised gain</div></div>
    <div class="kpi-card"><div class="kpi-value">${gainPct}%</div><div class="kpi-label">Markup %</div></div>
  </div>

  <div class="dash-section-label">By category</div>
  <div class="table-wrap"><table>
    <thead><tr><th>Category</th><th>Items</th><th>Asking value</th><th>Cost basis</th><th>Unrealised gain</th><th>Markup %</th></tr></thead>
    <tbody>
    ${rows.length ? rows.map(r => {
      const pct = parseFloat(r.cost_value)>0 ? ((parseFloat(r.unrealized_gain)/parseFloat(r.cost_value))*100).toFixed(1)+'%' : '—';
      return `<tr>
        <td><strong>${esc(r.category)}</strong></td>
        <td>${r.count}</td>
        <td>${usd(r.asking_value)}</td>
        <td>${usd(r.cost_value)}</td>
        <td style="color:var(--green)">${usd(r.unrealized_gain)}</td>
        <td>${pct}</td>
      </tr>`;
    }).join('') : '<tr><td colspan="6" class="empty-state">No inventory items</td></tr>'}
    </tbody>
  </table></div>

  ${roi.length ? `
  <div class="dash-section-label" style="margin-top:24px">ROI by acquisition source</div>
  <div class="table-wrap"><table>
    <thead><tr><th>Source</th><th>Items sold</th><th>Total revenue</th><th>Total cost</th><th>Avg ROI %</th></tr></thead>
    <tbody>${roi.map(r=>`<tr>
      <td>${esc(r.source||r[0]||'Unknown')}</td>
      <td>${r.count||r[1]||0}</td>
      <td>${usd(r.revenue||r[3]||0)}</td>
      <td>${usd(r.cost||r[4]||0)}</td>
      <td style="color:var(--green)">${parseFloat(r.avg_roi||r[2]||0).toFixed(1)}%</td>
    </tr>`).join('')}</tbody>
  </table></div>` : ''}
</div>`;
    } catch(e) { el.innerHTML = `<div class="dash-error">Error loading portfolio: ${e}</div>`; }
}

// -----------------------------------------------------------------------
// Consignor statement -- printable HTML per consignor
// -----------------------------------------------------------------------
async function renderConsignorStatement(consignorName) {
    const r = await Api.get('/api/consignments');
    const all = Array.isArray(r.data) ? r.data : [];
    const items = all.filter(c => c.consignor_name === consignorName);
    if (!items.length) { alert('No consignment records found for ' + consignorName); return; }
    function usd2(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+n.toFixed(2); } }
    const totalSales  = items.reduce((s,c)=>s+(parseFloat(c.sale_price)||0),0);
    const totalPayout = items.reduce((s,c)=>s+(parseFloat(c.payout_amount)||0),0);
    const unpaid      = items.filter(c=>c.status==='sold'&&!c.payout_date).reduce((s,c)=>s+(parseFloat(c.payout_amount)||0),0);
    const html = `<!DOCTYPE html><html><head><meta charset="UTF-8">
<title>Consignor Statement — ${consignorName}</title>
<style>
body{font-family:Arial,sans-serif;max-width:700px;margin:40px auto;color:#222;font-size:13px}
h1{color:#C8960C;font-size:20px}h2{font-size:13px;color:#666;font-weight:400;margin-top:0}
table{width:100%;border-collapse:collapse;margin:16px 0}
th{background:#f5f5f5;text-align:left;padding:7px 10px;font-size:12px;border-bottom:2px solid #ddd}
td{padding:7px 10px;border-bottom:1px solid #eee;font-size:12px}
.total{font-weight:700;font-size:14px}.owing{color:#c62828;font-weight:700}
.paid{color:#2e7d32}.badge{border-radius:12px;padding:2px 8px;font-size:11px}
.footer{margin-top:32px;font-size:11px;color:#999;border-top:1px solid #eee;padding-top:12px}
@media print{body{margin:10px}}
</style></head><body>
<h1>Consignor Statement</h1>
<h2>Prepared for: <strong>${consignorName}</strong> &mdash; ${new Date().toLocaleDateString()}</h2>
<table>
<thead><tr><th>Description</th><th>Received</th><th>Status</th><th>Sale price</th><th>Your %</th><th>Payout</th><th>Paid</th></tr></thead>
<tbody>${items.map(c=>`<tr>
<td>${c.description||c.id}</td>
<td>${(c.received_date||'').slice(0,10)}</td>
<td><span class="badge" style="background:${c.status==='paid'?'#e8f5e9':c.status==='sold'?'#fff3e0':'#e3f2fd'}">${c.status}</span></td>
<td>${c.sale_price?usd2(c.sale_price):'—'}</td>
<td>${parseFloat(c.agreed_pct)||0}%</td>
<td>${c.payout_amount?usd2(c.payout_amount):'—'}</td>
<td>${c.payout_date?`<span class="paid">&#x2713; ${c.payout_date.slice(0,10)}</span>`:'<span style="color:#e65100">Pending</span>'}</td>
</tr>`).join('')}
</tbody></table>
<table style="width:auto;margin-left:auto">
<tr><td style="text-align:right;padding:6px 16px">Total sales:</td><td class="total">${usd2(totalSales)}</td></tr>
<tr><td style="text-align:right;padding:6px 16px">Total payout earned:</td><td class="total">${usd2(totalPayout)}</td></tr>
<tr><td style="text-align:right;padding:6px 16px;color:#c62828">Amount owing:</td><td class="owing">${usd2(unpaid)}</td></tr>
</table>
<div class="footer">Generated by ${document.title||'Metis Antique Marketplace Plus'} &mdash; ${new Date().toISOString().slice(0,10)}</div>
</body></html>`;
    const w = window.open('','_blank');
    w.document.write(html);
    w.document.close();
    w.print();
}
