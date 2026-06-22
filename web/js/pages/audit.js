// Metis Antique Marketplace Plus - Audit page (admin only)

Pages.renderAudit = async function(el) {
    // Stat cards first, then tabs
    const statsR = await Api.get('/api/audit/stats');
    const s = statsR.data || {};

    function esc(x) { return String(x??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Audit &amp; Logs</h1>
  <div style="display:flex;gap:8px">
    <button class="btn-ghost" onclick="AuditUI.refresh()">&#x21BA; Refresh</button>
  </div>
</div>

<div class="stat-grid" style="margin-bottom:20px">
  <div class="stat-card"><div class="stat-label">Active sessions</div><div class="stat-value">${esc(s.active_sessions||0)}</div></div>
  <div class="stat-card"><div class="stat-label">Item changes</div><div class="stat-value">${esc(s.item_changes||0)}</div></div>
  <div class="stat-card"><div class="stat-label">Failed logins</div><div class="stat-value${parseInt(s.failed_logins||0)>0?' amber':''}">${esc(s.failed_logins||0)}</div></div>
  <div class="stat-card"><div class="stat-label">Total sales</div><div class="stat-value green">${esc(s.total_sales||0)}</div></div>
  <div class="stat-card"><div class="stat-label">Buyer inquiries</div><div class="stat-value">${esc(s.buyer_inquiries||0)}</div></div>
</div>

<div class="pill-bar" id="audit-pills">
  <button class="pill active" data-pill="trail" onclick="AuditUI.showPill('trail')">&#x1F4CB; Audit trail</button>
  <button class="pill"        data-pill="log"   onclick="AuditUI.showPill('log')">&#x1F4C4; System log</button>
  <button class="pill"        data-pill="sessions" onclick="AuditUI.showPill('sessions')">&#x1F464; Active sessions</button>
</div>

<div id="audit-panel-trail"></div>
<div id="audit-panel-log"      style="display:none"></div>
<div id="audit-panel-sessions" style="display:none"></div>`;

    AuditUI.showPill('trail');
};

const AuditUI = (() => {
    function esc(x) { return String(x??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

    const TYPE_LABELS = {
        item_change:  { label:'Item change',   color:'var(--blue)',   icon:'&#x270F;' },
        item_create:  { label:'Item added',    color:'var(--green)',  icon:'&#x2795;' },
        sale:         { label:'Sale',           color:'var(--green)',  icon:'&#x1F4B0;' },
        listing:      { label:'Listing',        color:'var(--accent2)',icon:'&#x1F4CB;' },
        photo:        { label:'Photo',          color:'var(--text2)',  icon:'&#x1F4F7;' },
        comment:      { label:'Note',           color:'var(--text2)',  icon:'&#x1F4AC;' },
        inquiry:      { label:'Inquiry',        color:'var(--amber)',  icon:'&#x2709;' },
        login:        { label:'Login',          color:'var(--green)',  icon:'&#x1F511;' },
        failed_login: { label:'Failed login',   color:'var(--red)',    icon:'&#x26A0;' },
    };

    const LOG_COLORS = {
        INFO:   'var(--text)',
        WARN:   'var(--amber)',
        ERROR:  'var(--red)',
        system: 'var(--blue)',
    };

    async function showPill(pill) {
        // Update pill styles
        document.querySelectorAll('#audit-pills .pill').forEach(b => {
            b.classList.toggle('active', b.dataset.pill === pill);
        });
        // Hide all panels
        ['trail','log','sessions'].forEach(p => {
            const el = document.getElementById('audit-panel-'+p);
            if (el) el.style.display = p === pill ? '' : 'none';
        });
        // Load content if empty
        const panel = document.getElementById('audit-panel-'+pill);
        if (!panel) return;
        if (panel.dataset.loaded) return;
        panel.dataset.loaded = '1';
        panel.innerHTML = '<p style="color:var(--text2);padding:20px">Loading...</p>';

        if (pill === 'trail')    await loadTrail(panel);
        if (pill === 'log')      await loadLog(panel);
        if (pill === 'sessions') await loadSessions(panel);
    }

    async function loadTrail(panel) {
        const r = await Api.get('/api/audit/trail?limit=200');
        const rows = r.data?.trail || [];

        panel.innerHTML = `
<div class="toolbar" style="margin-top:14px">
  <select id="trail-type-filter" onchange="AuditUI.filterTrail()">
    <option value="">All event types</option>
    <option value="item_change">Item changes</option>
    <option value="item_create">Items added</option>
    <option value="sale">Sales</option>
    <option value="listing">Listings</option>
    <option value="photo">Photos</option>
    <option value="comment">Notes</option>
    <option value="inquiry">Inquiries</option>
    <option value="login">Logins</option>
    <option value="failed_login">Failed logins</option>
  </select>
  <input type="text" id="trail-search" placeholder="Search actor or description..."
    oninput="AuditUI.filterTrail()" style="flex:2">
</div>
<div id="trail-list" style="margin-top:12px;display:flex;flex-direction:column;gap:6px">
  ${renderTrailRows(rows)}
</div>`;
        window._trailRows = rows;
    }

    function renderTrailRows(rows) {
        if (!rows.length) return '<div class="empty-state"><div class="empty-icon">&#x1F4CB;</div><p>No audit events found</p></div>';
        return rows.map(r => {
            const meta = TYPE_LABELS[r.type] || { label: r.type, color:'var(--text2)', icon:'&#x2022;' };
            return `<div class="audit-row" data-type="${esc(r.type)}" data-text="${esc((r.actor+r.description+r.detail).toLowerCase())}">
  <div class="audit-row-icon" style="color:${meta.color}">${meta.icon}</div>
  <div class="audit-row-body">
    <div class="audit-row-desc">${esc(r.description)}</div>
    ${r.detail ? `<div class="audit-row-detail">${esc(r.detail)}</div>` : ''}
  </div>
  <div class="audit-row-right">
    <span class="audit-pill" style="color:${meta.color};background:${meta.color}22">${meta.label}</span>
    <span class="audit-actor">&#x1F464; ${esc(r.actor)}</span>
    <span class="audit-ts">${esc((r.ts||'').slice(0,16).replace('T',' '))}</span>
  </div>
</div>`;
        }).join('');
    }

    function filterTrail() {
        const type = document.getElementById('trail-type-filter')?.value || '';
        const q    = (document.getElementById('trail-search')?.value || '').toLowerCase();
        const rows = (window._trailRows || []).filter(r => {
            if (type && r.type !== type) return false;
            if (q && !(r.actor+r.description+r.detail).toLowerCase().includes(q)) return false;
            return true;
        });
        const list = document.getElementById('trail-list');
        if (list) list.innerHTML = renderTrailRows(rows);
    }

    async function loadLog(panel) {
        panel.innerHTML = `
<div class="toolbar" style="margin-top:14px">
  <select id="log-level-filter" onchange="AuditUI.filterLog()">
    <option value="">All levels</option>
    <option value="INFO">INFO</option>
    <option value="WARN">WARN</option>
    <option value="ERROR">ERROR</option>
    <option value="system">system</option>
  </select>
  <input type="text" id="log-search" placeholder="Search log messages..."
    oninput="AuditUI.filterLog()" style="flex:2">
  <span id="log-status" style="font-size:11px;color:var(--text2);white-space:nowrap">Connecting...</span>
  <button class="btn-ghost" style="font-size:12px" onclick="AuditUI.togglePause()">&#x23F8; Pause</button>
</div>
<div id="log-list" class="log-list" style="margin-top:12px;max-height:600px;overflow-y:auto">
  <p style="color:var(--text2);padding:20px;text-align:center;font-style:italic">Connecting to live log stream...</p>
</div>`;
        window._logLines   = [];
        window._logPaused  = false;
        window._logActive  = true;

        // Connect WebSocket
        const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = proto + '//' + location.host + '/ws/logs?token=' +
                      (sessionStorage.getItem('map_token')||'');
        let ws;
        try { ws = new WebSocket(wsUrl); } catch(e) { fallbackLog(panel); return; }

        panel._ws = ws;

        ws.onopen = () => {
            const st = document.getElementById('log-status');
            if (st) st.textContent = 'Live';
            if (st) st.style.color = 'var(--green)';
        };

        ws.onmessage = (evt) => {
            if (window._logPaused || !window._logActive) return;
            try {
                const d = JSON.parse(evt.data);
                if (d.type === 'history_end') return;
                window._logLines.unshift(d); // newest first
                if (window._logLines.length > 1000) window._logLines.pop();
                renderLogTable();
            } catch(e) {}
        };

        ws.onerror = ws.onclose = () => {
            const st = document.getElementById('log-status');
            if (st) { st.textContent = 'Disconnected'; st.style.color = 'var(--red)'; }
            // Re-connect after 5s if page still active
            if (window._logActive) setTimeout(() => loadLog(panel), 5000);
        };
    }

    async function fallbackLog(panel) {
        // REST fallback when WebSocket not supported
        const r = await Api.get('/api/audit/log?limit=500');
        window._logLines = r.data?.lines || [];
        renderLogTable();
    }

    function renderLogTable() {
        const level = document.getElementById('log-level-filter')?.value||'';
        const q     = (document.getElementById('log-search')?.value||'').toLowerCase();
        const lines = (window._logLines||[]).filter(l => {
            if (level && l.level !== level) return false;
            if (q && !(l.msg||l.message||'').toLowerCase().includes(q)) return false;
            return true;
        });
        const list = document.getElementById('log-list');
        if (!list) return;
        if (!lines.length) {
            list.innerHTML = '<p style="color:var(--text2);padding:20px;text-align:center">No log lines</p>';
            return;
        }
        const LOG_COLORS = {INFO:'var(--text)',WARN:'var(--amber)',ERROR:'var(--red)',system:'var(--blue)'};
        list.innerHTML = `<table style="width:100%;border-collapse:collapse;font-family:monospace;font-size:12px"><tbody>
${lines.map(l => {
    const ts  = l.ts  || l.timestamp || '';
    const lv  = l.level  || '';
    const msg = l.msg || l.message || '';
    const col = LOG_COLORS[lv] || 'var(--text)';
    return `<tr class="log-line">
  <td class="log-ts">${esc(ts)}</td>
  <td class="log-lv" style="color:${col}">[${esc(lv)}]</td>
  <td class="log-msg" style="color:${col}">${esc(msg)}</td>
</tr>`;}).join('')}</tbody></table>`;
        // Auto-scroll to top (newest)
        list.scrollTop = 0;
    }

        function filterLog() {
        renderLogTable();
    }

    function togglePause() {
        window._logPaused = !window._logPaused;
        const btn = document.querySelector('#audit-panel-log button[onclick*="togglePause"]');
        if (btn) btn.textContent = window._logPaused ? '▶ Resume' : '⏸ Pause';
    }

    async function loadSessions(panel) {
        const r = await Api.get('/api/audit/sessions');
        const sessions = Array.isArray(r.data) ? r.data : [];
        panel.innerHTML = `
<div style="margin-top:14px" class="table-wrap">
  <table>
    <thead><tr>
      <th>Username</th><th>Role</th><th>Token</th>
      <th>Created</th><th>Expires</th><th>Status</th><th></th>
    </tr></thead>
    <tbody>${sessions.length ? sessions.map(s => `
<tr>
  <td><strong>${esc(s.username)}</strong></td>
  <td>${esc(s.role)}</td>
  <td style="font-family:monospace;font-size:11px;color:var(--text2)">${esc(s.token)}</td>
  <td style="font-size:12px">${esc((s.created_at||'').slice(0,16).replace('T',' '))}</td>
  <td style="font-size:12px">${esc((s.expires_at||'').slice(0,16).replace('T',' '))}</td>
  <td><span class="badge ${s.active==='1'?'badge-green':'badge-gray'}">${s.active==='1'?'Active':'Expired'}</span></td>
  <td>${s.active==='1'
    ? `<button class="btn-danger" style="font-size:11px;padding:3px 8px"
         onclick="AuditUI.kickSession('${esc(s.token)}',this)">Kick</button>`
    : ''}</td>
</tr>`).join('') :
'<tr><td colspan="7" style="text-align:center;color:var(--text2);padding:24px">No sessions found</td></tr>'}</tbody>
  </table>
</div>`;
    }

    async function kickSession(token, btn) {
        if (!confirm('Force-expire this session? That user will be logged out immediately.')) return;
        const r = await Api.del('/api/audit/sessions/' + token);
        if (r.ok) {
            const row = btn.closest('tr');
            if (row) {
                row.querySelector('.badge').className = 'badge badge-gray';
                row.querySelector('.badge').textContent = 'Expired';
                btn.remove();
            }
        } else alert('Error: ' + (r.data?.error || 'failed'));
    }

    function refresh() {
        // Clean up WebSocket if log panel active
        window._logActive = false;
        const logPanel = document.getElementById('audit-panel-log');
        if (logPanel && logPanel._ws) { try { logPanel._ws.close(); } catch(e){} }
        // Clear loaded flags and reload current pill
        ['trail','log','sessions'].forEach(p => {
            const el = document.getElementById('audit-panel-'+p);
            if (el) { el.dataset.loaded = ''; el.innerHTML = ''; }
        });
        const active = document.querySelector('#audit-pills .pill.active')?.dataset?.pill || 'trail';
        showPill(active);
    }

    return { showPill, filterTrail, filterLog, togglePause, kickSession, refresh };
})();
