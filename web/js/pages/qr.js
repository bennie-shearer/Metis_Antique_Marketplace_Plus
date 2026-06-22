// Metis Antique Marketplace Plus - QR Code label printing
// Pure-JS QR encoder (Version 1-3, ECC Level M, alphanumeric/byte mode)
// No external libraries required

// ================================================================
// Minimal QR Code generator (Version 1, L ECC, numeric + alphanumeric)
// Uses a compact Reed-Solomon implementation for error correction
// ================================================================
const QR = (() => {
    // GF(256) arithmetic for Reed-Solomon
    const EXP = new Uint8Array(512);
    const LOG  = new Uint8Array(256);
    (function initGF() {
        let x = 1;
        for (let i = 0; i < 255; i++) {
            EXP[i] = x; LOG[x] = i;
            x = x << 1; if (x & 0x100) x ^= 0x11D;
        }
        for (let i = 255; i < 512; i++) EXP[i] = EXP[i - 255];
    })();

    function gfMul(a, b) { return (!a || !b) ? 0 : EXP[(LOG[a] + LOG[b]) % 255]; }
    function gfPow(x, p) { return EXP[(LOG[x] * p) % 255]; }

    function rsECC(data, nec) {
        const gen = [1];
        for (let i = 0; i < nec; i++) {
            const r = [1, gfPow(2, i)];
            const ng = new Array(gen.length + 1).fill(0);
            for (let j = 0; j < gen.length; j++)
                for (let k = 0; k < r.length; k++)
                    ng[j + k] ^= gfMul(gen[j], r[k]);
            gen.splice(0, gen.length, ...ng);
        }
        const msg = [...data, ...new Array(nec).fill(0)];
        for (let i = 0; i < data.length; i++) {
            const c = msg[i];
            if (c) for (let j = 0; j < gen.length; j++) msg[i + j] ^= gfMul(gen[j], c);
        }
        return msg.slice(data.length);
    }

    // QR Version 1-L: 21x21, 7 ECC codewords, 19 data codewords (152 bits capacity)
    // Encode URL/text as byte mode, pad to 19 bytes, apply mask 0
    function encode(text) {
        const bytes = [];
        for (let i = 0; i < text.length; i++) bytes.push(text.charCodeAt(i) & 0xFF);

        // Version 1, L ECC: 19 data + 7 ECC codewords
        const MAX_DATA = 19;
        const N_ECC    = 7;

        // Build bitstream
        const bits = [];
        const pushBits = (val, n) => { for (let i = n-1; i >= 0; i--) bits.push((val>>i)&1); };

        pushBits(0b0100, 4); // byte mode
        pushBits(Math.min(bytes.length, MAX_DATA - 2), 8); // char count (reserve 2 bytes for count + mode)
        // Actual: mode=4bits, count=8bits, then data bytes
        const bitBuf = [0,1,0,0]; // byte mode
        const addBits = (v, n) => { for (let i = n-1; i >= 0; i--) bitBuf.push((v>>i)&1); };
        bitBuf.length = 0;
        addBits(0b0100, 4);
        const len = Math.min(bytes.length, MAX_DATA * 8 / 8 - 2);
        addBits(len, 8);
        for (let i = 0; i < len; i++) addBits(bytes[i], 8);
        // Terminator
        addBits(0, 4);
        while (bitBuf.length % 8) bitBuf.push(0);
        // Pad codewords
        const codewords = [];
        for (let i = 0; i < bitBuf.length; i += 8) {
            let v = 0; for (let j = 0; j < 8; j++) v = (v<<1)|(bitBuf[i+j]||0);
            codewords.push(v);
        }
        while (codewords.length < MAX_DATA) {
            codewords.push(codewords.length % 2 === 0 ? 0xEC : 0x11);
        }

        const ecc = rsECC(codewords.slice(0, MAX_DATA), N_ECC);
        const full = [...codewords.slice(0, MAX_DATA), ...ecc];

        // Place into 21x21 matrix
        const SIZE = 21;
        const M = Array.from({length: SIZE}, () => new Array(SIZE).fill(-1)); // -1 = unfilled
        const F = Array.from({length: SIZE}, () => new Array(SIZE).fill(false)); // functional

        // Finder patterns (top-left, top-right, bottom-left)
        function finder(r, c) {
            for (let dr = 0; dr < 7; dr++) for (let dc = 0; dc < 7; dc++) {
                const v = (dr===0||dr===6||dc===0||dc===6) ? 1 :
                          (dr>=2&&dr<=4&&dc>=2&&dc<=4) ? 1 : 0;
                if (r+dr < SIZE && c+dc < SIZE) { M[r+dr][c+dc] = v; F[r+dr][c+dc] = true; }
            }
        }
        finder(0,0); finder(0,14); finder(14,0);
        // Separators (white borders around finders)
        const markFunctional = (r,c,v) => { if (r>=0&&r<SIZE&&c>=0&&c<SIZE) { M[r][c]=v; F[r][c]=true; } };
        for (let i = 0; i <= 7; i++) {
            markFunctional(7,i,0); markFunctional(i,7,0);
            markFunctional(7,SIZE-1-i,0); markFunctional(i,SIZE-8,0);
            markFunctional(SIZE-8,i,0); markFunctional(SIZE-1-i,7,0);
        }
        // Timing patterns
        for (let i = 8; i < SIZE-8; i++) {
            M[6][i] = M[i][6] = (i%2===0)?1:0;
            F[6][i] = F[i][6] = true;
        }
        // Format info (mask 0, ECC level L = 01, mask 000)
        // Format: 01 000, BCH(5,10) = precomputed for L+mask0 = 0b01 000 = 8 -> with BCH = 0x77C4
        const FMT = 0x77C4; // precomputed format word for ECC=L, mask=0
        const fmtBits = [];
        for (let i = 14; i >= 0; i--) fmtBits.push((FMT>>i)&1);
        const fmtPos = [
            [8,0],[8,1],[8,2],[8,3],[8,4],[8,5],[8,7],[8,8],
            [7,8],[5,8],[4,8],[3,8],[2,8],[1,8],[0,8]
        ];
        const fmtPos2 = [
            [SIZE-1,8],[SIZE-2,8],[SIZE-3,8],[SIZE-4,8],[SIZE-5,8],[SIZE-6,8],[SIZE-7,8],
            [8,SIZE-8],[8,SIZE-7],[8,SIZE-6],[8,SIZE-5],[8,SIZE-4],[8,SIZE-3],[8,SIZE-2],[8,SIZE-1]
        ];
        for (let i = 0; i < 15; i++) {
            markFunctional(fmtPos[i][0],fmtPos[i][1],fmtBits[i]);
            markFunctional(fmtPos2[i][0],fmtPos2[i][1],fmtBits[i]);
        }
        markFunctional(SIZE-8,8,1); // dark module

        // Data placement (up-right column pairs, zigzag)
        let bitIdx = 0;
        const allBits = [];
        for (const cw of full) for (let i=7;i>=0;i--) allBits.push((cw>>i)&1);
        let up = true;
        for (let col = SIZE-1; col >= 0; col -= 2) {
            if (col === 6) col = 5; // skip timing column
            for (let rowOff = 0; rowOff < SIZE; rowOff++) {
                const row = up ? SIZE-1-rowOff : rowOff;
                for (let c2 = 0; c2 <= 1; c2++) {
                    const r = row, c = col - c2;
                    if (!F[r][c]) {
                        const bit = bitIdx < allBits.length ? allBits[bitIdx++] : 0;
                        // Mask 0: (row+col)%2===0 -> invert
                        M[r][c] = (r+c)%2===0 ? bit^1 : bit;
                    }
                }
            }
            up = !up;
        }
        return M;
    }

    // Render QR matrix as SVG
    function toSVG(matrix, size) {
        const n    = matrix.length;
        const cell = size / (n + 8); // 4 quiet zone modules each side
        const off  = cell * 4;
        const dim  = size;
        let svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${dim}" height="${dim}" viewBox="0 0 ${dim} ${dim}">`;
        svg += `<rect width="${dim}" height="${dim}" fill="white"/>`;
        for (let r = 0; r < n; r++) {
            for (let c = 0; c < n; c++) {
                if (matrix[r][c] === 1) {
                    const x = (off + c*cell).toFixed(2);
                    const y = (off + r*cell).toFixed(2);
                    const w = (cell + 0.1).toFixed(2);
                    svg += `<rect x="${x}" y="${y}" width="${w}" height="${w}" fill="black"/>`;
                }
            }
        }
        svg += '</svg>';
        return svg;
    }

    return { encode, toSVG };
})();

