/**
 * app.js — Eilik Web UI
 * Live status dashboard + control API
 */

const BASE = '';  // same-origin

// =================== TOAST ===================
let toastTimer = null;
function toast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 2000);
}

// =================== STATE ===================
let lastState = {};
let stateTimer = null;

const MOOD_ICONS = {
  NEUTRAL: '😐', HAPPY: '😊', ANGRY: '😠', SLEEPY: '😴',
  CURIOUS: '🤔', CONFUSED: '😕', WAKE_UP: '🌅'
};
const MODE_ICONS = {
  0: { icon: '😴', name: 'SLEEP' },
  1: { icon: '😌', name: 'CALM' },
  2: { icon: '🤔', name: 'CURIOUS' },
  3: { icon: '🤩', name: 'PLAYFUL' }
};

function updateDashboard(s) {
  lastState = s;

  // Robot face
  const moodEl = document.getElementById('mood-tag');
  const faceEl = document.getElementById('robot-face');
  if (s.mood) {
    moodEl.textContent = s.mood;
    faceEl.textContent = MOOD_ICONS[s.mood] || '😐';
    // Color mood
    const colors = {
      HAPPY:'#00ffd5', ANGRY:'#ff5577', SLEEPY:'#ffbb33',
      CURIOUS:'#00aaff', CONFUSED:'#ffbb33', WAKE_UP:'#ffaa00', NEUTRAL:'#00ffd5'
    };
    moodEl.style.background = colors[s.mood] || '#007a68';
    moodEl.style.color = '#050b0f';
  }

  // Stats
  if (s.pan  !== undefined) document.getElementById('stat-pan').textContent = Math.round(s.pan);
  if (s.tilt !== undefined) document.getElementById('stat-tilt').textContent = Math.round(s.tilt);

  // Mode
  const modeEl = document.getElementById('stat-mode');
  if (s.mode !== undefined) {
    const m = MODE_ICONS[s.mode] || MODE_ICONS[1];
    modeEl.textContent = m.name;
    // Highlight active mode card
    document.querySelectorAll('.mode-card').forEach(c => {
      c.classList.toggle('active', c.dataset.v === m.name);
    });
  }

  // Uptime
  if (s.uptime_s !== undefined) {
    const d = Math.floor(s.uptime_s / 86400);
    const h = Math.floor((s.uptime_s % 86400) / 3600);
    const m = Math.floor((s.uptime_s % 3600) / 60);
    const el = document.getElementById('stat-uptime');
    el.textContent = d > 0 ? `${d}d${h}h` : (h > 0 ? `${h}h${m}m` : `${m}m`);
  }

  // Clients
  if (s.clients !== undefined) document.getElementById('stat-clients').textContent = s.clients;

  // Heap
  if (s.heap !== undefined && s.min_heap !== undefined) {
    const pct = Math.round((s.heap / 327680) * 100);
    document.getElementById('stat-heap').textContent = pct;
  }

  // IP / SSID
  if (s.ip)  document.getElementById('stat-ip').textContent = s.ip;
  if (s.ssid) document.getElementById('stat-ssid').textContent = s.ssid;

  // Moving bar
  const bar = document.getElementById('moving-bar');
  bar.classList.toggle('show', s.moving === true);

  // Firmware
  if (s.fw) document.getElementById('fw-ver').textContent = s.fw;

  // Online
  const led = document.getElementById('led');
  const conn = document.getElementById('conn-text');
  led.classList.add('on');
  conn.textContent = 'online';
}

async function fetchState() {
  try {
    const r = await fetch(`${BASE}/api/state`);
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const s = await r.json();
    updateDashboard(s);
  } catch (e) {
    const led = document.getElementById('led');
    const conn = document.getElementById('conn-text');
    led.classList.remove('on');
    conn.textContent = 'offline';
  }
}

