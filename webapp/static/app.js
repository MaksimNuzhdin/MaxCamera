'use strict';
const $ = id => document.getElementById(id);
let csrf = '',
    catalog = [],
    state = {
        connected: false,
        streaming: false,
        values: {}
    },
    tab = 'quick',
    working = false,
    objectUrl = '',
    frameNumber = '',
    lastStatus = 0;
let starred = new Set(JSON.parse(localStorage.getItem('maxcam-stars') || '["ExposureTime","GainFactor","FpgaBlackLevel"]'));
const QUICK = ['Width', 'Height', 'PixelFormat', 'AcquisitionFrameRate', 'ExposureAuto', 'ExposureTime', 'GainAuto', 'GainFactor', 'BalanceWhiteAuto', 'FpgaProcessingEnable', 'FpgaBlackLevel'];
let toastTimer;

function toast(message) {
    $('toast').textContent = message;
    $('toast').hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => $('toast').hidden = true, 4500);
}

function notice(message, error = false) {
    $('notice').textContent = message;
    $('notice').hidden = !message;
    $('notice').classList.toggle('error', error);
}
async function api(path, data) {
    const options = {
        credentials: 'same-origin',
        headers: {}
    };
    if (data !== undefined) {
        options.method = 'POST';
        options.headers = {
            'Content-Type': 'application/json',
            'X-CSRF-Token': csrf
        };
        options.body = JSON.stringify(data);
    }
    const response = await fetch('/api/' + path, options);
    let body = {};
    if (response.headers.get('content-type')?.includes('application/json')) body = await response.json();
    if (!response.ok) {
        if (response.status === 401 && path !== 'login') showLogin();
        throw Error(body.detail || `Request failed (${response.status})`);
    }
    return body;
}

function showLogin() {
    $('workspace').hidden = true;
    $('login').hidden = false;
    csrf = '';
    state = {
        connected: false,
        streaming: false,
        values: {}
    };
    clearPreview();
}
async function showWorkspace() {
    catalog = await api('catalog');
    $('login').hidden = true;
    $('workspace').hidden = false;
    $('password').value = '';
    $('feature-count').textContent = catalog.length;
    const groups = [...new Set(catalog.map(f => f.group))];
    $('group').replaceChildren(new Option('All groups', ''), ...groups.map(g => new Option(label(g), g)));
    renderFeatures();
    await pollStatus();
}
$('login-form').addEventListener('submit', async e => {
    e.preventDefault();
    const button = e.submitter;
    button.disabled = true;
    $('login-error').textContent = '';
    try {
        const result = await api('login', {
            username: $('username').value,
            password: $('password').value
        });
        csrf = result.csrf;
        await showWorkspace();
    } catch (err) {
        $('login-error').textContent = err.message;
    } finally {
        button.disabled = false;
    }
});
$('show-password').onclick = () => {
    const visible = $('password').type === 'password';
    $('password').type = visible ? 'text' : 'password';
    $('show-password').textContent = visible ? 'Hide' : 'Show';
    $('show-password').setAttribute('aria-label', visible ? 'Hide password' : 'Show password');
};

function label(name) {
    return name.replace(/^Fpga/, 'FPGA ').replace(/([a-z0-9])([A-Z])/g, '$1 $2').replace(/([A-Z])([A-Z][a-z])/g, '$1 $2').replace(/User Set/g, 'UserSet');
}

function element(tag, cls, text) {
    const el = document.createElement(tag);
    if (cls) el.className = cls;
    if (text !== undefined) el.textContent = text;
    return el;
}

function format(value) {
    if (value === undefined) return '—';
    if (typeof value === 'boolean') return value ? 'On' : 'Off';
    if (typeof value === 'number') return Number.isInteger(value) ? String(value) : Number(value.toFixed(4)).toString();
    return String(value);
}

