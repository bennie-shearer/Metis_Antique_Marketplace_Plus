// Metis Antique Marketplace Plus v1.2.48 - Application controller
const app = (() => {
    let currentPage = 'inventory';
    let currentRole = '';

    function esc(s) {
        return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    }

    async function init() {
        // Dark/light mode from localStorage
        const theme = localStorage.getItem('map_theme');
        if (theme === 'light') document.documentElement.classList.add('light');
        const initLabel = (theme === 'light') ? '☽ Dark' : '☼ Light';
        document.querySelectorAll('.pill-theme').forEach(b => b.textContent = initLabel);

        if (Api.hasToken()) {
            const r = await Api.get('/api/auth/me');
            if (r.ok) { showApp(r.data.username, r.data.role); return; }
            Api.clearToken();
        }
        showLogin();
    }

    function showLogin() {
        document.getElementById('login-screen').classList.remove('hidden');
        document.getElementById('app-screen').classList.add('hidden');
    }

    function showApp(username, role) {
        currentRole = role || 'dealer';
        window._currentRole = currentRole;
        document.getElementById('login-screen').classList.add('hidden');
        document.getElementById('app-screen').classList.remove('hidden');
        document.getElementById('nav-user').textContent = username;
        const roleEl = document.getElementById('nav-role');
        roleEl.textContent = role;
        roleEl.className = 'nav-role role-' + role;
        document.querySelectorAll('.admin-only').forEach(el =>
            el.classList.toggle('hidden', role !== 'admin'));
        document.querySelectorAll('.dealer-only').forEach(el =>
            el.classList.toggle('hidden', role !== 'dealer'));
        // Dealers land on their personal dashboard; admin on shared dashboard
        nav(role === 'dealer' ? 'dealer-dashboard' : 'dashboard');
        refreshInquiryBadge();
        // Load client-safe config values from server
        Api.getConfig().then(r => {
            if (r.ok && r.data) {
                if (r.data.currency_symbol) CurrencySymbol = r.data.currency_symbol;
                if (r.data.currency_code)   CurrencyCode   = r.data.currency_code;
                if (r.data.currency_locale) CurrencyLocale = r.data.currency_locale;
                // App name from config.pson app.name
                if (r.data.app_name) {
                    const name = r.data.app_name;
                    document.title = name;
                    const el = document.getElementById('sidebar-app-name');
                    if (el) el.textContent = name;
                    const sub = document.getElementById('dash-app-subtitle');
                    if (sub) sub.textContent = name;
                    const signout = document.querySelector('.pill-signout');
                    if (signout) signout.title = 'Sign out of ' + name;
                }
                // TLS badges
                if (r.data.tls_enabled) {
                    const wrap   = document.getElementById('tls-badges');
                    const verEl  = document.getElementById('badge-tls-ver');
                    const cipEl  = document.getElementById('badge-tls-cipher');
                    const pqEl   = document.getElementById('badge-pq');
                    if (wrap)  wrap.classList.remove('hidden');
                    if (verEl && r.data.tls_version)
                        verEl.textContent = r.data.tls_version;
                    else if (verEl)
                        verEl.textContent = 'TLS';
                    if (cipEl && r.data.tls_cipher) {
                        // Shorten cipher name: TLS_AES_256_GCM_SHA384 -> AES-256-GCM
                        const short = r.data.tls_cipher
                            .replace(/^TLS_/, '')
                            .replace(/_SHA\d+$/, '')
                            .replace(/_/g, '-');
                        cipEl.textContent = short;
                        cipEl.classList.remove('hidden');
                    } else if (cipEl) {
                        cipEl.classList.add('hidden');
                    }
                    if (pqEl && r.data.post_quantum) pqEl.classList.remove('hidden');
                }
            }
        });
    }

    async function login() {
        const user = document.getElementById('l-user').value.trim();
        const pass = document.getElementById('l-pass').value;
        const err  = document.getElementById('login-err');
        err.classList.add('hidden');
        if (!user || !pass) { err.textContent = 'Username and password required'; err.classList.remove('hidden'); return; }
        const r = await Api.post('/api/auth/login', { username: user, password: pass });
        if (r.ok) { Api.setToken(r.data.token); showApp(r.data.user, r.data.role); }
        else { err.textContent = r.data.error || 'Login failed'; err.classList.remove('hidden'); }
    }

    async function logout() {
        await Api.post('/api/auth/logout', {});
        Api.clearToken();
        currentRole = '';
        showLogin();
    }

    async function nav(page) {
        currentPage = page;
        document.querySelectorAll('.nav-item').forEach(el =>
            el.classList.toggle('active', el.dataset.page === page));
        document.querySelectorAll('.page').forEach(el => el.classList.add('hidden'));
        const el = document.getElementById('page-' + page);
        el.classList.remove('hidden');
        el.innerHTML = '<p style="color:var(--text2);padding:32px">Loading...</p>';

        const writePages = ['inventory','purchases','listings','sales','expenses','appraisals'];
        const showBanner = currentRole === 'viewer' && writePages.includes(page);

        const renders = {
            dashboard:      (el) => Pages.renderDashboard(currentRole),
            'dealer-dashboard': (el) => Pages.renderDealerDashboard(el),
            inventory:      Pages.renderInventory,
            purchases:      Pages.renderPurchases,
            listings:       Pages.renderListings,
            sales:          Pages.renderSales,
            expenses:       Pages.renderExpenses,
            pnl:            Pages.renderPnl,
            appraisals:     Pages.renderAppraisals,
            market:         Pages.renderMarket,
            portfolio:      (el) => Pages.renderPortfolio(el),
            inquiries:      Pages.renderInquiries,
            users:          Pages.renderUsers,
            audit:          Pages.renderAudit,
            consignments:   (el) => Pages.renderConsignments(el, currentRole),
            rentals:        (el) => Pages.renderRentals(el, currentRole),
            advertising:    (el) => Pages.renderAdvertising(el, currentRole),
            ccfees:         (el) => Pages.renderCcFees(el, currentRole),
            settlements:    (el) => Pages.renderSettlements(el, currentRole),
            pos:            (el) => Pages.renderPos(el),
        };
        if (renders[page]) await renders[page](el);
        if (showBanner) {
            const b = document.createElement('div');
            b.className = 'readonly-banner';
            b.textContent = 'View-only account — changes are not permitted.';
            el.insertBefore(b, el.firstChild);
        }
    }

    function openModal(title, bodyHtml) {
        document.getElementById('modal-title').textContent = title;
        document.getElementById('modal-body').innerHTML = bodyHtml;
        document.getElementById('modal').classList.remove('hidden');
    }
    function closeModal() {
        document.getElementById('modal').classList.add('hidden');
        document.getElementById('modal-body').innerHTML = '';
    }

    // -----------------------------------------------------------------------
    // Dark/light toggle
    function toggleTheme() {
        const isLight = document.documentElement.classList.toggle('light');
        localStorage.setItem('map_theme', isLight ? 'light' : 'dark');
        const label = isLight ? '☽ Dark' : '☼ Light';
        document.querySelectorAll('.pill-theme').forEach(b => b.textContent = label);
    }

    // -----------------------------------------------------------------------
    // Item detail view — 5 tabs: Details | Photos | Notes | Inquiries | Audit
    async function openItemDetail(id) {
        const itemR = await Api.get('/api/items/' + id);
        if (!itemR.ok) return;
        const it = itemR.data;
        openModal(it.title, '');

        const fields = [
            ['Title', it.title], ['Category', it.category], ['Era', it.era],
            ['Maker', it.maker], ['Origin', it.origin], ['Material', it.material],
            ['Source', it.source], ['Condition', it.condition], ['Status', it.status],
            ['Cost price', it.cost_price ? CurrencySymbol+it.cost_price : ''],
            ['Asking price', it.asking_price ? CurrencySymbol+it.asking_price : ''],
            ['Consignor', it.consignor_name],
            ['Consignor %', it.consignor_pct ? it.consignor_pct+'%' : '']
        ].filter(([,v]) => v);

        const tabs = [
            ['details',   'Details'],
            ['photos',    'Photos'],
            ['notes',     'Notes'],
            ['purchase',  'Purchase'],
            ['inquiries', 'Inquiries'],
            ['history',   'Audit'],
        ];

        const tabBar = tabs.map(([key, label]) => {
            const cls = 'detail-tab' + (key === 'details' ? ' active' : '');
            const oc  = 'DetailUI.showTab(&quot;' + key + '&quot;,' + id + ')';
            return '<button class="' + cls + '" id="dtab-' + key + '" onclick="' + oc + '">' + label + '</button>';
        }).join('');

        const tabPanels = tabs.map(([key]) => {
            let attrs = 'id="dtab-panel-' + key + '"';
            if (key !== 'details') attrs += ' style="display:none"';
            if (key === 'photos') attrs += ' data-gallery="true"';
            return '<div ' + attrs + '></div>';
        }).join('');

        document.getElementById('modal-body').innerHTML =
            '<div class="detail-tab-bar">' + tabBar + '</div>' +
            tabPanels +
            '<div class="form-actions">' +
            '<button class="btn-ghost" onclick="LabelUI.printLabel(' + id + ')">&#x1F3F7; Print label</button>' +
            '<button class="btn-ghost" onclick="app.closeModal()">Close</button>' +
            '<button class="btn" onclick="app.closeModal();app.openEditItem(' + id + ')">Edit item</button>' +
            '</div>';

        // Render details panel immediately
        const detailsPanel = document.getElementById('dtab-panel-details');
        detailsPanel.innerHTML =
            '<div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px">' +
            fields.map(([k,v]) =>
                '<div><div style="font-size:11px;color:var(--text2);margin-bottom:2px">' + esc(k) +
                '</div><div style="font-size:13px">' + esc(String(v)) + '</div></div>'
            ).join('') +
            (it.description ?
                '<div style="grid-column:1/-1"><div style="font-size:11px;color:var(--text2);margin-bottom:2px">Description</div>' +
                '<div style="font-size:13px;white-space:pre-wrap">' + esc(it.description) + '</div></div>' : '') +
            (it.provenance ?
                '<div style="grid-column:1/-1"><div style="font-size:11px;color:var(--text2);margin-bottom:2px">Provenance</div>' +
                '<div style="font-size:13px;white-space:pre-wrap">' + esc(it.provenance) + '</div></div>' : '') +
            '</div>';
    }

    const DetailUI = (() => {
        async function showTab(tab, itemId) {
            document.querySelectorAll('.detail-tab').forEach(b => {
                b.classList.toggle('active', b.id === 'dtab-' + tab);
            });
            document.querySelectorAll('[id^="dtab-panel-"]').forEach(p => {
                p.style.display = p.id === 'dtab-panel-' + tab ? '' : 'none';
            });

            const panel = document.getElementById('dtab-panel-' + tab);
            if (!panel || panel.dataset.loaded) return;
            panel.dataset.loaded = '1';

            if (tab === 'photos') {
                await Pages.renderGallery(itemId, '', panel);
            }
            if (tab === 'notes') {
                await Pages.renderComments(itemId, panel);
            }
            if (tab === 'purchase') {
                await renderItemPurchase(itemId, panel);
            }
            if (tab === 'inquiries') {
                await renderItemInquiries(itemId, panel);
            }
            if (tab === 'history') {
                await renderItemHistory(itemId, panel);
            }
        }

        async function renderItemPurchase(itemId, panel) {
            const r = await Api.get('/api/items/' + itemId + '/purchase');
            function esc(x) { return String(x??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
            function usd(v) { const n=parseFloat(v)||0; try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+n.toFixed(2); } }
            if (!r.ok) {
                panel.innerHTML =
                    '<div class="empty-state" style="padding:24px">' +
                    '<div class="empty-icon">&#x1F6CD;</div>' +
                    '<p>No purchase record yet</p>' +
                    '<button class="btn" style="margin-top:12px" ' +
                    'onclick="PurchaseUI.openAddForItem(' + itemId + ')">Record purchase</button>' +
                    '</div>';
                return;
            }
            const p = r.data;
            const fields = [
                ['Vendor',          p.vendor_name],
                ['Contact',         p.vendor_contact],
                ['Acquired',        (p.acquisition_date||'').slice(0,10)],
                ['Channel',         p.purchase_channel],
                ['Payment',         p.payment_method],
                ['Receipt ref',     p.receipt_ref],
            ].filter(([,v]) => v);
            panel.innerHTML =
                '<div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px">' +
                fields.map(([k,v]) =>
                    '<div><div style="font-size:11px;color:var(--text2);margin-bottom:2px">' + esc(k) +
                    '</div><div style="font-size:13px">' + esc(String(v)) + '</div></div>'
                ).join('') +
                '</div>' +
                '<div style="background:var(--bg3);border-radius:6px;padding:14px;margin-bottom:14px">' +
                '<div style="display:grid;grid-template-columns:repeat(4,1fr);gap:10px">' +
                [['Purchase price', p.purchase_price],
                 ["Buyer's premium", p.buyers_premium],
                 ['Transport', p.transport_cost],
                 ['Restoration', p.restoration_budget]
                ].map(([k,v]) =>
                    '<div><div style="font-size:11px;color:var(--text2)">' + k + '</div>' +
                    '<div style="font-size:14px;font-weight:500">' + usd(v) + '</div></div>'
                ).join('') +
                '</div>' +
                '<div style="border-top:1px solid var(--border);margin-top:10px;padding-top:10px;' +
                'display:flex;justify-content:space-between;align-items:center">' +
                '<span style="font-size:12px;color:var(--text2)">Total cost (cost basis)</span>' +
                '<span style="font-size:20px;font-weight:700;color:var(--accent)">' + usd(p.total_cost) + '</span>' +
                '</div></div>' +
                (p.notes ? '<p style="font-size:13px;color:var(--text2);white-space:pre-wrap">' + esc(p.notes) + '</p>' : '') +
                '<div class="form-actions">' +
                '<button class="btn-ghost" onclick="PurchaseUI.openEditFromDetail(' + p.id + ')">Edit purchase</button>' +
                '</div>';
        }

        async function renderItemInquiries(itemId, panel) {
            const r = await Api.get('/api/inquiries?item_id=' + itemId);
            const inqs = Array.isArray(r.data) ? r.data : [];
            function esc(x) { return String(x??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
            const statusColors = { open:'badge-amber', replied:'badge-blue', closed:'badge-gray' };
            panel.innerHTML = inqs.length ? `
<div style="display:flex;flex-direction:column;gap:8px;margin-bottom:12px">
${inqs.map(inq => `
<div style="background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:12px">
  <div style="display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:8px">
    <div>
      <strong style="font-size:13px">${esc(inq.buyer_name)}</strong>
      <span style="font-size:12px;color:var(--text2);margin-left:8px">${esc(inq.buyer_email)}</span>
      ${inq.buyer_phone ? '<span style="font-size:12px;color:var(--text2);margin-left:8px">'+esc(inq.buyer_phone)+'</span>' : ''}
    </div>
    <div style="display:flex;gap:8px;align-items:center">
      <span class="badge ${statusColors[inq.status]||'badge-gray'}">${esc(inq.status)}</span>
      <span style="font-size:11px;color:var(--text2)">${esc((inq.created_at||'').slice(0,10))}</span>
    </div>
  </div>
  ${inq.subject ? '<p style="font-size:12px;color:var(--text2);margin-bottom:4px">'+esc(inq.subject)+'</p>' : ''}
  <p style="font-size:13px;white-space:pre-wrap">${esc(inq.body)}</p>
  <div style="margin-top:10px;display:flex;justify-content:space-between;align-items:center">
    <span style="font-size:12px;color:var(--text2)">${inq.reply_count||0} repl${inq.reply_count==1?'y':'ies'}</span>
    <button class="btn" style="font-size:12px;padding:5px 12px"
      onclick="InquiryUI.openThread(${inq.id})">View thread</button>
  </div>
</div>`).join('')}
</div>` :
'<div class="empty-state" style="padding:32px"><div class="empty-icon">&#x1F4AC;</div><p>No buyer inquiries for this item yet</p></div>';
        }

        async function renderItemHistory(itemId, panel) {
            const r = await Api.get('/api/items/' + itemId + '/history');
            const hist = Array.isArray(r.data) ? r.data : [];
            function esc(x) { return String(x??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
            panel.innerHTML = hist.length ? `
<div class="table-wrap" style="margin-bottom:12px">
  <table>
    <thead><tr><th>Field</th><th>From</th><th>To</th><th>By</th><th>When</th></tr></thead>
    <tbody>${hist.map(h => `
<tr>
  <td style="color:var(--text2)">${esc(h.field)}</td>
  <td style="text-decoration:line-through;color:var(--text2)">${esc(h.old_value||'—')}</td>
  <td style="color:var(--accent2)">${esc(h.new_value||'—')}</td>
  <td>${esc(h.changed_by||'—')}</td>
  <td style="font-size:12px;font-family:monospace">${esc((h.changed_at||'').slice(0,16).replace('T',' '))}</td>
</tr>`).join('')}</tbody>
  </table>
</div>` :
'<div class="empty-state" style="padding:32px"><div class="empty-icon">&#x1F4CB;</div><p>No changes recorded yet</p></div>';
        }

        return { showTab };
    })();

    // -----------------------------------------------------------------------
    // Items
    function openAddItem() { openModal('Add item', itemForm({})); }

    async function openEditItem(id) {
        const r = await Api.get('/api/items/' + id);
        if (!r.ok) return;
        openModal('Edit item', itemForm(r.data));
    }

    function itemForm(it) {
        const cats = ['Furniture','Ceramics','Jewelry','Art & Prints','Silver & Metals','Clocks & Watches','Books & Maps','Other'];
        const conds = ['Excellent','Very Good','Good','Fair','Poor'];
        const sources = ['Estate sale','Auction house','Private dealer','Antique market','Online purchase','Trade','Consignment','Other'];
        return `
<div style="display:flex;gap:12px;margin-bottom:12px;flex-wrap:wrap">
  ${it.id?`<div style="background:var(--bg3);border-radius:6px;padding:8px 12px;font-size:12px;color:var(--text2)">&#x1F4F7; Photos are managed on the <strong>Photos tab</strong> in the item detail view.</div>`:''}
</div>
<div class="form-grid">
  <div class="form-group full"><label>Title *</label><input type="text" id="f-title" value="${esc(it.title||'')}"></div>
  <div class="form-group"><label>Category *</label><select id="f-cat">${cats.map(c=>`<option${it.category===c?' selected':''}>${esc(c)}</option>`).join('')}</select></div>
  <div class="form-group"><label>Era</label><input type="text" id="f-era" placeholder="e.g. c. 1875" value="${esc(it.era||'')}"></div>
  <div class="form-group"><label>Maker / Artist</label><input type="text" id="f-maker" value="${esc(it.maker||'')}"></div>
  <div class="form-group"><label>Origin / Country</label><input type="text" id="f-origin" value="${esc(it.origin||'')}"></div>
  <div class="form-group"><label>Material</label><input type="text" id="f-material" value="${esc(it.material||'')}"></div>
  <div class="form-group"><label>Acquisition source</label><select id="f-source">${['',  ...sources].map(c=>`<option value="${esc(c)}"${it.source===c?' selected':''}>${esc(c)||'— not specified —'}</option>`).join('')}</select></div>
  <div class="form-group"><label>Condition</label><select id="f-condition">${conds.map(c=>`<option${it.condition===c?' selected':''}>${c}</option>`).join('')}</select></div>
  <div class="form-group"><label>Status</label><select id="f-status">${['inventory','listed','sold'].map(c=>`<option value="${c}"${it.status===c?' selected':''}>${c}</option>`).join('')}</select></div>
  <div class="form-group"><label>Cost price (' + CurrencySymbol + ')</label><input type="number" id="f-cost" value="${it.cost_price||0}" min="0" step="0.01"></div>
  <div class="form-group"><label>Asking price (' + CurrencySymbol + ')</label><input type="number" id="f-ask" value="${it.asking_price||0}" min="0" step="0.01"></div>
  <div class="form-group"><label>Consignor name</label><input type="text" id="f-consignor" value="${esc(it.consignor_name||'')}"></div>
  <div class="form-group"><label>Consignor % (of sale)</label><input type="number" id="f-consignor-pct" value="${it.consignor_pct||''}" min="0" max="100" step="0.1"></div>
  <div class="form-group full"><label>Description</label><textarea id="f-desc">${esc(it.description||'')}</textarea></div>
  <div class="form-group full"><label>Provenance notes</label><textarea id="f-prov">${esc(it.provenance||'')}</textarea></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="app.saveItem(${it.id||0})">${it.id?'Save changes':'Add item'}</button>
</div>`;
    }

    async function saveItem(id) {
        const body = {
            title:          document.getElementById('f-title').value.trim(),
            category:       document.getElementById('f-cat').value,
            era:            document.getElementById('f-era').value.trim(),
            maker:          document.getElementById('f-maker').value.trim(),
            origin:         document.getElementById('f-origin').value.trim(),
            material:       document.getElementById('f-material').value.trim(),
            source:         document.getElementById('f-source').value,
            condition:      document.getElementById('f-condition').value,
            status:         document.getElementById('f-status').value,
            cost_price:     parseFloat(document.getElementById('f-cost').value)||0,
            asking_price:   parseFloat(document.getElementById('f-ask').value)||0,
            description:    document.getElementById('f-desc').value.trim(),
            provenance:     document.getElementById('f-prov').value.trim(),
            consignor_name: document.getElementById('f-consignor').value.trim()
        };
        const pct = document.getElementById('f-consignor-pct').value;
        if (pct) body.consignor_pct = parseFloat(pct);
        if (!body.title) { alert('Title is required'); return; }
        const r = id ? await Api.put('/api/items/'+id, body) : await Api.post('/api/items', body);
        if (!r.ok) { alert('Error: '+(r.data.error||'save failed')); return; }
        closeModal();
        nav('inventory');
    }

    async function uploadPhoto(id, file) {
        const fd = new FormData();
        fd.append('photo', file, file.name);
        // Use raw fetch for multipart
        const token = sessionStorage.getItem('map_token')||'';
        await fetch('/api/items/'+id+'/photo', {
            method: 'POST',
            headers: { 'Authorization': 'Bearer '+token },
            body: fd
        });
    }

    async function deletePhoto(id) {
        if (!confirm('Remove photo?')) return;
        await Api.del('/api/items/'+id+'/photo');
        closeModal();
        if (id) openEditItem(id);
    }

    async function deleteItem(id) {
        if (!confirm('Delete this item? This cannot be undone.')) return;
        await Api.del('/api/items/'+id);
        nav('inventory');
    }

    async function duplicateItem(id) {
        const r = await Api.post('/api/items/'+id+'/duplicate', {});
        if (r.ok) { nav('inventory'); }
        else alert('Error: '+(r.data.error||'duplicate failed'));
    }

    function openImport() {
        openModal('Bulk import CSV', `
<p style="font-size:13px;color:var(--text2);margin-bottom:12px">
  CSV must have header row with columns:<br>
  <code style="font-size:12px">title,category,era,maker,origin,material,condition,cost_price,asking_price,provenance</code>
</p>
<p style="font-size:12px;color:var(--text2);margin-bottom:16px">title and category are required. All others optional.</p>
<div class="form-group">
  <label>CSV file</label>
  <input type="file" id="csv-file" accept=".csv,text/csv">
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="app.doImport()">Import</button>
</div>`);
    }

    async function doImport() {
        const f = document.getElementById('csv-file').files[0];
        if (!f) { alert('Select a CSV file'); return; }
        const text = await f.text();
        const token = sessionStorage.getItem('map_token')||'';
        const resp = await fetch('/api/items/import', {
            method: 'POST',
            headers: { 'Content-Type': 'text/plain', 'Authorization': 'Bearer '+token },
            body: text
        });
        const data = await resp.json();
        const msg = `Imported: ${data.imported} items` +
            (data.errors?.length ? `\nErrors:\n${data.errors.join('\n')}` : '');
        alert(msg);
        closeModal();
        nav('inventory');
    }

    // -----------------------------------------------------------------------
    // Listings
    async function openAddListing() {
        openModal('New listing', '<p style="color:var(--text2);padding:12px">Loading items...</p>');
        const ir = await Api.get('/api/items?status=inventory&limit=200');
        const lr = await Api.get('/api/items?status=listed&limit=200');
        const items = [...(ir.data?.items||[]), ...(lr.data?.items||[])];
        const channels = ['1stDibs','eBay','Etsy','Ruby Lane','Chairish','Own website','Auction house'];
        document.getElementById('modal-body').innerHTML =
            '<div class="form-grid">' +
            '<div class="form-group full"><label>Item *</label>' +
            '<select id="l-item"><option value="">— select item —</option>' +
            items.map(i => '<option value="'+i.id+'">'+esc(i.title)+(i.maker?' — '+esc(i.maker):'')+' ('+esc(i.status)+')</option>').join('') +
            '</select></div>' +
            '<div class="form-group"><label>Channel</label><select id="l-channel">' +
            channels.map(ch => '<option>'+ch+'</option>').join('') +
            '</select></div>' +
            '<div class="form-group"><label>Type</label><select id="l-type">' +
            '<option value="fixed">Fixed price</option><option value="auction">Auction</option>' +
            '</select></div>' +
            '<div class="form-group"><label>List price ($) *</label><input type="number" id="l-price" min="0" step="0.01"></div>' +
            '<div class="form-group"><label>Listing URL</label><input type="text" id="l-url" placeholder="https://..."></div>' +
            '<div class="form-group"><label>Auction end date</label><input type="date" id="l-end"></div>' +
            '</div><div class="form-actions">' +
            '<button class="btn-ghost" onclick="app.closeModal()">Cancel</button>' +
            '<button class="btn" onclick="app.saveListing()">Create listing</button>' +
            '</div>';
    }

        async function saveListing() {
        const body = {
            item_id: parseInt(document.getElementById('l-item').value)||0,
            channel: document.getElementById('l-channel').value,
            list_type: document.getElementById('l-type').value,
            list_price: parseFloat(document.getElementById('l-price').value)||0,
            auction_end: document.getElementById('l-end').value,
            listing_url: document.getElementById('l-url').value.trim()
        };
        if (!body.item_id || !body.list_price) { alert('Item ID and price required'); return; }
        const r = await Api.post('/api/listings', body);
        if (r.ok) { closeModal(); nav('listings'); }
        else alert('Error: '+(r.data.error||'save failed'));
    }

    async function openEditListing(id, currentData) {
        const r = await Api.get('/api/listings/' + id);
        const listing = r.ok ? r.data : {};
        const channels = ['1stDibs','eBay','Etsy','Ruby Lane','Chairish','Own website','Auction house'];
        const statuses = ['active','ended','sold','removed'];
        openModal('Edit listing',
            '<div class="form-grid">'
            + '<div class="form-group"><label>List price ($)</label>'
            + '<input type="number" id="el-price" value="'+(listing.list_price||0)+'" min="0" step="0.01"></div>'
            + '<div class="form-group"><label>Channel</label><select id="el-channel">'
            + channels.map(ch => '<option value="'+ch+'"'+(listing.channel===ch?' selected':'')+'>'+ch+'</option>').join('')
            + '</select></div>'
            + '<div class="form-group"><label>Status</label><select id="el-status">'
            + statuses.map(s => '<option value="'+s+'"'+(listing.status===s?' selected':'')+'>'+s+'</option>').join('')
            + '</select></div>'
            + '<div class="form-group"><label>Auction end date</label>'
            + '<input type="date" id="el-end" value="'+(listing.auction_end||'').slice(0,10)+'"></div>'
            + '<div class="form-group full"><label>Listing URL</label>'
            + '<input type="text" id="el-url" value="'+esc(listing.listing_url||'')+'" placeholder="https://..."></div>'
            + '</div><div class="form-actions">'
            + '<button class="btn-ghost" onclick="app.closeModal()">Cancel</button>'
            + '<button class="btn" onclick="app.saveListingEdit('+id+')">Save changes</button>'
            + '</div>');
    }

    async function saveListingEdit(id) {
        const body = {
            list_price:  parseFloat(document.getElementById('el-price')?.value)||0,
            channel:     document.getElementById('el-channel')?.value||'',
            status:      document.getElementById('el-status')?.value||'active',
            auction_end: document.getElementById('el-end')?.value||'',
            listing_url: document.getElementById('el-url')?.value.trim()||''
        };
        const r = await Api.put('/api/listings/'+id, body);
        if (r.ok) { closeModal(); nav('listings'); }
        else alert('Error: '+(r.data?.error||'save failed'));
    }

    async function saveListingUrl(id) {  // kept for backward compat
        await saveListingEdit(id);
    }

    async function updateWatchers(id, val) {
        await Api.put('/api/listings/'+id, { watchers: parseInt(val)||0 });
    }

    async function removeListing(id) {
        if (!confirm('Remove this listing?')) return;
        await Api.del('/api/listings/'+id);
        nav('listings');
    }

    // -----------------------------------------------------------------------
    // Sales
    async function openEditSale(id) {
        const r = await Api.get('/api/sales/' + id);
        if (!r.ok) { alert('Could not load sale'); return; }
        const s = r.data;
        openModal('Edit sale', `<div class="form-grid">
  <div class="form-group full" style="background:var(--bg3);border-radius:6px;padding:10px;font-size:13px">
    <strong>${esc(s.title)}</strong> — sold ${esc((s.sale_date||'').slice(0,10))}
  </div>
  <div class="form-group"><label>Sale price (' + CurrencySymbol + ') *</label><input type="number" id="es-price" value="${s.sale_price||0}" min="0" step="0.01"></div>
  <div class="form-group"><label>Platform fee (' + CurrencySymbol + ')</label><input type="number" id="es-fee" value="${s.platform_fee||0}" min="0" step="0.01"></div>
  <div class="form-group"><label>Shipping cost (' + CurrencySymbol + ')</label><input type="number" id="es-ship" value="${s.shipping_cost||0}" min="0" step="0.01"></div>
  <div class="form-group"><label>Payment method</label><select id="es-pay">
    ${['Cash','Check','PayPal','Venmo','Credit card','Wire','Other'].map(m=>`<option value="${esc(m)}"${s.payment_method===m?' selected':''}>${esc(m)}</option>`).join('')}
  </select></div>
  <div class="form-group"><label>Channel</label><select id="es-channel">
    ${['1stDibs','eBay','Etsy','Ruby Lane','Chairish','Private sale','Auction house'].map(ch=>`<option${s.channel===ch?' selected':''}>${ch}</option>`).join('')}
  </select></div>
  <div class="form-group"><label>Buyer name</label><input type="text" id="es-buyer" value="${esc(s.buyer_name||'')}"></div>
  <div class="form-group"><label>Buyer email</label><input type="email" id="es-email" value="${esc(s.buyer_email||'')}"></div>
  <div class="form-group"><label>Sale date</label><input type="date" id="es-date" value="${esc((s.sale_date||'').slice(0,10))}"></div>
  <div class="form-group full"><label>Notes</label><textarea id="es-notes">${esc(s.notes||'')}</textarea></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="app.saveEditSale(${id})">Save changes</button>
</div>`);
    }

    async function saveEditSale(id) {
        const body = {
            sale_price:     parseFloat(document.getElementById('es-price')?.value)||0,
            platform_fee:   parseFloat(document.getElementById('es-fee')?.value)||0,
            shipping_cost:  parseFloat(document.getElementById('es-ship')?.value)||0,
            payment_method: document.getElementById('es-pay')?.value||'',
            channel:        document.getElementById('es-channel')?.value||'',
            buyer_name:     document.getElementById('es-buyer')?.value.trim()||'',
            buyer_email:    document.getElementById('es-email')?.value.trim()||'',
            sale_date:      document.getElementById('es-date')?.value||'',
            notes:          document.getElementById('es-notes')?.value.trim()||''
        };
        const r = await Api.put('/api/sales/'+id, body);
        if (r.ok) { closeModal(); nav('sales'); }
        else alert('Error: '+(r.data?.error||'save failed'));
    }

    // ── Consignments ──────────────────────────────────────────────────────
    async function openConsignment(id) {
        const title = id ? 'Edit consignment' : 'Add consignment';
        let vals = { consignor_name:'', consignor_email:'', consignor_phone:'',
                     description:'', agreed_pct:0, agreed_floor:0,
                     received_date:'', sold_date:'', sale_price:'',
                     payout_amount:'', payout_date:'', status:'active', notes:'' };
        if (id) { const r = await Api.get('/api/consignments/'+id); if (r.ok && r.data[0]) vals=r.data[0]; }
        openModal(title, `
<div class="form-group"><label>Consignor name *</label><input id="cn-name" value="${escVal(vals.consignor_name)}"></div>
<div class="form-group"><label>Email</label><input type="email" id="cn-email" value="${escVal(vals.consignor_email)}"></div>
<div class="form-group"><label>Phone</label><input id="cn-phone" value="${escVal(vals.consignor_phone)}"></div>
<div class="form-group"><label>Description</label><textarea id="cn-desc">${escVal(vals.description)}</textarea></div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Consignor % *</label><input type="number" id="cn-pct" value="${vals.agreed_pct||0}" step="0.1" min="0" max="100"></div>
<div class="form-group"><label>Floor amount</label><input type="number" id="cn-floor" value="${vals.agreed_floor||0}" step="0.01" min="0"></div>
</div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Received date</label><input type="date" id="cn-received" value="${vals.received_date||''}"></div>
<div class="form-group"><label>Status</label><select id="cn-status">
  <option value="active"${vals.status==='active'?' selected':''}>Active</option>
  <option value="sold"${vals.status==='sold'?' selected':''}>Sold</option>
  <option value="paid"${vals.status==='paid'?' selected':''}>Paid out</option>
  <option value="returned"${vals.status==='returned'?' selected':''}>Returned</option>
</select></div>
</div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Sale price</label><input type="number" id="cn-saleprice" value="${vals.sale_price||''}" step="0.01" min="0"></div>
<div class="form-group"><label>Payout amount</label><input type="number" id="cn-payout" value="${vals.payout_amount||''}" step="0.01" min="0"></div>
</div>
<div class="form-group"><label>Payout date</label><input type="date" id="cn-paydate" value="${vals.payout_date||''}"></div>
<div class="form-group"><label>Notes</label><textarea id="cn-notes">${escVal(vals.notes)}</textarea></div>
<div class="form-actions"><button onclick="app.saveConsignment(${id||''})">Save</button></div>`);
    }
    async function saveConsignment(id) {
        const body = {
            consignor_name:  document.getElementById('cn-name')?.value.trim()||'',
            consignor_email: document.getElementById('cn-email')?.value.trim()||'',
            consignor_phone: document.getElementById('cn-phone')?.value.trim()||'',
            description:     document.getElementById('cn-desc')?.value.trim()||'',
            agreed_pct:      document.getElementById('cn-pct')?.value||'0',
            agreed_floor:    document.getElementById('cn-floor')?.value||'0',
            received_date:   document.getElementById('cn-received')?.value||'',
            status:          document.getElementById('cn-status')?.value||'active',
            sale_price:      document.getElementById('cn-saleprice')?.value||'',
            payout_amount:   document.getElementById('cn-payout')?.value||'',
            payout_date:     document.getElementById('cn-paydate')?.value||'',
            notes:           document.getElementById('cn-notes')?.value.trim()||''
        };
        if (!body.consignor_name) { alert('Consignor name required'); return; }
        const r = id ? await Api.put('/api/consignments/'+id, body)
                     : await Api.post('/api/consignments', body);
        if (r.ok) { closeModal(); nav('consignments'); }
        else alert('Error: '+(r.data?.error||'save failed'));
    }
    async function deleteConsignment(id) {
        if (!confirm('Delete this consignment record?')) return;
        const r = await Api.del('/api/consignments/'+id);
        if (r.ok) nav('consignments');
    }

    // ── Rentals ───────────────────────────────────────────────────────────
    async function openRental(id) {
        let vals = { rental_type:'booth', location:'', vendor:'', vendor_user_id:'',
                     monthly_rate:0, discount_pct:0, discount_label:'',
                     start_date:'', end_date:'', payment_method:'',
                     auto_renew:1, due_day:1, grace_days:5, late_fee_pct:0, notes:'' };
        if (id) { const r = await Api.get('/api/rentals'); if (r.ok) { const f=r.data?.find?.(x=>String(x.id)===String(id)); if(f) vals=f; } }
        openModal(id?'Edit rental':'Add rental', `
<div class="form-group"><label>Type</label><select id="rl-type">
  <option value="booth"${vals.rental_type==='booth'?' selected':''}>Booth</option>
  <option value="storage"${vals.rental_type==='storage'?' selected':''}>Storage</option>
  <option value="display"${vals.rental_type==='display'?' selected':''}>Display space</option>
  <option value="shop"${vals.rental_type==='shop'?' selected':''}>Shop/premises</option>
  <option value="other"${vals.rental_type==='other'?' selected':''}>Other</option>
</select></div>
<div class="form-group"><label>Location / venue *</label><input id="rl-loc" value="${escVal(vals.location)}"></div>
<div class="form-group"><label>Vendor / tenant name *</label><input id="rl-vendor" value="${escVal(vals.vendor)}"></div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Monthly rate</label><input type="number" id="rl-rate" value="${vals.monthly_rate||0}" step="0.01" min="0"></div>
<div class="form-group"><label>Manual discount % (0 = auto)</label><input type="number" id="rl-disc-pct" value="${vals.discount_pct||0}" step="0.1" min="0" max="100"></div>
</div>
<div class="form-group"><label>Discount label (optional)</label><input id="rl-disc-lbl" value="${escVal(vals.discount_label)}" placeholder="e.g. Founding tenant, Preferred vendor"></div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Start date</label><input type="date" id="rl-start" value="${vals.start_date||''}"></div>
<div class="form-group"><label>End date (blank=ongoing)</label><input type="date" id="rl-end" value="${vals.end_date||''}"></div>
</div>
<div style="display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px">
<div class="form-group"><label>Due day of month</label><input type="number" id="rl-due" value="${vals.due_day||1}" min="1" max="28"></div>
<div class="form-group"><label>Grace days</label><input type="number" id="rl-grace" value="${vals.grace_days||5}" min="0" max="30"></div>
<div class="form-group"><label>Late fee %</label><input type="number" id="rl-latefee" value="${vals.late_fee_pct||0}" step="0.1" min="0"></div>
</div>
<div class="form-group"><label>Payment method</label><input id="rl-pay" value="${escVal(vals.payment_method)}"></div>
<div class="form-group"><label><input type="checkbox" id="rl-auto" ${vals.auto_renew==1?' checked':''}> Auto-renew</label></div>
<div class="form-group"><label>Notes</label><textarea id="rl-notes">${escVal(vals.notes)}</textarea></div>
<div class="form-actions"><button onclick="app.saveRental(${id||''})">Save</button></div>`);
    }
    async function saveRental(id) {
        const body = {
            rental_type:    document.getElementById('rl-type')?.value||'booth',
            location:       document.getElementById('rl-loc')?.value.trim()||'',
            vendor:         document.getElementById('rl-vendor')?.value.trim()||'',
            monthly_rate:   document.getElementById('rl-rate')?.value||'0',
            discount_pct:   document.getElementById('rl-disc-pct')?.value||'0',
            discount_label: document.getElementById('rl-disc-lbl')?.value.trim()||'',
            start_date:     document.getElementById('rl-start')?.value||'',
            end_date:       document.getElementById('rl-end')?.value||'',
            due_day:        document.getElementById('rl-due')?.value||'1',
            grace_days:     document.getElementById('rl-grace')?.value||'5',
            late_fee_pct:   document.getElementById('rl-latefee')?.value||'0',
            payment_method: document.getElementById('rl-pay')?.value.trim()||'',
            auto_renew:     document.getElementById('rl-auto')?.checked?'1':'0',
            notes:          document.getElementById('rl-notes')?.value.trim()||''
        };
        const r = id ? await Api.put('/api/rentals/'+id, body)
                     : await Api.post('/api/rentals', body);
        if (r.ok) { closeModal(); nav('rentals'); }
        else alert('Error: '+(r.data?.error||'save failed'));
    }
    async function deleteRental(id) {
        if (!confirm('Delete this rental record?')) return;
        const r = await Api.del('/api/rentals/'+id);
        if (r.ok) nav('rentals');
    }
    async function generateBoothInvoice(rentalId) {
        const period = prompt('Period for invoice (YYYY-MM):', new Date().toISOString().slice(0,7));
        if (!period) return;
        const r = await Api.post('/api/rentals/'+rentalId+'/invoice', { period });
        if (r.ok) {
            const d = r.data;
            alert(`Invoice #${d.invoice_id} generated for ${d.vendor}\nPeriod: ${d.period}\nBase: ${CurrencySymbol}${parseFloat(d.base).toFixed(2)}\nDiscount (${d.discount_label||'none'}): -${CurrencySymbol}${parseFloat(d.discount_amount).toFixed(2)}\nTotal due: ${CurrencySymbol}${parseFloat(d.total_due).toFixed(2)}`);
            nav('rentals');
        } else alert('Error: '+(r.data?.error||'invoice failed'));
    }
    async function markInvoicePaid(id) {
        if (!confirm('Mark invoice #'+id+' as paid?')) return;
        const r = await Api.put('/api/booth-invoices/'+id+'/paid', {});
        if (r.ok) nav('rentals');
        else alert('Error: '+(r.data?.error||'update failed'));
    }

    async function openRentalPayment(id) {
        openModal('Record payment', `
<div class="form-group"><label>Period (e.g. 2026-06)</label><input id="rp-period" value="${new Date().toISOString().slice(0,7)}"></div>
<div class="form-group"><label>Amount paid</label><input type="number" id="rp-amount" step="0.01" min="0"></div>
<div class="form-group"><label>Date paid</label><input type="date" id="rp-date" value="${new Date().toISOString().slice(0,10)}"></div>
<div class="form-group"><label>Receipt ref</label><input id="rp-ref"></div>
<div class="form-actions"><button onclick="app.saveRentalPayment(${id})">Save</button></div>`);
    }
    async function saveRentalPayment(rentalId) {
        const body = {
            period: document.getElementById('rp-period')?.value||'',
            amount: document.getElementById('rp-amount')?.value||'0',
            paid_date: document.getElementById('rp-date')?.value||'',
            receipt_ref: document.getElementById('rp-ref')?.value.trim()||''
        };
        const r = await Api.post('/api/rentals/'+rentalId+'/payments', body);
        if (r.ok) { closeModal(); nav('rentals'); }
        else alert('Error: '+(r.data?.error||'save failed'));
    }

    // ── Advertising ───────────────────────────────────────────────────────
    async function openAd(id) {
        let vals = { platform:'', campaign:'', ad_type:'listing', start_date:'',
                     end_date:'', budget:0, spent:0, impressions:0, clicks:0,
                     conversions:0, status:'active', notes:'' };
        if (id) { const r = await Api.get('/api/advertising'); if (r.ok) { const f=r.data?.find?.(x=>String(x.id)===String(id)); if(f) vals=f; } }
        openModal(id?'Edit campaign':'Add campaign', `
<div class="form-group"><label>Platform *</label><input id="ad-platform" value="${escVal(vals.platform)}" placeholder="eBay, 1stDibs, Google, etc."></div>
<div class="form-group"><label>Campaign name</label><input id="ad-campaign" value="${escVal(vals.campaign)}"></div>
<div class="form-group"><label>Ad type</label><select id="ad-type">
  <option value="listing"${vals.ad_type==='listing'?' selected':''}>Listing promotion</option>
  <option value="display"${vals.ad_type==='display'?' selected':''}>Display ad</option>
  <option value="social"${vals.ad_type==='social'?' selected':''}>Social media</option>
  <option value="search"${vals.ad_type==='search'?' selected':''}>Search (PPC)</option>
  <option value="print"${vals.ad_type==='print'?' selected':''}>Print</option>
  <option value="email"${vals.ad_type==='email'?' selected':''}>Email</option>
  <option value="other"${vals.ad_type==='other'?' selected':''}>Other</option>
</select></div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Start date</label><input type="date" id="ad-start" value="${vals.start_date||''}"></div>
<div class="form-group"><label>End date</label><input type="date" id="ad-end" value="${vals.end_date||''}"></div>
</div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Budget</label><input type="number" id="ad-budget" value="${vals.budget||0}" step="0.01" min="0"></div>
<div class="form-group"><label>Spent</label><input type="number" id="ad-spent" value="${vals.spent||0}" step="0.01" min="0"></div>
</div>
<div style="display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px">
<div class="form-group"><label>Impressions</label><input type="number" id="ad-imp" value="${vals.impressions||0}" min="0"></div>
<div class="form-group"><label>Clicks</label><input type="number" id="ad-clicks" value="${vals.clicks||0}" min="0"></div>
<div class="form-group"><label>Conversions</label><input type="number" id="ad-conv" value="${vals.conversions||0}" min="0"></div>
</div>
<div class="form-group"><label>Status</label><select id="ad-status">
  <option value="active"${vals.status==='active'?' selected':''}>Active</option>
  <option value="paused"${vals.status==='paused'?' selected':''}>Paused</option>
  <option value="ended"${vals.status==='ended'?' selected':''}>Ended</option>
</select></div>
<div class="form-group"><label>Notes</label><textarea id="ad-notes">${escVal(vals.notes)}</textarea></div>
<div class="form-actions"><button onclick="app.saveAd(${id||''})">Save</button></div>`);
    }
    async function saveAd(id) {
        const body = {
            platform: document.getElementById('ad-platform')?.value.trim()||'',
            campaign: document.getElementById('ad-campaign')?.value.trim()||'',
            ad_type: document.getElementById('ad-type')?.value||'listing',
            start_date: document.getElementById('ad-start')?.value||'',
            end_date: document.getElementById('ad-end')?.value||'',
            budget: document.getElementById('ad-budget')?.value||'0',
            spent: document.getElementById('ad-spent')?.value||'0',
            impressions: document.getElementById('ad-imp')?.value||'0',
            clicks: document.getElementById('ad-clicks')?.value||'0',
            conversions: document.getElementById('ad-conv')?.value||'0',
            status: document.getElementById('ad-status')?.value||'active',
            notes: document.getElementById('ad-notes')?.value.trim()||''
        };
        if (!body.platform) { alert('Platform required'); return; }
        const r = id ? await Api.put('/api/advertising/'+id, body)
                     : await Api.post('/api/advertising', body);
        if (r.ok) { closeModal(); nav('advertising'); }
        else alert('Error: '+(r.data?.error||'save failed'));
    }
    async function deleteAd(id) {
        if (!confirm('Delete this advertising campaign?')) return;
        const r = await Api.del('/api/advertising/'+id);
        if (r.ok) nav('advertising');
    }

    // ── CC Fees ───────────────────────────────────────────────────────────
    async function calcCcFee() {
        const amt = document.getElementById('cc-calc-amount')?.value;
        const proc = document.getElementById('cc-calc-proc')?.value;
        const res = document.getElementById('cc-calc-result');
        if (!amt || !res) return;
        const r = await Api.get('/api/cc-fees/calculate?amount='+amt+'&processor='+proc);
        if (r.ok) res.textContent = 'Fee: ' + (r.data.fee_amount ? CurrencySymbol + parseFloat(r.data.fee_amount).toFixed(2) : '—');
    }
    async function openCcFee(saleId) {
        openModal('Record CC fee', `
<div class="form-group"><label>Processor</label><select id="cf-proc">
  <option value="stripe">Stripe</option>
  <option value="square">Square</option>
  <option value="paypal">PayPal</option>
  <option value="venmo">Venmo</option>
  <option value="other">Other</option>
</select></div>
<div class="form-group"><label>Sale ID (optional)</label><input type="number" id="cf-sale" value="${saleId||''}"></div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
<div class="form-group"><label>Fee %</label><input type="number" id="cf-pct" value="2.9" step="0.01" min="0"></div>
<div class="form-group"><label>Flat fee</label><input type="number" id="cf-flat" value="0.30" step="0.01" min="0"></div>
</div>
<div class="form-group"><label>Fee amount *</label><input type="number" id="cf-amount" step="0.01" min="0"></div>
<div class="form-group"><label>Transaction ref</label><input id="cf-ref"></div>
<div class="form-group"><label>Date</label><input type="date" id="cf-date" value="${new Date().toISOString().slice(0,10)}"></div>
<div class="form-group"><label>Notes</label><textarea id="cf-notes"></textarea></div>
<div class="form-actions"><button onclick="app.saveCcFee()">Save</button></div>`);
    }
    async function saveCcFee() {
        const body = {
            processor: document.getElementById('cf-proc')?.value||'',
            sale_id: document.getElementById('cf-sale')?.value||'',
            fee_pct: document.getElementById('cf-pct')?.value||'0',
            fee_flat: document.getElementById('cf-flat')?.value||'0',
            fee_amount: document.getElementById('cf-amount')?.value||'0',
            transaction_ref: document.getElementById('cf-ref')?.value.trim()||'',
            fee_date: document.getElementById('cf-date')?.value||'',
            notes: document.getElementById('cf-notes')?.value.trim()||''
        };
        if (!body.fee_amount || parseFloat(body.fee_amount)<=0) { alert('Fee amount required'); return; }
        const r = await Api.post('/api/cc-fees', body);
        if (r.ok) { closeModal(); nav('ccfees'); }
        else alert('Error: '+(r.data?.error||'save failed'));
    }
    async function deleteCcFee(id) {
        if (!confirm('Delete this fee record?')) return;
        const r = await Api.del('/api/cc-fees/'+id);
        if (r.ok) nav('ccfees');
    }

    function escVal(s) { return String(s??'').replace(/"/g,'&quot;').replace(/</g,'&lt;'); }

    // ── Settlements ───────────────────────────────────────────────────────
    // ── POS hardware ──────────────────────────────────────────────────────
    async function posTestPrint() {
        const r = await Api.post('/api/pos/print-receipt', { sale_id: 'test' });
        if (!r.ok) alert('Printer error: ' + (r.data?.error||'check config.pson pos block'));
        else alert('Test receipt sent to printer.');
    }
    async function posOpenDrawer() {
        const r = await Api.post('/api/pos/open-drawer', {});
        if (!r.ok) alert('Drawer error: ' + (r.data?.error||'check config.pson pos block'));
    }
    async function posHandleScan(code) {
        const el = document.getElementById('pos-scan-result');
        if (el) el.innerHTML = '<span style="color:var(--text2)">Looking up ' + code + '\u2026</span>';
        const r = await Api.post('/api/pos/scan', { sku: code });
        if (r.ok && r.data) {
            const it = r.data;
            const fmtPrice = (() => { try { return parseFloat(it.asking_price).toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol+parseFloat(it.asking_price).toFixed(2); } })();
            if (el) el.innerHTML = `
<div style="background:var(--bg3);border-radius:6px;padding:10px;font-size:13px">
  <strong>${it.title}</strong><br>
  <span style="color:var(--text2)">${it.category}</span> &nbsp;&#x2022;&nbsp;
  <span style="color:var(--accent);font-weight:700">${fmtPrice}</span> &nbsp;&#x2022;&nbsp;
  <span class="badge badge-${it.status==='inventory'?'gray':it.status==='listed'?'blue':'green'}">${it.status}</span><br>
  <div style="margin-top:6px;display:flex;gap:6px">
    <button class="btn-ghost" style="font-size:11px" onclick="app.openItemDetail(${it.id})">View item</button>
    ${it.status!=='sold' ? `<button class="btn-ghost" style="font-size:11px" onclick="app.posPrintTag(${it.id})">&#x1F3F7; Print tag</button>` : ''}
    ${it.status==='listed'||it.status==='inventory' ? `<button class="btn" style="font-size:11px" onclick="app.openRecordSale(${it.id})">&#x1F4B0; Quick sale</button>` : ''}
  </div>
</div>`;
        } else {
            if (el) el.innerHTML = `<span style="color:var(--red)">Not found: ${code}</span>`;
        }
    }
    async function posManualScan() {
        const val = document.getElementById('pos-scan-input')?.value.trim();
        if (val) posHandleScan(val);
    }
    async function posPrintTag(itemId) {
        const r = await Api.post('/api/pos/print-tag', { item_id: String(itemId) });
        if (!r.ok) alert('Print error: ' + (r.data?.error||'check printer config'));
    }
    async function posPrintReceipt(saleId) {
        const r = await Api.post('/api/pos/print-receipt', { sale_id: String(saleId) });
        if (!r.ok) alert('Print error: ' + (r.data?.error||'check printer config'));
        else alert('Receipt sent to printer.');
    }

    // ── Platform sync ─────────────────────────────────────────────────────
    async function loadSyncListings() {
        const wrap = document.getElementById('sync-listings-wrap');
        if (!wrap) return;
        const r = await Api.get('/api/sync/listings');
        const listings = Array.isArray(r.data) ? r.data : [];
        wrap.innerHTML = Pages.renderSyncListings(listings);
    }
    async function syncPushOne(listingId) {
        const r = await Api.post('/api/sync/push', { listing_id: String(listingId) });
        if (r.ok) { alert('Sync started — refresh in a moment to see status.'); loadSyncListings(); }
        else alert('Sync error: ' + (r.data?.error||'unknown'));
    }
    async function syncPushAll(channel) {
        if (!confirm('Push all active listings for ' + channel + ' to the platform?')) return;
        const r = await Api.post('/api/sync/push-all', { channel });
        if (r.ok) alert('Queued ' + (r.data.queued||0) + ' listings for sync.');
        else alert('Error: ' + (r.data?.error||'unknown'));
        loadSyncListings();
    }

    async function markSettlementPaid(id) {
        if (!confirm('Mark this settlement as paid?')) return;
        const r = await Api.put('/api/settlements/'+id+'/paid', {});
        if (r.ok) nav('settlements');
        else alert('Error: '+(r.data?.error||'update failed'));
    }
    function emailSettlement(id) {
        alert('Settlement email queued. Ensure email.enabled=true in config.pson.');
    }

    // ── Server shutdown ───────────────────────────────────────────────────
    async function shutdownServer() {
        if (!confirm('Shut down the ' + (document.title || 'Metis Antique Marketplace Plus') + ' server?\n\nThe browser will lose connection immediately.')) return;
        try { await Api.post('/api/admin/shutdown', {}); } catch (_) {}
        document.body.innerHTML =
            '<div style="display:flex;align-items:center;justify-content:center;height:100vh;' +
            'font-family:sans-serif;color:#eee;background:#1a1a1a;flex-direction:column;gap:16px">' +
            '<div style="font-size:48px">&#x26D4;</div>' +
            '<div style="font-size:18px;font-weight:600">Server shut down</div>' +
            '<div style="font-size:13px;color:#aaa">Close this tab or restart the server to reconnect.</div>' +
            '</div>';
    }

    async function emailReceipt(id, email) {
        const r = await Api.post('/api/sales/' + id + '/email-receipt', {});
        if (r.ok) alert('Receipt sent to ' + email);
        else alert('Email failed: ' + (r.data?.error || 'unknown error'));
    }

    async function deleteSale(id) {
        if (!confirm('Delete this sale record? The item will be returned to inventory.')) return;
        const r = await Api.del('/api/sales/'+id);
        if (r.ok) nav('sales');
        else alert('Error: '+(r.data?.error||'delete failed'));
    }

    async function openRecordSale() {
        openModal('Record sale', '<p style="color:var(--text2);padding:12px">Loading items...</p>');
        const [ir, allR] = await Promise.all([
            Api.get('/api/items?status=listed&limit=200'),
            Api.get('/api/items?status=inventory&limit=200')
        ]);
        const items = [...(ir.data?.items||[]), ...(allR.data?.items||[])];
        const channels = ['1stDibs','eBay','Etsy','Ruby Lane','Chairish','Private sale','Auction house'];
        const methods  = ['Cash','Check','PayPal','Venmo','Credit card','Wire','Other'];
        document.getElementById('modal-body').innerHTML =
            '<div class="form-grid">' +
            '<div class="form-group full"><label>Item *</label>' +
            '<select id="s-item"><option value="">— select item to sell —</option>' +
            items.map(i => '<option value="'+i.id+'">'+esc(i.title)+(i.maker?' — '+esc(i.maker):'')+' ('+esc(i.status)+')</option>').join('') +
            '</select></div>' +
            '<div class="form-group"><label>Sale price (' + CurrencySymbol + ') *</label><input type="number" id="s-price" min="0" step="0.01"></div>' +
            '<div class="form-group"><label>Platform fee (' + CurrencySymbol + ')</label><input type="number" id="s-fee" value="0" min="0" step="0.01"></div>' +
            '<div class="form-group"><label>Shipping cost (' + CurrencySymbol + ')</label><input type="number" id="s-ship" value="0" min="0" step="0.01"></div>' +
            '<div class="form-group"><label>Payment method</label><select id="s-pay">' +
            ['','Cash','Check','PayPal','Venmo','Credit card','Wire','Other'].map(m => '<option value="'+m+'">'+(m||'— not specified —')+'</option>').join('') +
            '</select></div>' +
            '<div class="form-group"><label>Channel</label><select id="s-channel">' +
            channels.map(ch => '<option>'+ch+'</option>').join('') +
            '</select></div>' +
            '<div class="form-group"><label>Buyer name</label><input type="text" id="s-buyer"></div>' +
            '<div class="form-group"><label>Buyer email</label><input type="email" id="s-email"></div>' +
            '<div class="form-group full"><label>Notes</label><textarea id="s-notes"></textarea></div>' +
            '</div><div class="form-actions">' +
            '<button class="btn-ghost" onclick="app.closeModal()">Cancel</button>' +
            '<button class="btn" onclick="app.saveSale()">Record sale</button>' +
            '</div>';
    }


    async function saveSale() {
        const body = {
            item_id:        parseInt(document.getElementById('s-item')?.value)||0,
            sale_price:     parseFloat(document.getElementById('s-price').value)||0,
            platform_fee:   parseFloat(document.getElementById('s-fee').value)||0,
            shipping_cost:  parseFloat(document.getElementById('s-ship').value)||0,
            payment_method: document.getElementById('s-pay').value,
            channel:        document.getElementById('s-channel').value,
            buyer_name:     document.getElementById('s-buyer').value.trim(),
            buyer_email:    document.getElementById('s-email').value.trim(),
            notes:          document.getElementById('s-notes').value.trim()
        };
        if (!body.item_id || !body.sale_price) { alert('Item ID and sale price required'); return; }
        const r = await Api.post('/api/sales', body);
        if (r.ok) { closeModal(); nav('sales'); }
        else alert('Error: '+(r.data.error||'save failed'));
    }

    // -----------------------------------------------------------------------
    // Appraisals
    async function openAddAppraisal() {
        openModal('Request appraisal', '<p style="color:var(--text2);padding:12px">Loading...</p>');
        const ir = await Api.get('/api/items?status=inventory&limit=200');
        const items = ir.data?.items || [];
        const html = appraisalForm({});
        document.getElementById('modal-body').innerHTML = html;
        const wrap = document.getElementById('a-item-wrap');
        if (wrap) {
            wrap.innerHTML =
                '<div class="form-group full"><label>Item *</label>' +
                '<select id="a-item"><option value="">— select item —</option>' +
                items.map(i => '<option value="'+i.id+'">'+esc(i.title)+(i.maker?' — '+esc(i.maker):'')+' ('+esc(i.status)+')</option>').join('') +
                '</select></div>';
        }
    }

    async function openEditAppraisal(id) {
        const r = await Api.get('/api/appraisals/' + id);
        if (!r.ok) { alert('Could not load appraisal'); return; }
        const a = r.data;
        openModal('Update appraisal', appraisalForm(a));
        // Show item name (not editable when editing)
        const wrap = document.getElementById('a-item-wrap');
        if (wrap) wrap.innerHTML =
            '<div class="form-group full" style="background:var(--bg3);border-radius:6px;padding:10px;font-size:13px">'
            + '<strong>' + esc(a.title||'') + '</strong></div>';
    }

    function appraisalForm(a) {
        return `<div class="form-grid">
  <div id="a-item-wrap"></div>
  <div class="form-group full"><label>Appraiser name / firm</label><input type="text" id="a-appraiser" value="${esc(a.appraiser||'')}"></div>
  <div class="form-group"><label>Value low ($)</label><input type="number" id="a-vlow" value="${a.value_low||''}" min="0" step="0.01"></div>
  <div class="form-group"><label>Value high ($)</label><input type="number" id="a-vhigh" value="${a.value_high||''}" min="0" step="0.01"></div>
  <div class="form-group"><label>Condition assessment</label><input type="text" id="a-cond" value="${esc(a.condition||'')}"></div>
  <div class="form-group"><label>Status</label><select id="a-status">
    ${['pending','in review','complete'].map(s=>`<option value="${s}"${a.status===s?' selected':''}>${s}</option>`).join('')}
  </select></div>
  <div class="form-group full"><label>Notes</label><textarea id="a-notes">${esc(a.notes||'')}</textarea></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="app.saveAppraisal(${a.id||0})">${a.id?'Update':'Submit'}</button>
</div>`;
    }

    async function deleteAppraisal(id) {
        if (!confirm('Delete this appraisal?')) return;
        const r = await Api.del('/api/appraisals/'+id);
        if (r.ok) nav('appraisals');
        else alert('Error: '+(r.data?.error||'delete failed'));
    }

    async function saveAppraisal(id) {
        const body = {
            appraiser: document.getElementById('a-appraiser').value.trim(),
            value_low:  document.getElementById('a-vlow').value,
            value_high: document.getElementById('a-vhigh').value,
            condition:  document.getElementById('a-cond').value.trim(),
            status:     document.getElementById('a-status').value,
            notes:      document.getElementById('a-notes').value.trim()
        };
        if (!id) body.item_id = parseInt(document.getElementById('a-item').value)||0;
        const r = id ? await Api.put('/api/appraisals/'+id, body) : await Api.post('/api/appraisals', body);
        if (r.ok) { closeModal(); nav('appraisals'); }
        else alert('Error: '+(r.data.error||'save failed'));
    }

    // -----------------------------------------------------------------------
    // Users
    function openAddUser() {
        openModal('Add user', `<div class="form-grid">
  <div class="form-group full"><label>Username *</label><input type="text" id="u-name" autocomplete="off"></div>
  <div class="form-group full"><label>Display name</label><input type="text" id="u-dname" autocomplete="off"></div>
  <div class="form-group full"><label>Email</label><input type="email" id="u-email" autocomplete="off"></div>
  <div class="form-group full"><label>Password *</label><input type="password" id="u-pass" autocomplete="new-password"></div>
  <div class="form-group full"><label>Role</label><select id="u-role">
    <option value="dealer">Dealer — full access to inventory and sales</option>
    <option value="viewer">Viewer — read-only</option>
    <option value="admin">Admin — full access + user management</option>
  </select></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="app.saveNewUser()">Create user</button>
</div>`);
    }

    async function saveNewUser() {
        const uname = document.getElementById('u-name').value.trim();
        const pass  = document.getElementById('u-pass').value;
        const role  = document.getElementById('u-role').value;
        const email = document.getElementById('u-email')?.value.trim()||'';
        const dname = document.getElementById('u-dname')?.value.trim()||'';
        if (!uname||!pass) { alert('Username and password required'); return; }
        const r = await Api.post('/api/users', { username:uname, password:pass, role, email, display_name:dname });
        if (r.ok) { closeModal(); nav('users'); }
        else alert('Error: '+(r.data.error||'create failed'));
    }

    function openEditUser(id, username, role, email='', dname='') {
        openModal('Edit user: '+username, `<div class="form-grid">
  <div class="form-group full"><label>Display name</label><input type="text" id="eu-dname" value="${escVal(dname)}"></div>
  <div class="form-group full"><label>Email</label><input type="email" id="eu-email" value="${escVal(email)}"></div>
  <div class="form-group full"><label>Role</label><select id="eu-role">
    <option value="dealer"${role==='dealer'?' selected':''}>Dealer</option>
    <option value="viewer"${role==='viewer'?' selected':''}>Viewer</option>
    <option value="admin"${role==='admin'?' selected':''}>Admin</option>
  </select></div>
  <div class="form-group full"><label>New password <span style="color:var(--text2);font-weight:400">(leave blank to keep)</span></label>
    <input type="password" id="eu-pass" autocomplete="new-password"></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="app.saveEditUser(${id})">Save</button>
</div>`);
    }

    async function saveEditUser(id) {
        const body = {
            role:         document.getElementById('eu-role').value,
            email:        document.getElementById('eu-email')?.value.trim()||'',
            display_name: document.getElementById('eu-dname')?.value.trim()||''
        };
        const pass = document.getElementById('eu-pass').value;
        if (pass) body.password = pass;
        const r = await Api.put('/api/users/'+id, body);
        if (r.ok) { closeModal(); nav('users'); }
        else alert('Error: '+(r.data.error||'update failed'));
    }

    async function revokeSession(token) {
        if (!confirm('Revoke this session? The user will be logged out immediately.')) return;
        const r = await Api.del('/api/audit/sessions/'+token);
        if (r.ok) nav('users');
        else alert('Error: '+(r.data?.error||'revoke failed'));
    }

    async function deleteUser(id, username) {
        if (!confirm('Delete user "'+username+'"?')) return;
        const r = await Api.del('/api/users/'+id);
        if (r.ok) nav('users');
        else alert('Error: '+(r.data.error||'delete failed'));
    }

    // Change own password
    function openChangePassword() {
        openModal('Change password', `<div class="form-grid">
  <div class="form-group full"><label>Current password</label><input type="password" id="cp-old" autocomplete="current-password"></div>
  <div class="form-group full"><label>New password</label><input type="password" id="cp-new" autocomplete="new-password"></div>
  <div class="form-group full"><label>Confirm new password</label><input type="password" id="cp-confirm" autocomplete="new-password"></div>
</div>
<div class="form-actions">
  <button class="btn-ghost" onclick="app.closeModal()">Cancel</button>
  <button class="btn" onclick="app.saveChangePassword()">Change password</button>
</div>`);
    }

    async function saveChangePassword() {
        const old_p = document.getElementById('cp-old').value;
        const new_p = document.getElementById('cp-new').value;
        const con_p = document.getElementById('cp-confirm').value;
        if (!old_p||!new_p) { alert('All fields required'); return; }
        if (new_p!==con_p) { alert('New passwords do not match'); return; }
        const r = await Api.post('/api/auth/change-password', {old_password:old_p,new_password:new_p});
        if (r.ok) { closeModal(); alert('Password changed successfully'); }
        else alert('Error: '+(r.data.error||'change failed'));
    }

    async function refreshInquiryBadge() {
        const r = await Api.get('/api/inquiries/summary');
        const badge = document.getElementById('inq-badge');
        if (!badge) return;
        const open = parseInt(r.data?.open||0);
        if (open > 0) { badge.textContent = open; badge.style.display = ''; }
        else badge.style.display = 'none';
    }

    return {
        init, login, logout, nav, closeModal, toggleTheme,
        openItemDetail, openAddItem, openEditItem, saveItem, deleteItem, duplicateItem,
        deletePhoto, openImport, doImport,
        openAddListing, saveListing, openEditListing, saveListingEdit, saveListingUrl,
        updateWatchers, removeListing,
        openRecordSale, saveSale, openEditSale, saveEditSale, deleteSale, emailReceipt, shutdownServer,
        openConsignment, saveConsignment, deleteConsignment,
        openRental, saveRental, deleteRental, openRentalPayment, saveRentalPayment,
        generateBoothInvoice, markInvoicePaid,
        openAd, saveAd, deleteAd,
        calcCcFee, openCcFee, saveCcFee, deleteCcFee,
        markSettlementPaid, emailSettlement,
        posTestPrint, posOpenDrawer, posHandleScan, posManualScan,
        posPrintTag, posPrintReceipt,
        loadSyncListings, syncPushOne, syncPushAll,
        openAddAppraisal, openEditAppraisal, saveAppraisal, deleteAppraisal,
        openAddUser, saveNewUser, openEditUser, saveEditUser, deleteUser, revokeSession,
        openChangePassword, saveChangePassword,
        refreshInquiryBadge
    };
})();

document.addEventListener('DOMContentLoaded', app.init);
document.addEventListener('keydown', e => { if (e.key==='Escape') app.closeModal(); });

// ── i18n ──────────────────────────────────────────────────────────────────────
function applyI18n() {
    document.querySelectorAll('[data-i18n]').forEach(el => {
        el.textContent = I18n.t(el.dataset.i18n);
    });
    document.querySelectorAll('[data-i18n-ph]').forEach(el => {
        el.placeholder = I18n.t(el.dataset.i18nPh);
    });
    document.querySelectorAll('[data-i18n-title]').forEach(el => {
        el.title = I18n.t(el.dataset.i18nTitle);
    });
    document.title = I18n.t('app_title');
}

function initLangSelect() {
    const sel = document.getElementById('lang-select');
    if (!sel) return;
    const names = I18n.getLanguageNames();
    I18n.getLocales().forEach(code => {
        const opt = document.createElement('option');
        opt.value = code;
        opt.textContent = names[code] || code;
        if (code === I18n.getLocale()) opt.selected = true;
        sel.appendChild(opt);
    });
    sel.addEventListener('change', () => I18n.setLocale(sel.value));
    document.addEventListener('localeChanged', () => applyI18n());
}

// ── Font size ─────────────────────────────────────────────────────────────────
(function () {
    const FONT_SIZES  = [13, 15, 17, 19];
    const FONT_LABELS = ['Small', 'Normal', 'Large', 'X-Large'];
    let idx = parseInt(localStorage.getItem('metis_antique_font_idx') || '1', 10);
    if (idx < 0 || idx >= FONT_SIZES.length) idx = 1;

    function applyFontSize(i) {
        idx = Math.max(0, Math.min(FONT_SIZES.length - 1, i));
        document.documentElement.style.fontSize = FONT_SIZES[idx] + 'px';
        localStorage.setItem('metis_antique_font_idx', idx);
        const dec   = document.getElementById('btn-font-decrease');
        const reset = document.getElementById('btn-font-reset');
        const inc   = document.getElementById('btn-font-increase');
        if (dec)   dec.disabled   = (idx === 0);
        if (inc)   inc.disabled   = (idx === FONT_SIZES.length - 1);
        if (reset) reset.title    = 'Font: ' + FONT_LABELS[idx] + ' \u2014 click to reset';
    }

    document.addEventListener('DOMContentLoaded', () => {
        applyFontSize(idx);
        document.getElementById('btn-font-decrease')
            ?.addEventListener('click', () => applyFontSize(idx - 1));
        document.getElementById('btn-font-reset')
            ?.addEventListener('click', () => applyFontSize(1));
        document.getElementById('btn-font-increase')
            ?.addEventListener('click', () => applyFontSize(idx + 1));
        initLangSelect();
        applyI18n();
    });
})();

// ================================================================
// KEYBOARD NAVIGATION
// ================================================================
(function() {
    // --- Table row navigation with arrow keys ---
    let focusedRow = null;

    function getRows() {
        const tbl = document.querySelector('.page:not(.hidden) table tbody');
        return tbl ? Array.from(tbl.querySelectorAll('tr')) : [];
    }

    function moveFocus(delta) {
        const rows = getRows();
        if (!rows.length) return;
        const cur = rows.indexOf(focusedRow);
        let next = cur === -1 ? (delta > 0 ? 0 : rows.length - 1)
                               : Math.max(0, Math.min(rows.length - 1, cur + delta));
        if (focusedRow) focusedRow.classList.remove('kb-focus');
        focusedRow = rows[next];
        focusedRow.classList.add('kb-focus');
        focusedRow.scrollIntoView({ block: 'nearest' });
    }

    document.addEventListener('keydown', e => {
        // Don't intercept when typing in form fields
        const tag = document.activeElement?.tagName;
        const inField = tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT';

        // Escape: close modal
        if (e.key === 'Escape') { app.closeModal(); return; }

        // Table navigation (only when not in form fields)
        if (!inField && !document.getElementById('modal')?.classList.contains('hidden') === false) {
            if (e.key === 'ArrowDown') { e.preventDefault(); moveFocus(1); return; }
            if (e.key === 'ArrowUp')   { e.preventDefault(); moveFocus(-1); return; }
            if (e.key === 'Enter' && focusedRow) {
                // Click the first button in the row, or the row itself
                const btn = focusedRow.querySelector('button');
                if (btn) btn.click();
                return;
            }
        }

        // / key: focus search input if present
        if (!inField && e.key === '/') {
            e.preventDefault();
            const search = document.querySelector('.page:not(.hidden) input[type="text"][placeholder*="earch"]') ||
                           document.querySelector('.page:not(.hidden) input[type="text"]');
            if (search) { search.focus(); search.select(); }
            return;
        }

        // Modal form: Ctrl+Enter or Cmd+Enter submits
        if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
            const modal = document.getElementById('modal');
            if (modal && !modal.classList.contains('hidden')) {
                // Find the primary submit button (last .btn in modal-body)
                const btns = modal.querySelectorAll('#modal-body .btn:not(.btn-ghost):not(.btn-danger)');
                if (btns.length) { btns[btns.length - 1].click(); }
            }
            return;
        }

        // N: new item shortcut (when not in modal, not in field)
        if (!inField && e.key === 'n' && document.getElementById('modal')?.classList.contains('hidden')) {
            const addBtn = document.querySelector('.page:not(.hidden) .page-header .btn');
            if (addBtn) { e.preventDefault(); addBtn.click(); }
        }

        // Tab navigation between modal form groups (already native, just ensure submit hint)
    });

    // Clear focused row when clicking elsewhere
    document.addEventListener('click', e => {
        if (!e.target.closest('tr')) {
            if (focusedRow) focusedRow.classList.remove('kb-focus');
            focusedRow = null;
        }
    });

    // Add submit hint to modal forms
    document.addEventListener('focusin', e => {
        if (!e.target.closest('#modal-body')) return;
        const hint = document.querySelector('#modal-body .form-submit-hint');
        if (!hint) {
            const actions = document.querySelector('#modal-body .form-actions');
            if (actions) {
                const div = document.createElement('div');
                div.className = 'form-submit-hint';
                div.textContent = 'Ctrl+Enter to submit';
                actions.parentNode.insertBefore(div, actions);
            }
        }
    });
})();
