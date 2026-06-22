// Metis Antique Marketplace Plus - Purchases and Expenses pages

// ================================================================
// PURCHASES PAGE
// ================================================================
Pages.renderPurchases = async function(el) {
    function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+Math.abs(n).toFixed(2); } }
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

    const [sumR, purchR] = await Promise.all([
        Api.get('/api/purchases/summary'),
        Api.get('/api/purchases')
    ]);
    const s = sumR.data || {};
    const purchases = Array.isArray(purchR.data) ? purchR.data : [];
    const byChan = s.by_channel || [];
    const maxChan = Math.max(...byChan.map(c => parseFloat(c.total)||0), 1);

    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Purchases</h1>
  <button class="btn" onclick="PurchaseUI.openAdd()">+ Record purchase</button>
</div>

<div class="stat-grid">
  <div class="stat-card"><div class="stat-label">Items acquired</div><div class="stat-value">${esc(s.total_purchases||0)}</div></div>
  <div class="stat-card"><div class="stat-label">Total cost basis</div><div class="stat-value">${usd(s.total_cost)}</div></div>
  <div class="stat-card"><div class="stat-label">Purchase price</div><div class="stat-value">${usd(s.total_purchase_price)}</div></div>
  <div class="stat-card"><div class="stat-label">Buyer's premiums</div><div class="stat-value amber">${usd(s.total_buyers_premium)}</div></div>
  <div class="stat-card"><div class="stat-label">Transport</div><div class="stat-value">${usd(s.total_transport)}</div></div>
  <div class="stat-card"><div class="stat-label">Restoration</div><div class="stat-value">${usd(s.total_restoration)}</div></div>
  <div class="stat-card"><div class="stat-label">This month</div><div class="stat-value">${usd(s.month_cost)}</div></div>
  <div class="stat-card"><div class="stat-label">This month (count)</div><div class="stat-value">${esc(s.month_purchases||0)}</div></div>
</div>

${byChan.length ? `<div class="chart-card" style="margin-bottom:18px">
  <h3>Spend by acquisition channel</h3>
  ${byChan.map(c => {
      const pct = Math.round((parseFloat(c.total)||0)/maxChan*100);
      return `<div class="bar-row">
    <div class="bar-label" title="${esc(c.channel)}">${esc(c.channel||'Unknown')}</div>
    <div class="bar-track"><div class="bar-fill" style="width:${pct}%"></div></div>
    <div class="bar-val">${usd(c.total)} <span style="font-size:11px;color:var(--text2)">(${esc(c.count)})</span></div>
  </div>`;}).join('')}
</div>` : ''}

<div class="toolbar">
  <input type="date" id="p-from">
  <span style="color:var(--text2);font-size:13px">to</span>
  <input type="date" id="p-to">
  <select id="p-channel">
    <option value="">All channels</option>
    ${['Estate sale','Auction house','Private dealer','Antique market',
       'Online purchase','Trade','Consignment','Other'].map(c =>
       `<option>${esc(c)}</option>`).join('')}
  </select>
  <button class="btn" onclick="PurchaseUI.loadPurchases()">Filter</button>
  <button class="btn-ghost" onclick="document.getElementById('p-from').value='';document.getElementById('p-to').value='';document.getElementById('p-channel').value='';PurchaseUI.loadPurchases()">Clear</button>
</div>
<div id="purchases-table">${renderPurchasesTable(purchases)}</div>`;
};

function renderPurchasesTable(rows) {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+Math.abs(n).toFixed(2); } }
    if (!rows.length)
        return '<div class="empty-state"><div class="empty-icon">&#x1F6CD;</div><p>No purchases recorded yet.<br>Click <strong>+ Record purchase</strong> to log your first acquisition.</p></div>';
    return `<div class="table-wrap"><table>
<thead><tr>
  <th>Item</th><th>Vendor</th><th>Date</th><th>Channel</th>
  <th style="text-align:right">Purchase price</th>
  <th style="text-align:right">Premium</th>
  <th style="text-align:right">Transport</th>
  <th style="text-align:right">Total cost</th>
  <th>Payment</th><th>Receipt</th><th></th>
