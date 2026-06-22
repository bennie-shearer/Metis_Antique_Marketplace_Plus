// Metis Antique Marketplace Plus v1.2.48 -- POS hardware control panel
'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// Barcode scanner: HID keyboard wedge listens for rapid keystrokes ending in
// Enter. Fires app.posHandleScan(code) when a scan is detected.
// ─────────────────────────────────────────────────────────────────────────────
var POSScanner = (() => {
    let buf = '', lastKey = 0;
    const SCAN_TIMEOUT = 80;  // ms between chars -- scanners type faster than humans

    document.addEventListener('keydown', e => {
        // Ignore if focus is inside an input/textarea
        const tag = document.activeElement?.tagName;
        if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;

        const now = Date.now();
        if (now - lastKey > 500) buf = ''; // reset on long pause
        lastKey = now;

        if (e.key === 'Enter' && buf.length >= 4) {
            const code = buf.trim();
            buf = '';
            if (typeof app !== 'undefined' && typeof app.posHandleScan === 'function')
                app.posHandleScan(code);
            e.preventDefault();
            return;
        }
        if (e.key && e.key.length === 1) buf += e.key;
    });
    return { getBuffer: () => buf };
})();

// ─────────────────────────────────────────────────────────────────────────────
// POS page renderer
// ─────────────────────────────────────────────────────────────────────────────
Pages.renderPos = async function(el) {
    el.innerHTML = '<div class="dash-loading">Loading POS\u2026</div>';
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function fmtB(v) {
        const n = parseFloat(v)||0;
        try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); }
        catch(e) { return CurrencySymbol + n.toFixed(2); }
    }

    try {
        const [posR, syncR] = await Promise.all([
            Api.get('/api/pos/status'),
            Api.get('/api/sync/status')
        ]);
        const pos  = posR.data  || {};
        const sync = syncR.data || {};
        const channels = Array.isArray(sync.channels) ? sync.channels : [];

        el.innerHTML = `
<div class="dash-wrap">
  <div class="dash-header">
    <h2 class="dash-title">&#x1F5A8; POS &amp; Platform Sync</h2>
  </div>

  <!-- POS Hardware -->
  <div class="dash-section-label">POS Hardware</div>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-bottom:24px">
    <div style="background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px">
      <div style="font-weight:600;margin-bottom:12px">&#x1F5A8; Receipt printer</div>
      <div style="font-size:13px;color:var(--text2);line-height:1.8">
        Status: <strong style="color:${pos.enabled?'var(--green)':'var(--text2)'}">${pos.enabled?'Enabled':'Disabled'}</strong><br>
        Type: <strong>${esc(pos.printer_type||'—')}</strong><br>
        Address: <strong>${esc(pos.printer_ip||'—')}:${pos.printer_port||9100}</strong><br>
        Cash drawer: <strong>${pos.cash_drawer?'&#x2705; Enabled':'—'}</strong>
      </div>
      <div style="margin-top:12px;display:flex;gap:8px;flex-wrap:wrap">
        <button class="btn-ghost" onclick="app.posTestPrint()">&#x1F9EA; Test print</button>
        <button class="btn-ghost" onclick="app.posOpenDrawer()">&#x1F4B0; Open drawer</button>
      </div>
      ${!pos.enabled ? `<div style="margin-top:10px;font-size:11px;color:var(--text2)">Enable in config.pson: <code>pos.enabled = true</code></div>` : ''}
    </div>

    <div style="background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px">
      <div style="font-weight:600;margin-bottom:12px">&#x1F4F7; Barcode scanner</div>
      <div style="font-size:13px;color:var(--text2);line-height:1.8">
        Status: <strong style="color:var(--green)">Always active</strong><br>
        Type: HID keyboard wedge<br>
        No drivers or config required.<br>
        Plug in any USB scanner — it works natively in the browser.
      </div>
      <div style="margin-top:12px">
        <div style="font-size:12px;color:var(--text2);margin-bottom:6px">Test scan (or scan a barcode):</div>
        <div style="display:flex;gap:8px">
          <input id="pos-scan-input" type="text" placeholder="SKU or UPC" style="flex:1" onkeydown="if(event.key==='Enter')app.posManualScan()">
          <button class="btn-ghost" onclick="app.posManualScan()">Lookup</button>
        </div>
      </div>
      <div id="pos-scan-result" style="margin-top:10px;min-height:40px"></div>
    </div>
  </div>

  <!-- Platform Sync -->
  <div class="dash-section-label">Platform listing sync</div>
  <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin-bottom:24px">
    ${channels.map(c => `
    <div style="background:var(--bg2);border:1px solid ${c.enabled&&c.credentials?'var(--green)':c.enabled?'var(--amber)':'var(--border)'};border-radius:8px;padding:14px">
      <div style="font-weight:600;font-size:14px;margin-bottom:6px">${esc(c.channel)}</div>
      <div style="font-size:12px;color:var(--text2);margin-bottom:8px">
        ${c.enabled && c.credentials ? '<span style="color:var(--green)">&#x2705; Ready</span>' :
          c.enabled ? '<span style="color:var(--amber)">&#x26A0; Missing credentials</span>' :
          '<span style="color:var(--text2)">Disabled</span>'}
      </div>
      ${c.enabled && c.credentials ? `
      <button class="btn-ghost" style="font-size:11px;width:100%" onclick="app.syncPushAll('${esc(c.channel)}')">
        &#x1F503; Push all listings
      </button>` : `
      <div style="font-size:10px;color:var(--text2)">Set in config.pson<br>sync.${c.channel.toLowerCase().replace('1stdibs','onedibs')}.enabled = true</div>
      `}
    </div>`).join('')}
  </div>

  <!-- Sync listings table -->
  <div class="dash-section-label">Listing sync status</div>
  <div id="sync-listings-wrap">Loading\u2026</div>
</div>`;

        // Load sync listings table
        app.loadSyncListings();
    } catch(e) { el.innerHTML = `<div class="dash-error">Error: ${e}</div>`; }
};