function bounds(f) {
    let lo = f.minimum,
        hi = f.maximum,
        step = 1;
    if (f.kind === 'Float') {
        lo /= f.scale || 1;
        hi /= f.scale || 1;
        step = 1 / (f.scale || 1);
    }
    if (f.kind === 'TemperatureADC') {
        lo = -40;
        hi = 125;
        step = .1;
    }
    if (f.name === 'ExposureTime') {
        const period = state.values.SensorLinePeriod || 32;
        lo = period;
        hi = (state.values.ExposureMaximumLines || 65531) * period;
        step = period;
    }
    if (f.name === 'Width') {
        hi = state.values.WidthMaximum || 2592;
        step = state.values.WidthIncrement || 2;
    }
    if (f.name === 'Height') {
        hi = state.values.HeightMaximum || 1944;
        step = 2;
    }
    return {
        lo,
        hi,
        step
    };
}

function unavailable(f) {
    if (state.feature_errors?.[f.name]) return true;
    const dep = f.available?.replace(/Register$/, '');
    return dep && state.values[dep] !== undefined && !state.values[dep];
}

function renderFeatures() {
    const root = $('features');
    const scroll = root.scrollTop;
    root.replaceChildren();
    const query = $('search').value.toLowerCase();
    let list = catalog.filter(f => (tab !== 'quick' || QUICK.includes(f.name)) && (tab !== 'favorites' || starred.has(f.name)) && (!$('group').value || tab === 'quick' || f.group === $('group').value) && (!query || (f.name + ' ' + f.group + ' ' + f.description).toLowerCase().includes(query)));
    if (query) list = catalog.filter(f => (f.name + ' ' + label(f.name) + ' ' + f.group + ' ' + f.description).toLowerCase().includes(query));
    const groups = [...new Set(list.map(f => tab === 'quick' ? (f.name.startsWith('Fpga') ? 'FPGA processing' : f.group === 'ImageFormatControl' ? 'Image format' : f.group === 'SensorTiming' ? 'Acquisition timing' : f.group === 'WhiteBalance' ? 'White balance' : 'Exposure & gain') : label(f.group)))];
    for (const group of groups) {
        const items = list.filter(f => (tab === 'quick' ? (f.name.startsWith('Fpga') ? 'FPGA processing' : f.group === 'ImageFormatControl' ? 'Image format' : f.group === 'SensorTiming' ? 'Acquisition timing' : f.group === 'WhiteBalance' ? 'White balance' : 'Exposure & gain') : label(f.group)) === group);
        const heading = element('div', 'feature-group-title', group);
        heading.append(element('span', '', String(items.length).padStart(2, '0')));
        root.append(heading);
        for (const f of items) root.append(featureControl(f));
    }
    if (!list.length) root.append(element('p', 'features-empty', tab === 'favorites' ? 'Star a feature to keep it close at hand.' : 'No matching features. Try another search.'));
    root.scrollTop = scroll;
}