</tr></thead>
<tbody>${rows.map(p => `
<tr>
  <td>
    <strong style="cursor:pointer;color:var(--accent)" onclick="app.openItemDetail(${p.item_id})">${esc(p.item_title)}</strong>
  </td>
  <td>
    ${p.vendor_name ? `<strong>${esc(p.vendor_name)}</strong>` : '<span style="color:var(--text2)">—</span>'}
    ${p.vendor_contact ? `<br><span style="font-size:11px;color:var(--text2)">${esc(p.vendor_contact)}</span>` : ''}
  </td>
  <td style="white-space:nowrap">${esc((p.acquisition_date||'').slice(0,10))}</td>
  <td><span class="badge badge-gray" style="font-size:11px">${esc(p.purchase_channel||'—')}</span></td>
  <td style="text-align:right">${usd(p.purchase_price)}</td>
  <td style="text-align:right">${parseFloat(p.buyers_premium||0)>0
    ? `<span style="color:var(--amber)">${usd(p.buyers_premium)}</span>` : '—'}</td>
  <td style="text-align:right">${parseFloat(p.transport_cost||0)>0 ? usd(p.transport_cost) : '—'}</td>
  <td style="text-align:right"><strong style="color:var(--accent)">${usd(p.total_cost)}</strong></td>
  <td style="font-size:12px">${esc(p.payment_method||'—')}</td>
  <td style="font-size:11px;color:var(--text2)">${esc(p.receipt_ref||'—')}</td>
  <td style="white-space:nowrap">
    <button class="btn-ghost" onclick="PurchaseUI.openEdit(${p.id})">Edit</button>
    <button class="btn-danger" onclick="PurchaseUI.deletePurchase(${p.id},'${esc(p.item_title)}')">&#x2715;</button>
  </td>
</tr>`).join('')}</tbody></table></div>`;
}

const PurchaseUI = (() => {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

    const CHANNELS = ['Estate sale','Auction house','Private dealer','Antique market',
                      'Online purchase','Trade','Consignment','Other'];
    const METHODS  = ['Cash','Check','PayPal','Venmo','Bank transfer','Credit card','Wire','Other'];

    async function loadPurchases() {
        const from = document.getElementById('p-from')?.value||'';
        const to   = document.getElementById('p-to')?.value||'';
        const ch   = document.getElementById('p-channel')?.value||'';
        let url = '/api/purchases';
        const qs = [];
        if (from) qs.push('from='+from);
        if (to)   qs.push('to='+to);
        if (ch)   qs.push('channel='+encodeURIComponent(ch));
        if (qs.length) url += '?' + qs.join('&');
        const r = await Api.get(url);
        const wrap = document.getElementById('purchases-table');
        if (wrap) wrap.innerHTML = renderPurchasesTable(Array.isArray(r.data)?r.data:[]);
    }

    // Fetch inventory items for the item picker
    async function getInventoryItems() {
        const r = await Api.get('/api/items?status=inventory&limit=200');
        return (r.data?.items || []).map(i => ({ id: i.id, label: i.title + (i.maker?' — '+i.maker:'') }));
    }

    function costFields(p) {
        return `
<div class="form-group"><label>Purchase price ($)</label>
  <input type="number" id="pf-price" value="${p.purchase_price||0}" min="0" step="0.01" oninput="PurchaseUI.recalc()"></div>
<div class="form-group"><label>Buyer's premium ($) <span style="font-size:11px;color:var(--text2)">auction add-on</span></label>
  <input type="number" id="pf-premium" value="${p.buyers_premium||0}" min="0" step="0.01" oninput="PurchaseUI.recalc()"></div>
<div class="form-group"><label>Transport cost ($)</label>
  <input type="number" id="pf-transport" value="${p.transport_cost||0}" min="0" step="0.01" oninput="PurchaseUI.recalc()"></div>
<div class="form-group"><label>Restoration budget ($)</label>
  <input type="number" id="pf-restore" value="${p.restoration_budget||0}" min="0" step="0.01" oninput="PurchaseUI.recalc()"></div>
