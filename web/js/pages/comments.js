// Metis Antique Marketplace Plus - Comments and buyer inquiry UI

Pages.renderComments = async function(itemId, container) {
    const r = await Api.get('/api/items/' + itemId + '/comments');
    const comments = Array.isArray(r.data) ? r.data : [];
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function ts(s) { return (s||'').slice(0,16).replace('T',' '); }
    container.innerHTML = `
<div style="margin-bottom:12px;display:flex;align-items:center;justify-content:space-between">
  <p style="font-size:12px;font-weight:600;color:var(--text2)">DEALER NOTES (${comments.length})</p>
</div>
<div id="comment-list-${itemId}" style="display:flex;flex-direction:column;gap:8px;margin-bottom:12px">
${comments.length ? comments.map(c => `
<div id="cmt-${c.id}" style="background:var(--bg3);border-radius:6px;padding:10px 12px">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">
    <span style="font-size:12px;font-weight:600;color:var(--accent2)">${esc(c.author)}</span>
    <div style="display:flex;gap:8px;align-items:center">
      <span style="font-size:11px;color:var(--text2)">${ts(c.created_at)}</span>
      ${window._currentRole === 'admin' ?
        `<button onclick="CommentsUI.deleteComment(${c.id},'${itemId}')"
           style="background:none;border:none;color:var(--red);cursor:pointer;font-size:13px">&#x2715;</button>` : ''}
    </div>
  </div>
  <p style="font-size:13px;white-space:pre-wrap">${esc(c.body)}</p>
</div>`).join('') :
'<p style="font-size:13px;color:var(--text2);text-align:center;padding:16px">No notes yet</p>'}
</div>
<div style="display:flex;gap:8px">
  <textarea id="new-comment-${itemId}" rows="2"
    placeholder="Add an internal note (visible to all staff)..."
    style="flex:1;resize:none;font-size:13px"></textarea>
  <button class="btn" style="align-self:flex-end" onclick="CommentsUI.postComment(${itemId})">Post</button>
</div>`;
};

const CommentsUI = (() => {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function ts(s) { return (s||'').slice(0,16).replace('T',' '); }

    async function postComment(itemId) {
        const ta = document.getElementById('new-comment-'+itemId);
        const body = ta?.value?.trim();
        if (!body) return;
        const r = await Api.post('/api/items/'+itemId+'/comments', { body });
        if (!r.ok) { alert('Error: '+(r.data.error||'post failed')); return; }
        ta.value = '';
        // Inject new comment
        const list = document.getElementById('comment-list-'+itemId);
        if (list) {
            const d = document.createElement('div');
            d.id = 'cmt-'+r.data.id;
            d.style.cssText = 'background:var(--bg3);border-radius:6px;padding:10px 12px;border-left:2px solid var(--accent)';
            d.innerHTML = `<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">
  <span style="font-size:12px;font-weight:600;color:var(--accent2)">${esc(r.data.author)}</span>
  <span style="font-size:11px;color:var(--text2)">${ts(r.data.created_at)}</span>
</div>
<p style="font-size:13px;white-space:pre-wrap">${esc(r.data.body)}</p>`;
            list.appendChild(d);
        }
    }

    async function deleteComment(commentId, itemId) {
        if (!confirm('Delete this note?')) return;
        await Api.del('/api/items/'+itemId+'/comments/'+commentId);
        document.getElementById('cmt-'+commentId)?.remove();
    }

    return { postComment, deleteComment };
})();