function featureControl(f) {
    const wrapper = element('div', 'feature');
    const header = element('div', 'feature-label');
    const name = element('label', '', label(f.name));
    name.title = f.description || f.name;
    name.htmlFor = 'feature-' + f.name;
    header.append(name);
    const star = element('button', 'star' + (starred.has(f.name) ? ' selected' : ''), starred.has(f.name) ? '★' : '☆');
    star.title = 'Star ' + f.name;
    star.setAttribute('aria-label', star.title);
    star.onclick = () => {
        if (starred.has(f.name)) starred.delete(f.name);
        else starred.add(f.name);
        localStorage.setItem('maxcam-stars', JSON.stringify([...starred]));
        renderFeatures();
    };
    header.append(star);
    wrapper.append(header);
    const value = state.values[f.name],
        blocked = !f.remote_writable,
        absent = unavailable(f);
    const autoLocked = (f.name === 'ExposureTime' && state.values.ExposureAuto !== 0) || (f.name === 'GainFactor' && state.values.GainAuto !== 0) || (f.name.startsWith('WhiteBalance') && state.values.BalanceWhiteAuto !== 0);
    const disabled = !state.connected || state.streaming || working || absent || autoLocked;
    const row = element('div', 'feature-row');
    if (blocked) {
        let text = format(value);
        if (f.entries) text = Object.keys(f.entries).find(k => f.entries[k] === value) || text;
        if (f.unit && value !== undefined) text += ' ' + f.unit;
        const output = element('output', 'readonly-value', text);
        output.id = 'feature-' + f.name;
        row.append(output, element('span', 'badge', f.access === 'RO' ? 'READ ONLY' : 'LOCAL ONLY'));
    } else if (f.kind === 'Command') {
        const run = element('button', 'secondary', 'Execute ' + label(f.name));
        run.disabled = disabled;
        run.onclick = () => {
            if (window.confirm(`Execute ${f.name}? ${f.description||'This changes the current camera state.'}`)) apply(f, 1);
        };
        row.append(run);
    } else {
        let input;
        const b = bounds(f);
        if (f.entries) {
            input = element('select');
            if (value === undefined) input.append(new Option('Awaiting camera…', ''));
            for (const [name, number] of Object.entries(f.entries)) input.append(new Option(label(name), String(number)));
            if (value !== undefined) input.value = String(value);
        } else if (f.kind === 'Boolean') {
            input = element('input');
            input.type = 'checkbox';
            input.checked = !!value;
        } else {
            input = element('input');
            input.type = 'number';
            input.min = String(b.lo);
            input.max = String(b.hi);
            input.step = String(b.step);
            if (value !== undefined) input.value = format(value);
            input.placeholder = f.unit || '—';
            input.setAttribute('aria-label', label(f.name) + (f.unit ? ' (' + f.unit + ')' : ''));
        }
        input.id = 'feature-' + f.name;
        input.disabled = disabled;
        row.append(input);
        if (f.unit) row.append(element('span', 'badge', f.unit));
        const button = element('button', 'secondary', 'Apply');
        button.disabled = disabled;
        const submit = () => {
            if (input.type === 'number' && !input.checkValidity()) {
                input.reportValidity();
                return;
            }
            apply(f, input.type === 'checkbox' ? (input.checked ? 1 : 0) : Number(input.value));
        };
        button.onclick = submit;
        input.onkeydown = e => {
            if (e.key === 'Enter') submit();
        };
        row.append(button);
        wrapper.append(row);
        if (!f.entries && f.kind !== 'Boolean' && b.hi > b.lo && b.hi < 1e7) {
            const range = element('input', 'feature-range');
            range.type = 'range';
            range.min = String(b.lo);
            range.max = String(b.hi);
            range.step = String(b.step);
            range.value = String(value ?? b.lo);
            range.disabled = disabled;
            range.setAttribute('aria-label', label(f.name) + ' slider');
            range.oninput = () => {
                input.value = range.value;
            };
            input.oninput = () => {
                range.value = input.value;
            };
            wrapper.append(range);
            const rangeLabels = element('div', 'bounds');
            rangeLabels.append(element('span', '', format(b.lo)), element('span', '', format(b.hi) + (f.unit ? ' ' + f.unit : '')));
            wrapper.append(rangeLabels);
        }
        if (absent) wrapper.append(element('p', 'description', 'Unavailable on this hardware or current configuration.'));
        else if (autoLocked && state.connected) wrapper.append(element('p', 'description', 'Set the corresponding automatic control to Off for manual adjustment.'));
        else if (f.description && tab !== 'quick') wrapper.append(element('p', 'description', f.description));
        return wrapper;
    }
    wrapper.append(row);
    if (blocked && f.access !== 'RO') wrapper.append(element('p', 'description', 'Restricted to local maintenance for network and device protection.'));
    if (absent) wrapper.append(element('p', 'description', 'Unavailable on this hardware or current configuration.'));
    return wrapper;
}
async function action(name, data = {}, success) {
    if (working) return;
    working = true;
    updateButtons();
    renderFeatures();
    try {
        await api(name, data);
        notice('');
        if (success) toast(success);
        await pollStatus(true);
    } catch (err) {
        notice(err.message, true);
    } finally {
        working = false;
        updateButtons();
        renderFeatures();
    }
}
async function apply(f, value) {
    await action('feature', {
        name: f.name,
        value
    }, label(f.name) + ' updated');
}
$('connect').onclick = () => action('connect', {}, 'Camera connected. You have exclusive control.');
$('release').onclick = () => action('release', {}, 'Camera released and transport restored.');
$('start').onclick = () => action('start');
$('stop').onclick = () => action('stop');
$('refresh').onclick = () => action('refresh', {}, 'Camera parameters read.');
$('logout').onclick = async () => {
    try {
        await api('logout', {});
        showLogin();
    } catch (err) {
        notice(err.message, true);
    }
};