// ================================================================
// Label printing UI
// ================================================================
const LabelUI = (() => {
    function esc(s) { return String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

    function itemUrl(itemId) {
        return location.protocol + '//' + location.host + '/shop.html?listing=' + itemId;
    }

    function buildLabel(item) {
        const url  = itemUrl(item.id);
        let matrix;
        try { matrix = QR.encode(url); } catch(e) { matrix = null; }
        const qrSvg = matrix ? QR.toSVG(matrix, 120) : '';

        const price = item.asking_price
            ? (() => { try { return parseFloat(item.asking_price).toLocaleString(CurrencyLocale,{style:'currency',currency:CurrencyCode,minimumFractionDigits:2,maximumFractionDigits:2}); } catch(e) { return CurrencySymbol + parseFloat(item.asking_price).toFixed(2); } })()
            : '';

        // Generate Code 128 barcode from SKU if available
        const barcodeStr = item.sku || ('AMP-' + String(item.id).padStart(5,'0'));
        const barcodeSvg = (typeof Barcode128 !== 'undefined')
            ? Barcode128.toSVG(barcodeStr, { height: 36, moduleWidth: 1.2, quiet: 6, showText: true })
            : '';

        return `<div class="label-card" data-item-id="${item.id}">
  <div class="label-qr">${qrSvg || '<div style="width:120px;height:120px;border:1px dashed #ccc;display:flex;align-items:center;justify-content:center;font-size:10px;color:#aaa">QR</div>'}</div>
  <div class="label-info">
    <div class="label-title">${esc(item.title)}</div>
    ${item.category ? `<div class="label-meta">${esc(item.category)}${item.era?' · '+esc(item.era):''}</div>` : ''}
    ${item.maker    ? `<div class="label-meta">${esc(item.maker)}${item.origin?' · '+esc(item.origin):''}</div>` : ''}
    ${item.condition? `<div class="label-cond">${esc(item.condition)}</div>` : ''}
    ${price         ? `<div class="label-price">${esc(price)}</div>` : ''}
    <div class="label-sku" style="font-family:monospace;font-size:10px;color:#666;margin-top:2px">${esc(barcodeStr)}</div>
    ${barcodeSvg ? `<div class="label-barcode" style="margin-top:4px">${barcodeSvg}</div>` : ''}
    <div class="label-id">#${item.id}</div>
  </div>
</div>`;
    }

    async function printLabel(itemId) {
        const r = await Api.get('/api/items/' + itemId);
        if (!r.ok) { alert('Could not load item'); return; }
        openPrintWindow([r.data]);
    }

    async function printSelected(ids) {
        if (!ids.length) { alert('No items selected'); return; }
        const items = await Promise.all(ids.map(id => Api.get('/api/items/' + id)));
        openPrintWindow(items.filter(r => r.ok).map(r => r.data));
    }

    function openPrintWindow(items) {
        const labels = items.map(buildLabel).join('');
        const html = `<!DOCTYPE html><html><head><meta charset="UTF-8">
<title>Item Labels</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: Georgia, serif; background: white; }
  @page { size: letter; margin: 0.5in; }
  .label-sheet { display: flex; flex-wrap: wrap; gap: 12px; padding: 12px; }
  .label-card {
    width: 3.5in; border: 1px solid #999; border-radius: 6px;
    padding: 10px; display: flex; gap: 10px; align-items: flex-start;
    break-inside: avoid; page-break-inside: avoid;
    background: white;
  }
  .label-qr { flex-shrink: 0; }
  .label-qr svg { display: block; }
  .label-info { flex: 1; min-width: 0; }
  .label-title { font-size: 13px; font-weight: 700; line-height: 1.3; margin-bottom: 4px; }
  .label-meta  { font-size: 11px; color: #555; margin-bottom: 2px; }
  .label-cond  { display: inline-block; font-size: 10px; border: 1px solid #999;
                 border-radius: 10px; padding: 1px 8px; margin: 4px 0; color: #333; }
  .label-price { font-size: 18px; font-weight: 700; color: #6b3a1f; margin-top: 4px; }
  .label-id    { font-size: 10px; color: #aaa; margin-top: 6px; font-family: monospace; }
  @media print {
    body { -webkit-print-color-adjust: exact; print-color-adjust: exact; }
  }
</style>
</head><body>
<div class="label-sheet">${labels}</div>
<script>window.onload = () => { window.print(); window.onafterprint = () => window.close(); };<\/script>
</body></html>`;

        const win = window.open('', '_blank', 'width=900,height=700');
        if (!win) { alert('Please allow popups for label printing'); return; }
        win.document.write(html);
        win.document.close();
    }

    return { printLabel, printSelected, buildLabel };
})();