// -----------------------------------------------------------------------
// Buyer inquiry page (dealer view)
// -----------------------------------------------------------------------
Pages.renderInquiries = async function(el) {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function ts(s) { return (s||'').slice(0,10); }

    const [sumR, inqR] = await Promise.all([
        Api.get('/api/inquiries/summary'),
        Api.get('/api/inquiries')
    ]);
    const sum = sumR.data || {};
    const inquiries = Array.isArray(inqR.data) ? inqR.data : [];

    const statusBadge = s => {
        const m = {open:'badge-amber', replied:'badge-blue', closed:'badge-gray'};
        return `<span class="badge ${m[s]||'badge-gray'}">${esc(s)}</span>`;
    };

    el.innerHTML = `
<div class="page-header">
  <h1 class="page-title">Buyer inquiries</h1>
</div>
<div class="stat-grid" style="margin-bottom:18px">
  <div class="stat-card"><div class="stat-label">Open</div><div class="stat-value amber">${sum.open||0}</div></div>
  <div class="stat-card"><div class="stat-label">Total</div><div class="stat-value">${sum.total||0}</div></div>
  <div class="stat-card"><div class="stat-label">Closed</div><div class="stat-value">${sum.closed||0}</div></div>
</div>
<div class="toolbar">
  <select id="inq-status" onchange="Pages.filterInquiries()">
    <option value="">All statuses</option>
    <option value="open">Open</option>
    <option value="replied">Replied</option>
    <option value="closed">Closed</option>
  </select>
</div>
<div class="table-wrap">
  <table id="inq-table">
    <thead><tr>
      <th>Item</th><th>Buyer</th><th>Subject</th>
      <th>Date</th><th>Replies</th><th>Status</th><th></th>
    </tr></thead>
    <tbody id="inq-tbody">
      ${inquiries.map(inq => `
<tr data-status="${inq.status}">
  <td>${esc(inq.item_title)}</td>
  <td>
    <strong>${esc(inq.buyer_name)}</strong><br>
    <span style="font-size:11px;color:var(--text2)">${esc(inq.buyer_email)}</span>
    ${inq.buyer_phone?`<br><span style="font-size:11px;color:var(--text2)">${esc(inq.buyer_phone)}</span>`:''}
  </td>
  <td>${esc(inq.subject||'(no subject)')}</td>
  <td>${ts(inq.created_at)}</td>
  <td style="text-align:center">${inq.reply_count||0}</td>
  <td>${statusBadge(inq.status)}</td>
  <td style="white-space:nowrap">
    <button class="btn" style="font-size:12px;padding:5px 10px"
      onclick="InquiryUI.openThread(${inq.id})">View / Reply</button>
  </td>
</tr>`).join('')}
    </tbody>
  </table>
</div>`;

    window._inquiries = inquiries;
};

Pages.filterInquiries = function() {
    const stat = document.getElementById('inq-status')?.value||'';
    document.querySelectorAll('#inq-tbody tr').forEach(tr => {
        const s = tr.dataset.status;
        tr.style.display = (!stat || s === stat) ? '' : 'none';
    });
};

