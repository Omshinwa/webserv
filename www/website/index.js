document.addEventListener('DOMContentLoaded', () => {
    // ---- Session login / logout (header) ----
    // The `sid` cookie is HttpOnly (JS can't read it); we read the
    // companion `user` cookie to know who is logged in.
    function getCookie(name) {
        const m = document.cookie.match(new RegExp('(?:^|; )' + name + '=([^;]*)'));
        return m ? decodeURIComponent(m[1]) : null;
    }

    async function doLogin() {
        const input = document.getElementById('login-name');
        const name = input.value.trim();
        if (!name) { input.focus(); return; }
        await fetch('/cgi-bin/session.py', { method: 'POST', body: name });
        renderAuth();      // server set the cookies -> reflect logged-in state
        renderGreeting();  // refresh the on-page greeting for the new session
    }

    async function doLogout() {
        console.log("LOGGED OUT")
        const msg = await (await fetch('/cgi-bin/session.py?action=logout')).text();
        renderAuth();      // cookies expired -> back to login
        document.getElementById('greeting').textContent = msg;
    }

    // render/rerenders buttons associated wit the authentifications
    function renderAuth() {
        const box = document.getElementById('auth-box');
        box.innerHTML = '';
        const user = getCookie('user');
        if (user) {
            const pill = document.createElement('span');
            pill.className = 'bg-teal-500/10 text-teal-300 px-3 py-2 rounded-full border border-teal-700 text-sm font-mono';
            pill.textContent = '👤 ' + user;   // textContent -> safe from HTML injection
            const btn = document.createElement('button');
            btn.className = 'bg-slate-700 hover:bg-slate-600 text-white px-4 py-2 rounded-lg text-sm font-semibold transition cursor-pointer';
            btn.textContent = 'Logout';
            btn.onclick = doLogout;
            box.append(pill, btn);
        } else {
            const input = document.createElement('input');
            input.id = 'login-name';
            input.type = 'text';
            input.placeholder = 'your name';
            input.className = 'bg-slate-900 border border-slate-700 rounded px-3 py-2 text-sm w-28 focus:outline-none focus:border-teal-400 text-slate-200';
            input.addEventListener('keydown', (e) => { if (e.key === 'Enter') doLogin(); });
            const btn = document.createElement('button');
            btn.className = 'bg-teal-600 hover:bg-teal-500 text-white px-4 py-2 rounded-lg text-sm font-semibold transition cursor-pointer';
            btn.textContent = 'Login';
            btn.onclick = doLogin;
            box.append(input, btn);
        }
    }

    async function renderGreeting() {
        const el = document.getElementById('greeting');
        const user = getCookie('user');
        if (!user) {
            el.textContent = '👋 Welcome, guest — log in above to start a session.';
            return;
        }
        // The GET bumps the per-session visit count; read it back to greet.
        const msg = await (await fetch('/cgi-bin/session.py')).text();
        el.textContent = msg;
    }

    renderAuth();
    renderGreeting();
});