function updateButtons() {
    const c = state.connected,
        s = state.streaming;
    $('preview-note').textContent = s ? 'COMPLETE FRAMES ONLY' : 'AWAITING STREAM';
    document.querySelector('.stream-empty .mono').textContent = c ? 'CONTROL ACQUIRED · STREAM ' + (s ? 'STARTING' : 'STOPPED') : 'NO CAMERA CONTROL HELD';
    document.querySelector('.stream-empty h3').textContent = s ? 'Acquiring your first frame.' : c ? 'Camera connected.' : 'Ready when you are.';
    document.querySelector('.stream-empty p').textContent = s ? 'Waiting for the first complete image from your camera.' : c ? 'Start acquisition to see the live camera feed.' : 'Connect your camera, then start acquisition.';
    $('connect').disabled = c || working;
    $('connect').textContent = working ? 'Working…' : 'Connect camera ↗';
    $('release').disabled = !c || working;
    $('start').disabled = !c || s || working;
    $('stop').disabled = !c || !s || working;
    $('refresh').disabled = !c || s || working;
    $('snapshot').disabled = !s || !objectUrl;
    $('export').disabled = !c || working;
    $('connection-state').textContent = c ? 'Connected' : state.busy ? 'Busy' : 'Disconnected';
    $('connection-state').classList.toggle('online', c);
    $('side-state').textContent = c ? '● Control acquired' : state.busy ? '○ In use by another session' : '○ Disconnected';
    $('stream-state').textContent = s ? 'ACQUIRING' : c ? 'CONNECTED' : 'STANDBY';
    $('live-dot').style.background = s ? '#b5ef73' : '#64707e';
    $('edit-hint').textContent = s ? 'Stop stream to edit parameters.' : c ? 'Runtime settings · changes apply immediately.' : 'Connect to read camera settings.';
}
async function pollStatus(force = false) {
    if (!csrf) return;
    try {
        const next = await api('status');
        const rerender = force || next.connected !== state.connected || next.streaming !== state.streaming;
        state = next;
        lastStatus = Date.now();
        updateButtons();
        $('stat-fps').replaceChildren(document.createTextNode(next.streaming ? format(next.fps) : '—'), element('small', '', ' fps'));
        const w = next.values.Width,
            h = next.values.Height;
        $('stat-resolution').textContent = w && h ? `${w} × ${h}` : '—';
        $('image-size').textContent = w && h ? `${w} × ${h}` : '';
        $('stat-frames').textContent = next.frames.toLocaleString();
        $('stat-drops').textContent = `${next.incomplete} incomplete observed`;
        $('stat-lease').textContent = next.connected ? `${Math.floor(next.lease_remaining/60)}:${String(next.lease_remaining%60).padStart(2,'0')}` : '—';
        $('last-sync').textContent = 'STATUS SYNCED / ' + new Date().toLocaleTimeString('en-GB');
        if (next.error) notice(next.error, true);
        if (!next.streaming) clearPreview();
        if (rerender && !working) renderFeatures();
    } catch (err) {
        notice(err.message, true);
        if (Date.now() - lastStatus > 4000) clearPreview();
    }
}

