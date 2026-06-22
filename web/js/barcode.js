// Metis Antique Marketplace Plus v1.2.48 - Code 128 Barcode renderer
// Pure JS, zero dependencies, generates SVG string
'use strict';

const Barcode128 = (() => {
    // Code 128B encoding table: character -> bar pattern (11 modules)
    const TABLE = {
        ' ':11011001100, '!':11001101100, '"':11001100110, '#':10010011000,
        '$':10010001100, '%':10001001100, '&':10011001000, "'":10011000100,
        '(':10001100100, ')':11001001000, '*':11001000100, '+':11000100100,
        ',':10110011100, '-':10011011100, '.':10011001110, '/':10111001100,
        '0':10011101100, '1':10011100110, '2':11001110010, '3':11001011100,
        '4':11001001110, '5':11011100100, '6':11001110100, '7':11101101110,
        '8':11101001100, '9':11100101100, ':':11100100110, ';':11101100100,
        '<':11100110100, '=':11100110010, '>':11110101110, '?':11110100110,
        '@':11100101110, 'A':11110100100, 'B':11110010100, 'C':11110010010,
        'D':11011011110, 'E':11011110110, 'F':11110110110, 'G':10101111000,
        'H':10100011110, 'I':10001011110, 'J':10111101000, 'K':10111100010,
        'L':11110101000, 'M':11110100010, 'N':10111011110, 'O':10111101110,
        'P':11101011110, 'Q':11110101100, 'R':10000101100, 'S':10000100110,
        'T':10000110100, 'U':10000110010, 'V':11000010010, 'W':11001010000,
        'X':10110001000, 'Y':10001101000, 'Z':10001100010, '[':11010000100,
        '\\':11000010100, ']':10001011000, '^':10001000110, '_':10110100000,
        '`':10110010000, 'a':11000100010, 'b':10001001000, 'c':10011101110,
        'd':10001110110, 'e':10001011100, 'f':11110011010, 'g':11110001010,
        'h':11100011010, 'i':11100010110, 'j':11101100010, 'k':11101000110,
        'l':11100011110, 'm':11100111010, 'n':10110001110, 'o':10110111000,
        'p':10110001100, 'q':11110011100, 'r':10011110110, 's':10011110010,
        't':10001111010, 'u':10001111100, 'v':11100111110, 'w':11110011110,
        'x':11000111010, 'y':11000011110, 'z':11001111010, '{':11001111100,
        '|':11001011110, '}':11001101110, '~':11010011110
    };

    // Special codes (numeric values for checksum)
    const START_B = 104;
    const STOP    = 106;
    // Code 128 patterns for special codes
    const SPECIAL = {
        104: 11010010000, // START B
        106: 11000111010  // STOP (note: actual stop is 13 modules but simplified here)
    };

    function charValue(ch) {
        const code = ch.charCodeAt(0) - 32;
        return code;
    }

    function encode(text) {
        // Build bit string
        let bits = '';
        let checksum = START_B;

        // Start code B pattern
        bits += toBits(11010010000, 11);

        for (let i = 0; i < text.length; i++) {
            const ch = text[i];
            const pat = TABLE[ch];
            if (pat === undefined) continue; // skip unsupported chars
            bits += toBits(pat, 11);
            checksum += charValue(ch) * (i + 1);
        }

        // Checksum symbol (checksum mod 103)
        const chkVal = checksum % 103;
        // For simplicity encode as the char at position chkVal+32
        const chkCh = String.fromCharCode(chkVal + 32);
        const chkPat = TABLE[chkCh];
        if (chkPat !== undefined) bits += toBits(chkPat, 11);

        // Stop: 13 modules 1100011101011
        bits += '1100011101011';

        return bits;
    }

    function toBits(n, width) {
        return n.toString(2).padStart(width, '0');
    }

    // Generate SVG string
    function toSVG(text, { height = 60, moduleWidth = 1.5, quiet = 10, showText = true } = {}) {
        const bits = encode(text);
        const totalModules = bits.length;
        const w = totalModules * moduleWidth + quiet * 2;
        const textH = showText ? 14 : 0;
        const h = height + textH + 4;

        let rects = '';
        let x = quiet;
        for (let i = 0; i < bits.length; i++) {
            if (bits[i] === '1') {
                // Find run length
                let run = 1;
                while (i + run < bits.length && bits[i + run] === '1') run++;
                rects += `<rect x="${(x + i * moduleWidth).toFixed(1)}" y="2" width="${(run * moduleWidth).toFixed(1)}" height="${height}" fill="#000"/>`;
                i += run - 1;
            }
            x += moduleWidth;
        }

        const textEl = showText
            ? `<text x="${(w / 2).toFixed(1)}" y="${height + textH}" font-family="monospace" font-size="10" text-anchor="middle" fill="#000">${text.replace(/&/g,'&amp;').replace(/</g,'&lt;')}</text>`
            : '';

        return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w.toFixed(1)} ${h}" width="${w.toFixed(1)}" height="${h}">
<rect width="${w.toFixed(1)}" height="${h}" fill="#fff"/>
${rects}
${textEl}
</svg>`;
    }

    return { toSVG, encode };
})();