<div class="form-group full" style="background:var(--bg3);border-radius:6px;padding:12px">
  <div style="display:flex;justify-content:space-between;align-items:center">
    <div>
      <span style="font-size:13px;color:var(--text2)">Total cost basis</span>
      <div style="font-size:11px;color:var(--text2);margin-top:2px">Synced to item cost price for margin calculations</div>
    </div>
    <span id="pf-total-display" style="font-size:22px;font-weight:700;color:var(--accent)">$${parseFloat(p.total_cost||0).toFixed(2)}</span>
  </div>
</div>`;
    }

    async function openAdd() {
        document.getElementById('modal-title').textContent = 'Record purchase';
        document.getElementById('modal-body').innerHTML =
            '<p style="color:var(--text2);padding:20px">Loading inventory...</p>';
        document.getElementById('modal').classList.remove('hidden');
        const items = await getInventoryItems();
        document.getElementById('modal-body').innerHTML = `
<div class="form-grid">
  <div class="form-group full">
    <label>Item * <span style="font-size:11px;color:var(--text2)">(items in stock)</span></label>
    <select id="pf-item">
      <option value="">— select item —</option>
      ${items.map(i => `<option value="${i.id}">${esc(i.label)}</option>`).join('')}
    </select>
  </div>
  <div class="form-group"><label>Vendor name</label><input type="text" id="pf-vendor" placeholder="Seller, estate, auction house..."></div>
  <div class="form-group"><label>Vendor contact</label><input type="text" id="pf-contact" placeholder="Phone, email, address..."></div>
  <div class="form-group"><label>Acquisition date *</label><input type="date" id="pf-date" value="${new Date().toISOString().slice(0,10)}"></div>
  <div class="form-group"><label>Channel</label>
    <select id="pf-channel">
      <option value="">— select —</option>
      ${CHANNELS.map(c => `<option>${esc(c)}</option>`).join('')}
    </select>
  </div>
  ${costFields({})}
  <div class="form-group"><label>Payment method</label>
    <select id="pf-method"><option value="">— select —</option>${METHODS.map(m=>`<option>${esc(m)}</option>`).join('')}</select>
  </div>
  <div class="form-group"><label>Receipt / ref #</label><input type="text" id="pf-receipt"></div>
  <div class="form-group full"><label>Notes</label><textarea id="pf-notes" rows="2" placeholder="Lot number, condition at purchase, provenance notes..."></textarea></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="PurchaseUI.save(0)">Record purchase</button>
</div>`;
    }

    // Called from item detail Purchase tab — pre-selects the item
    async function openAddForItem(itemId) {
        app.closeModal();
        await openAdd();
        // Wait for DOM then set item
        setTimeout(() => {
            const sel = document.getElementById('pf-item');
            if (sel) sel.value = itemId;
        }, 50);
    }

    async function openEdit(id) {
        document.getElementById('modal-title').textContent = 'Edit purchase';
        document.getElementById('modal-body').innerHTML =
            '<p style="color:var(--text2);padding:20px">Loading...</p>';
        document.getElementById('modal').classList.remove('hidden');
        const r = await Api.get('/api/purchases/' + id);
        if (!r.ok) { alert('Error loading purchase'); return; }
        const p = r.data;
        document.getElementById('modal-body').innerHTML = `
<div style="background:var(--bg3);border-radius:6px;padding:10px 14px;margin-bottom:14px;font-size:13px">
  <strong>Item:</strong> ${esc(p.item_title)}
</div>
<div class="form-grid">
  <div class="form-group"><label>Vendor name</label><input type="text" id="pf-vendor" value="${esc(p.vendor_name||'')}"></div>
  <div class="form-group"><label>Vendor contact</label><input type="text" id="pf-contact" value="${esc(p.vendor_contact||'')}"></div>
  <div class="form-group"><label>Acquisition date</label><input type="date" id="pf-date" value="${esc((p.acquisition_date||'').slice(0,10))}"></div>
  <div class="form-group"><label>Channel</label>
    <select id="pf-channel">
      <option value="">— select —</option>
      ${CHANNELS.map(c=>`<option value="${esc(c)}"${p.purchase_channel===c?' selected':''}>${esc(c)}</option>`).join('')}
    </select>
  </div>
  ${costFields(p)}
  <div class="form-group"><label>Payment method</label>
    <select id="pf-method"><option value="">— select —</option>
      ${METHODS.map(m=>`<option value="${esc(m)}"${p.payment_method===m?' selected':''}>${esc(m)}</option>`).join('')}
    </select>
  </div>
  <div class="form-group"><label>Receipt / ref #</label><input type="text" id="pf-receipt" value="${esc(p.receipt_ref||'')}"></div>
  <div class="form-group full"><label>Notes</label><textarea id="pf-notes" rows="2">${esc(p.notes||'')}</textarea></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="PurchaseUI.save(${id})">Save changes</button>