Pages.renderSyncListings = function(listings) {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function fmtB(v) {
        const n=parseFloat(v)||0;
        try { return n.toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:0,maximumFractionDigits:0}); }
        catch(e) { return CurrencySymbol+n.toFixed(0); }
    }
    const syncable = listings.filter(l => ['eBay','Etsy','1stDibs','Chairish'].includes(l.channel));
    if (!syncable.length) return '<div class="empty-state">No listings on syncable channels (eBay, Etsy, 1stDibs, Chairish)</div>';

    const statusColor = {
        synced:'var(--green)', error:'var(--red)', syncing:'var(--amber)',
        pending:'var(--text2)', local:'var(--text2)', sold:'var(--text2)'
    };
    return `<div class="table-wrap"><table>
  <thead><tr><th>Item</th><th>Channel</th><th>Price</th><th>Sync status</th><th>External ID</th><th>Last synced</th><th></th></tr></thead>
  <tbody>${syncable.map(l=>`<tr>
    <td>${esc(l.title)}</td>
    <td>${esc(l.channel)}</td>
    <td>${fmtB(l.list_price)}</td>
    <td>
      <span style="color:${statusColor[l.sync_status]||'var(--text2)'}">&#x25CF;</span>
      ${esc(l.sync_status)}
      ${l.sync_error ? `<br><span style="font-size:10px;color:var(--red)" title="${esc(l.sync_error)}">${esc(l.sync_error.slice(0,60))}${l.sync_error.length>60?'\u2026':''}</span>` : ''}
    </td>
    <td style="font-size:11px;font-family:monospace">
      ${l.external_url ? `<a href="${esc(l.external_url)}" target="_blank" style="color:var(--accent)">&#x1F517; ${esc(l.external_id.slice(0,16))}</a>` : (l.external_id || '—')}
    </td>
    <td style="font-size:11px;color:var(--text2)">${l.last_synced ? l.last_synced.slice(0,16).replace('T',' ') : '—'}</td>
    <td>
      <button class="btn-ghost" style="font-size:11px" onclick="app.syncPushOne(${l.id})">&#x1F503; Push</button>
    </td>
  </tr>`).join('')}
  </tbody>
</table></div>`;
};