// =================== POST helpers ===================
async function post(url, body) {
  try {
    const r = await fetch(`${BASE}${url}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    const j = await r.json().catch(() => ({}));
    if (!j.ok) toast('⚠ ' + (j.err || 'Error'));
    return j;
  } catch (e) {
    toast('⚠ Network error');
    return { ok: false };
  }
}

// =================== TABS ===================
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
    // Fetch state when switching to state tab
    if (btn.dataset.tab === 'state') fetchState();
  });
});

// =================== D-PAD ===================
document.querySelectorAll('.dbtn').forEach(btn => {
  btn.addEventListener('click', async () => {
    if (btn.id === 'center-btn') {
      await post('/api/center', {});
      toast('Centered ✓');
      return;
    }
    // Nudge từ vị trí hiện tại
    const pan  = parseFloat(btn.dataset.pan  || 0);
    const tilt = parseFloat(btn.dataset.tilt || 0);
    await post('/api/head', {
      pan:  (lastState.pan  || 90) + pan,
      tilt: (lastState.tilt || 90) + tilt
    });
    toast(`Pan ${pan > 0 ? '+' : ''}${pan}°  Tilt ${tilt > 0 ? '+' : ''}${tilt}°`);
  });
});

// =================== SLIDERS + GO ===================
const panSlider  = document.getElementById('pan-slider');
const tiltSlider = document.getElementById('tilt-slider');
const panVal     = document.getElementById('pan-val');
const tiltVal    = document.getElementById('tilt-val');

panSlider.addEventListener('input',  () => { panVal.textContent  = panSlider.value; });
tiltSlider.addEventListener('input', () => { tiltVal.textContent = tiltSlider.value; });

// Sync sliders với state khi vừa kết nối
function syncSlidersFromState() {
  if (lastState.pan  !== undefined) { panSlider.value  = lastState.pan;  panVal.textContent  = Math.round(lastState.pan); }
  if (lastState.tilt !== undefined) { tiltSlider.value = lastState.tilt; tiltVal.textContent = Math.round(lastState.tilt); }
}

document.getElementById('head-go').addEventListener('click', async () => {
  const pan  = parseInt(panSlider.value);
  const tilt = parseInt(tiltSlider.value);
  await post('/api/head', { pan, tilt });
  toast(`Head: ${pan}° / ${tilt}°`);
});

// =================== EMOTION / GESTURE / MODE buttons ===================
document.querySelectorAll('[data-act]').forEach(btn => {
  btn.addEventListener('click', async () => {
    const act = btn.dataset.act;
    const val = btn.dataset.v;
    await post(`/api/${act}`, { value: val });
    toast(`${act.toUpperCase()}: ${val}`);
  });
});

// =================== CALIB SLIDERS ===================
const offPan  = document.getElementById('off-pan');
const offTilt = document.getElementById('off-tilt');
document.getElementById('off-pan').addEventListener('input',  () => { document.getElementById('off-pan-val').textContent  = offPan.value;  });
document.getElementById('off-tilt').addEventListener('input', () => { document.getElementById('off-tilt-val').textContent = offTilt.value; });

document.getElementById('calib-save').addEventListener('click', async () => {
  const r = await post('/api/calib', {
    pan_off:  parseInt(offPan.value),
    tilt_off: parseInt(offTilt.value)
  });
  if (r.ok) toast('Calibration saved ✓');
});

// Load calib from state
function loadCalibFromState() {
  if (lastState.panOff !== undefined)  { offPan.value  = lastState.panOff;  document.getElementById('off-pan-val').textContent  = lastState.panOff; }
  if (lastState.tiltOff !== undefined) { offTilt.value = lastState.tiltOff; document.getElementById('off-tilt-val').textContent = lastState.tiltOff; }
}

// =================== INIT ===================
// Poll state mỗi 2 giây
fetchState();
setInterval(fetchState, 2000);

// Merge state updates với UI
setInterval(() => {
  syncSlidersFromState();
  loadCalibFromState();
}, 2000);