</div>`;
    }

    // Called from item detail tab Purchase → Edit button
    async function openEditFromDetail(id) {
        // Fetch purchase to know which item to return to
        const pr = await Api.get('/api/purchases/' + id);
        window._purchaseReturnItemId = pr.ok ? pr.data.item_id : null;
        app.closeModal();
        await openEdit(id);
    }

    function recalc() {
        const pp  = parseFloat(document.getElementById('pf-price')?.value)||0;
        const bp  = parseFloat(document.getElementById('pf-premium')?.value)||0;
        const tc  = parseFloat(document.getElementById('pf-transport')?.value)||0;
        const rb  = parseFloat(document.getElementById('pf-restore')?.value)||0;
        const tot = pp + bp + tc + rb;
        const el  = document.getElementById('pf-total-display');
        if (el) { try { el.textContent = tot.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { el.textContent = CurrencySymbol + tot.toFixed(2); } }
    }

    async function save(id) {
        const itemSel = document.getElementById('pf-item');
        const body = {
            vendor_name:       document.getElementById('pf-vendor')?.value.trim()||'',
            vendor_contact:    document.getElementById('pf-contact')?.value.trim()||'',
            acquisition_date:  document.getElementById('pf-date')?.value||'',
            purchase_channel:  document.getElementById('pf-channel')?.value||'',
            purchase_price:    parseFloat(document.getElementById('pf-price')?.value)||0,
            buyers_premium:    parseFloat(document.getElementById('pf-premium')?.value)||0,
            transport_cost:    parseFloat(document.getElementById('pf-transport')?.value)||0,
            restoration_budget:parseFloat(document.getElementById('pf-restore')?.value)||0,
            payment_method:    document.getElementById('pf-method')?.value||'',
            receipt_ref:       document.getElementById('pf-receipt')?.value.trim()||'',
            notes:             document.getElementById('pf-notes')?.value.trim()||''
        };
        if (!id) {
            body.item_id = parseInt(itemSel?.value)||0;
            if (!body.item_id) { alert('Please select an item'); return; }
        }
        const r = id ? await Api.put('/api/purchases/'+id, body)
                     : await Api.post('/api/purchases', body);
        if (!r.ok) { alert('Error: '+(r.data?.error||'save failed')); return; }
        app.closeModal();
        const returnId = window._purchaseReturnItemId;
        window._purchaseReturnItemId = null;
        if (returnId) {
            app.openItemDetail(returnId);
        } else {
            const cur = document.querySelector('.nav-item.active')?.dataset?.page;
            if (cur) app.nav(cur); else app.nav('purchases');
        }
    }

    async function deletePurchase(id, title) {
        if (!confirm('Delete purchase record for "' + title + '"?\nThis will NOT delete the item.')) return;
        const r = await Api.del('/api/purchases/' + id);
        if (r.ok) loadPurchases();
        else alert('Error: '+(r.data?.error||'delete failed'));
    }

    return { openAdd, openAddForItem, openEdit, openEditFromDetail, save,
             deletePurchase, loadPurchases, recalc };
})();


// ================================================================
// EXPENSES PAGE
// ================================================================
Pages.renderExpenses = async function(el) {
    function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+Math.abs(n).toFixed(2); } }
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

    const CATS = ['Show fees','Storage','Insurance','Shipping supplies','Photography',
                  'Restoration','Marketing','Travel','Office','Software','Other'];

    const [sumR, expR] = await Promise.all([
        Api.get('/api/expenses/summary'),
        Api.get('/api/expenses')
    ]);
    const s    = sumR.data || {};
    const exps = Array.isArray(expR.data) ? expR.data : [];
    const byCat = s.by_category || [];
    const maxAmt = Math.max(...byCat.map(c => parseFloat(c.total)||0), 1);

    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Expenses</h1>
  <button class="btn" onclick="ExpenseUI.openAdd()">+ Record expense</button>
</div>
<div class="stat-grid">
  <div class="stat-card"><div class="stat-label">Total expenses</div><div class="stat-value" style="color:var(--red)">${usd(s.total_expenses)}</div></div>
  <div class="stat-card"><div class="stat-label">Count</div><div class="stat-value">${esc(s.expense_count||0)}</div></div>
  <div class="stat-card"><div class="stat-label">This month</div><div class="stat-value" style="color:var(--red)">${usd(s.month_expenses)}</div></div>
</div>
${byCat.length ? `<div class="chart-card" style="margin-bottom:18px">
  <h3>By category</h3>
  ${byCat.map(c => {
      const pct = Math.round((parseFloat(c.total)||0)/maxAmt*100);
      return `<div class="bar-row">
    <div class="bar-label" title="${esc(c.category)}">${esc(c.category)}</div>
    <div class="bar-track"><div class="bar-fill" style="background:var(--red);width:${pct}%"></div></div>
    <div class="bar-val">${usd(c.total)} <span style="font-size:11px;color:var(--text2)">(${esc(c.count)})</span></div>
  </div>`;}).join('')}
