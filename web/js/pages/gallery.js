// Metis Antique Marketplace Plus - Photo gallery and comment/inquiry pages
// Injected into Pages namespace by pages.js

Pages.renderGallery = async function(itemId, itemTitle, container) {
    const r = await Api.get('/api/items/' + itemId + '/photos');
    const photos = Array.isArray(r.data) ? r.data : [];
    container.innerHTML = `
<div style="margin-bottom:12px;display:flex;align-items:center;justify-content:space-between">
  <p style="font-size:12px;font-weight:600;color:var(--text2)">PHOTOS (${photos.length})</p>
  <label class="btn-ghost" style="cursor:pointer;font-size:12px;padding:5px 10px">
    &#x1F4F7; Add photos
    <input type="file" accept="image/*" multiple style="display:none"
      onchange="GalleryUI.uploadPhotos(${itemId}, this)">
  </label>
</div>
${photos.length === 0 ?
  '<div style="text-align:center;padding:32px;color:var(--text2);border:1px dashed var(--border);border-radius:6px">No photos yet — drag and drop or click Add photos</div>' :
  `<div id="photo-grid-${itemId}" style="display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:10px">
    ${photos.map(p => GalleryUI.photoCard(p, itemId)).join('')}
  </div>`
}`;
};

const GalleryUI = (() => {
    function photoCard(p, itemId) {
        return `<div id="photo-card-${p.id}" style="position:relative;border-radius:6px;overflow:hidden;border:${p.is_primary?'2px solid var(--accent)':'1px solid var(--border)'}">
  <img src="${esc(p.url)}" style="width:100%;aspect-ratio:1;object-fit:cover;display:block;cursor:pointer"
    onclick="GalleryUI.openLightbox('${esc(p.url)}','${esc(p.caption)}')">
  <div style="padding:6px 8px;background:var(--bg2)">
    <input type="text" value="${esc(p.caption)}" placeholder="Caption..."
      style="width:100%;font-size:11px;padding:2px 4px;border:1px solid var(--border);border-radius:3px;background:var(--bg3);color:var(--text)"
      onchange="GalleryUI.updateCaption(${p.id},${itemId},this.value)">
    <div style="display:flex;gap:4px;margin-top:5px">
      ${p.is_primary ?
        '<span style="font-size:10px;color:var(--accent);font-weight:600">&#9733; Primary</span>' :
        `<button class="btn-ghost" style="font-size:10px;padding:2px 6px" onclick="GalleryUI.setPrimary(${p.id},${itemId})">Set primary</button>`}
      <button class="btn-danger" style="font-size:10px;padding:2px 6px;margin-left:auto" onclick="GalleryUI.deletePhoto(${p.id},${itemId})">&#x2715;</button>
    </div>
  </div>
</div>`;
    }

    function esc(s) {
        return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    }

    async function uploadPhotos(itemId, input) {
        const files = Array.from(input.files);
        if (!files.length) return;
        const token = sessionStorage.getItem('map_token')||'';
        let uploaded = 0;
        for (const file of files) {
            const fd = new FormData();
            fd.append('photo', file, file.name);
            const r = await fetch('/api/items/'+itemId+'/photos', {
                method: 'POST',
                headers: { 'Authorization': 'Bearer '+token },
                body: fd
            });
            if (r.ok) uploaded++;
        }
        // Refresh gallery
        const cont = input.closest('[data-gallery]');
        if (cont) await Pages.renderGallery(itemId, '', cont);
        else location.reload(); // fallback
        if (uploaded < files.length)
            alert(`${uploaded}/${files.length} photos uploaded`);
    }

    async function updateCaption(photoId, itemId, caption) {
        await Api.put('/api/items/'+itemId+'/photos/'+photoId, { caption });
    }

    async function setPrimary(photoId, itemId) {
        await Api.put('/api/items/'+itemId+'/photos/'+photoId, { is_primary: true });
        // Refresh: update borders
        document.querySelectorAll('[id^="photo-card-"]').forEach(el => {
            el.style.border = '1px solid var(--border)';
        });
        const card = document.getElementById('photo-card-'+photoId);
        if (card) card.style.border = '2px solid var(--accent)';
    }

    async function deletePhoto(photoId, itemId) {
        if (!confirm('Delete this photo?')) return;
        await Api.del('/api/items/'+itemId+'/photos/'+photoId);
        const card = document.getElementById('photo-card-'+photoId);
        if (card) card.remove();
    }

    function openLightbox(url, caption) {
        const lb = document.createElement('div');
        lb.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,.9);z-index:999;display:flex;flex-direction:column;align-items:center;justify-content:center;cursor:pointer';
        lb.onclick = () => lb.remove();
        const img = document.createElement('img');
        img.src = url;
        img.style.cssText = 'max-width:90vw;max-height:85vh;object-fit:contain;border-radius:6px';
        lb.appendChild(img);
        if (caption) {
            const cap = document.createElement('p');
            cap.textContent = caption;
            cap.style.cssText = 'color:#fff;margin-top:12px;font-size:14px;text-align:center';
            lb.appendChild(cap);
        }
        const hint = document.createElement('p');
        hint.textContent = 'Click anywhere to close';
        hint.style.cssText = 'color:rgba(255,255,255,0.4);font-size:12px;margin-top:8px';
        lb.appendChild(hint);
        document.body.appendChild(lb);
    }

    return { photoCard, uploadPhotos, updateCaption, setPrimary, deletePhoto, openLightbox };
})();