function clearPreview() {
    if (objectUrl) {
        URL.revokeObjectURL(objectUrl);
        objectUrl = '';
    }
    $('preview').hidden = true;
    $('preview').removeAttribute('src');
    $('stream-empty').hidden = false;
    $('view-tag').hidden = true;
    frameNumber = '';
}
async function frameLoop() {
    if (csrf && state.streaming && !document.hidden) {
        try {
            const response = await fetch('/api/frame', {
                credentials: 'same-origin'
            });
            if (response.ok) {
                const number = response.headers.get('x-frame-number');
                if (number !== frameNumber) {
                    const blob = await response.blob();
                    const next = URL.createObjectURL(blob);
                    $('preview').src = next;
                    $('preview').hidden = false;
                    $('stream-empty').hidden = true;
                    $('view-tag').hidden = false;
                    if (objectUrl) URL.revokeObjectURL(objectUrl);
                    objectUrl = next;
                    frameNumber = number;
                    $('snapshot').disabled = false;
                }
            } else if (response.status === 503) {
                clearPreview();
                notice('Stream is stale. Waiting for a complete frame.', true);
            }
        } catch {
            clearPreview();
        }
    }
    setTimeout(frameLoop, 150);
}
$('search').oninput = () => renderFeatures();
$('group').onchange = () => renderFeatures();

function selectTab(name) {
    tab = name;
    for (const n of ['quick', 'all', 'favorites']) $(n + '-tab').classList.toggle('active', name === n);
    $('group').hidden = name !== 'all';
    renderFeatures();
}
$('quick-tab').onclick = () => selectTab('quick');
$('all-tab').onclick = () => selectTab('all');
$('favorites-tab').onclick = () => selectTab('favorites');
$('live-nav').onclick = () => {
    selectTab('quick');
    document.querySelector('.stream-panel').scrollIntoView({
        behavior: 'smooth'
    });
};
$('features-nav').onclick = () => {
    selectTab('all');
    $('feature-panel').scrollIntoView({
        behavior: 'smooth'
    });
    $('search').focus();
};
$('diagnostics-nav').onclick = () => {
    selectTab('all');
    $('group').value = 'Diagnostics';
    renderFeatures();
    $('feature-panel').scrollIntoView({
        behavior: 'smooth'
    });
};
$('grid-btn').onclick = () => {
    const show = $('composition-grid').hidden;
    $('composition-grid').hidden = !show;
    $('grid-btn').classList.toggle('active', show);
    $('grid-btn').setAttribute('aria-pressed', String(show));
};
$('fit-btn').onclick = () => {
    const actual = $('viewport').classList.toggle('actual');
    $('fit-btn').classList.toggle('active', actual);
    $('fit-btn').textContent = actual ? 'Fit' : '1:1';
};
$('fullscreen-btn').onclick = () => {
    if (document.fullscreenElement) document.exitFullscreen();
    else $('viewport').requestFullscreen?.().catch(() => toast('Fullscreen is unavailable in this browser.'));
};

function download(url, name) {
    const a = element('a');
    a.href = url;
    a.download = name;
    document.body.append(a);
    a.click();
    a.remove();
}
$('snapshot').onclick = () => {
    if (objectUrl) download(objectUrl, 'maxcamera-preview-' + new Date().toISOString().replace(/[:.]/g, '-') + '.jpg');
};
$('export').onclick = () => download('/api/settings', 'maxcamera-settings.json');
$('guide-btn').onclick = () => $('guide').showModal();
$('close-guide').onclick = () => $('guide').close();
document.addEventListener('keydown', e => {
    if (e.key === '/' && !['INPUT', 'SELECT', 'TEXTAREA'].includes(document.activeElement.tagName) && csrf) {
        e.preventDefault();
        $('search').focus();
    }
});
window.addEventListener('pagehide', () => {
    if (csrf && state.connected) fetch('/api/release', {
        method: 'POST',
        keepalive: true,
        headers: {
            'Content-Type': 'application/json',
            'X-CSRF-Token': csrf
        },
        body: '{}'
    }).catch(() => {});
});
(async () => {
    try {
        const result = await api('session');
        csrf = result.csrf;
        await showWorkspace();
    } catch {
        showLogin();
    }
    setInterval(() => {
        if (!document.hidden) pollStatus();
    }, 2000);
    frameLoop();
})();