</div>` : ''}
<div class="toolbar">
  <select id="exp-cat-filter" onchange="ExpenseUI.loadExpenses()">
    <option value="">All categories</option>
    ${CATS.map(c => `<option>${esc(c)}</option>`).join('')}
  </select>
  <input type="date" id="exp-from" onchange="ExpenseUI.loadExpenses()">
  <span style="color:var(--text2);font-size:13px">to</span>
  <input type="date" id="exp-to" onchange="ExpenseUI.loadExpenses()">
  <button class="btn-ghost" onclick="document.getElementById('exp-cat-filter').value='';document.getElementById('exp-from').value='';document.getElementById('exp-to').value='';ExpenseUI.loadExpenses()">Clear</button>
</div>
<div id="expenses-table">${renderExpensesTable(exps)}</div>`;

    window._expenseCats = CATS;
};

function renderExpensesTable(rows) {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+Math.abs(n).toFixed(2); } }
    if (!rows.length)
        return '<div class="empty-state"><div class="empty-icon">&#x1F4B8;</div><p>No expenses recorded yet</p></div>';
    return `<div class="table-wrap"><table>
<thead><tr>
  <th>Category</th><th>Description</th><th>Date</th>
  <th>Vendor</th><th style="text-align:right">Amount</th>
  <th>Payment</th><th>Receipt</th><th></th>
</tr></thead>
<tbody>${rows.map(e => `
<tr>
  <td><span class="badge badge-gray">${esc(e.category)}</span></td>
  <td>${esc(e.description)}</td>
  <td style="white-space:nowrap">${esc((e.expense_date||'').slice(0,10))}</td>
  <td style="font-size:12px">${esc(e.vendor||'—')}</td>
  <td style="text-align:right;font-weight:500;color:var(--red)">${usd(e.amount)}</td>
  <td style="font-size:12px">${esc(e.payment_method||'—')}</td>
  <td style="font-size:11px;color:var(--text2)">${esc(e.receipt_ref||'—')}</td>
  <td style="white-space:nowrap">
    <button class="btn-ghost" onclick="ExpenseUI.openEdit(${e.id})">Edit</button>
    <button class="btn-danger" onclick="ExpenseUI.deleteExpense(${e.id})">&#x2715;</button>
  </td>
</tr>`).join('')}</tbody></table></div>`;
}