const InquiryUI = (() => {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function ts(s) { return (s||'').slice(0,16).replace('T',' '); }

    async function openThread(inquiryId) {
        const r = await Api.get('/api/inquiries/'+inquiryId);
        if (!r.ok) return;
        const inq = r.data;
        const replies = inq.replies || [];

        const roleColor = { dealer:'var(--accent2)', admin:'var(--amber)', buyer:'var(--blue)' };

        document.getElementById('modal-title').textContent =
            'Inquiry: ' + (inq.subject || 'from ' + inq.buyer_name);
        document.getElementById('modal-body').innerHTML = `
<div style="margin-bottom:14px;background:var(--bg3);border-radius:6px;padding:12px">
  <div style="display:flex;gap:24px;flex-wrap:wrap;margin-bottom:8px">
    <div><span style="font-size:11px;color:var(--text2)">FROM</span><br>
      <strong>${esc(inq.buyer_name)}</strong>
      ${inq.buyer_email?`<span style="font-size:12px;color:var(--text2)"> &lt;${esc(inq.buyer_email)}&gt;</span>`:''}
      ${inq.buyer_phone?`<br><span style="font-size:12px;color:var(--text2)">${esc(inq.buyer_phone)}</span>`:''}
    </div>
    <div><span style="font-size:11px;color:var(--text2)">ITEM</span><br>
      <strong>${esc(inq.item_title)}</strong>
    </div>
    <div><span style="font-size:11px;color:var(--text2)">DATE</span><br>${ts(inq.created_at)}</div>
    <div><span style="font-size:11px;color:var(--text2)">STATUS</span><br>
      <select id="inq-status-sel" style="font-size:12px;padding:3px 6px"
        onchange="InquiryUI.updateStatus(${inq.id},this.value)">
        ${['open','replied','closed'].map(s=>
          `<option value="${s}"${inq.status===s?' selected':''}>${s}</option>`).join('')}
      </select>
    </div>
  </div>
  <p style="font-size:13px;white-space:pre-wrap;border-top:1px solid var(--border);padding-top:8px;margin-top:4px">${esc(inq.body)}</p>
</div>

<div id="reply-thread" style="display:flex;flex-direction:column;gap:8px;max-height:300px;overflow-y:auto;margin-bottom:14px">
${replies.map(rep => `
<div style="display:flex;flex-direction:column;align-items:${rep.author_role==='buyer'?'flex-start':'flex-end'}">
  <div style="max-width:80%;background:${rep.author_role==='buyer'?'var(--bg3)':'var(--bg2)'};
    border:1px solid var(--border);border-radius:8px;padding:10px 14px">
    <div style="display:flex;justify-content:space-between;gap:16px;margin-bottom:4px">
      <span style="font-size:12px;font-weight:600;color:${roleColor[rep.author_role]||'var(--text2)'}">
        ${esc(rep.author)}
        <span style="font-size:10px;font-weight:400;color:var(--text2)">(${rep.author_role})</span>
      </span>
      <span style="font-size:11px;color:var(--text2)">${ts(rep.created_at)}</span>
    </div>
    <p style="font-size:13px;white-space:pre-wrap">${esc(rep.body)}</p>
  </div>
</div>`).join('')}
${replies.length===0?'<p style="text-align:center;color:var(--text2);font-size:13px;padding:16px">No replies yet</p>':''}
</div>

<div style="display:flex;gap:8px">
  <textarea id="reply-body" rows="3" placeholder="Type your reply..."
    style="flex:1;resize:none;font-size:13px"></textarea>
  <div style="display:flex;flex-direction:column;gap:6px;justify-content:flex-end">
    <button class="btn" onclick="InquiryUI.postReply(${inq.id})">Send reply</button>
    <button class="btn-ghost" onclick="app.closeModal()">Close</button>
  </div>
</div>`;

        document.getElementById('modal').classList.remove('hidden');
        // Scroll to bottom of thread
        const thread = document.getElementById('reply-thread');
        if (thread) thread.scrollTop = thread.scrollHeight;
    }

    async function postReply(inquiryId) {
        const body = document.getElementById('reply-body')?.value?.trim();
        if (!body) return;
        const r = await Api.post('/api/inquiries/'+inquiryId+'/replies', { body });
        if (!r.ok) { alert('Error: '+(r.data.error||'send failed')); return; }
        document.getElementById('reply-body').value = '';
        // Inject reply into thread
        const thread = document.getElementById('reply-thread');
        if (thread) {
            const d = document.createElement('div');
            d.style.cssText = 'display:flex;flex-direction:column;align-items:flex-end';
            d.innerHTML = `<div style="max-width:80%;background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:10px 14px">
  <div style="display:flex;justify-content:space-between;gap:16px;margin-bottom:4px">
    <span style="font-size:12px;font-weight:600;color:var(--accent2)">${esc(r.data.author)} <span style="font-size:10px;font-weight:400;color:var(--text2)">(dealer)</span></span>
    <span style="font-size:11px;color:var(--text2)">just now</span>
  </div>
  <p style="font-size:13px;white-space:pre-wrap">${esc(r.data.body)}</p>
</div>`;
            thread.appendChild(d);
            thread.scrollTop = thread.scrollHeight;
        }
        // Update status badge in select
        const sel = document.getElementById('inq-status-sel');
        if (sel) sel.value = 'replied';
    }

    async function updateStatus(inquiryId, status) {
        await Api.put('/api/inquiries/'+inquiryId+'/status', { status });
    }

    return { openThread, postReply, updateStatus };
})();
