// Metis Antique Marketplace Plus - API client
const Api = (() => {
    const BASE = '';
    function token() { return localStorage.getItem('map_token') || ''; }
    async function req(method, path, body) {
        const opts = { method, headers: { 'Content-Type': 'application/json' } };
        const t = token();
        if (t) opts.headers['Authorization'] = 'Bearer ' + t;
        if (body !== undefined) opts.body = JSON.stringify(body);
        const r = await fetch(BASE + path, opts);
        const text = await r.text();
        let json;
        try { json = JSON.parse(text); } catch { json = { error: text }; }
        return { status: r.status, data: json, ok: r.ok };
    }
    return {
        get:        (path)       => req('GET',    path),
        post:       (path, body) => req('POST',   path, body),
        put:        (path, body) => req('PUT',    path, body),
        del:        (path)       => req('DELETE', path),
        setToken:   (t)          => localStorage.setItem('map_token', t),
        clearToken: ()           => localStorage.removeItem('map_token'),
        hasToken:   ()           => !!localStorage.getItem('map_token'),
        getConfig:  ()           => req('GET', '/api/config')
    };
})();

// Global currency config -- loaded from /api/config after login
var CurrencySymbol = '$';
var CurrencyCode   = 'USD';
var CurrencyLocale = 'en-US';