const ExpenseUI = (() => {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    const CATS    = ['Show fees','Storage','Insurance','Shipping supplies','Photography',
                     'Restoration','Marketing','Travel','Office','Software','Other'];
    const METHODS = ['Cash','Check','PayPal','Venmo','Bank transfer','Credit card','Wire','Other'];

    async function loadExpenses() {
        const cat  = document.getElementById('exp-cat-filter')?.value||'';
        const from = document.getElementById('exp-from')?.value||'';
        const to   = document.getElementById('exp-to')?.value||'';
        let url = '/api/expenses';
        const qs = [];
        if (cat)  qs.push('category='+encodeURIComponent(cat));
        if (from) qs.push('from='+from);
        if (to)   qs.push('to='+to);
        if (qs.length) url += '?' + qs.join('&');
        const r = await Api.get(url);
        const wrap = document.getElementById('expenses-table');
        if (wrap) wrap.innerHTML = renderExpensesTable(Array.isArray(r.data)?r.data:[]);
    }

    function expenseForm(e) {
        return `<div class="form-grid">
<div class="form-group"><label>Category *</label>
  <select id="ef-cat">
    <option value="">— select —</option>
    ${CATS.map(c=>`<option value="${esc(c)}"${e.category===c?' selected':''}>${esc(c)}</option>`).join('')}
  </select>
</div>
<div class="form-group"><label>Amount ($) *</label>
  <input type="number" id="ef-amount" value="${e.amount||''}" min="0.01" step="0.01">
</div>
<div class="form-group full"><label>Description *</label>
  <input type="text" id="ef-desc" value="${esc(e.description||'')}" placeholder="What was this expense for?">
</div>
<div class="form-group"><label>Expense date</label>
  <input type="date" id="ef-date" value="${esc(e.expense_date||new Date().toISOString().slice(0,10))}">
</div>
<div class="form-group"><label>Vendor / payee</label>
  <input type="text" id="ef-vendor" value="${esc(e.vendor||'')}">
</div>
<div class="form-group"><label>Payment method</label>
  <select id="ef-method">
    <option value="">— select —</option>
    ${METHODS.map(m=>`<option value="${esc(m)}"${e.payment_method===m?' selected':''}>${esc(m)}</option>`).join('')}
  </select>
</div>
<div class="form-group"><label>Receipt / ref #</label>
  <input type="text" id="ef-receipt" value="${esc(e.receipt_ref||'')}">
</div>
<div class="form-group full"><label>Notes</label>
  <textarea id="ef-notes" rows="2">${esc(e.notes||'')}</textarea>
</div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="ExpenseUI.save(${e.id||0})">${e.id?'Save changes':'Record expense'}</button>
</div>`;
    }

    function openAdd() {
        document.getElementById('modal-title').textContent = 'Record expense';
        document.getElementById('modal-body').innerHTML = expenseForm({});
        document.getElementById('modal').classList.remove('hidden');
    }

    async function openEdit(id) {
        document.getElementById('modal-title').textContent = 'Edit expense';
        document.getElementById('modal-body').innerHTML =
            '<p style="color:var(--text2);padding:20px">Loading...</p>';
        document.getElementById('modal').classList.remove('hidden');
        // Use dedicated GET /api/expenses/:id endpoint
        const r = await Api.get('/api/expenses/' + id);
        if (!r.ok) { alert('Error loading expense'); return; }
        document.getElementById('modal-body').innerHTML = expenseForm(r.data);
    }

    async function save(id) {
        const body = {
            category:       document.getElementById('ef-cat')?.value||'',
            description:    document.getElementById('ef-desc')?.value.trim()||'',
            amount:         parseFloat(document.getElementById('ef-amount')?.value)||0,
            expense_date:   document.getElementById('ef-date')?.value||'',
            vendor:         document.getElementById('ef-vendor')?.value.trim()||'',
            payment_method: document.getElementById('ef-method')?.value||'',
            receipt_ref:    document.getElementById('ef-receipt')?.value.trim()||'',
            notes:          document.getElementById('ef-notes')?.value.trim()||''
        };
        if (!body.category || !body.description || !body.amount) {
            alert('Category, description, and amount are required'); return;
        }
        const r = id ? await Api.put('/api/expenses/'+id, body)
                     : await Api.post('/api/expenses', body);
        if (!r.ok) { alert('Error: '+(r.data?.error||'save failed')); return; }
        app.closeModal();
        app.nav('expenses');
    }

    async function deleteExpense(id) {
        if (!confirm('Delete this expense record?')) return;
        const r = await Api.del('/api/expenses/'+id);
        if (r.ok) loadExpenses();
        else alert('Error: '+(r.data?.error||'delete failed'));
    }

    return { openAdd, openEdit, save, deleteExpense, loadExpenses };
})();
