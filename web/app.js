const cfg = window.APP_CONFIG || {};

const GALLERY_GRID_CLASS = 'mt-6 grid grid-cols-2 gap-4 sm:grid-cols-3 md:grid-cols-4';
const GALLERY_MESSAGE_CLASS = 'mt-6 rounded-xl border border-dashed border-stone-300 py-12 text-center text-sm text-stone-400';

function apiUrl(pathname) {
  return cfg.apiUrl.replace(/\/$/, '') + pathname;
}

function showGalleryMessage(text) {
  const gallery = document.getElementById('gallery');
  gallery.className = GALLERY_MESSAGE_CLASS;
  gallery.textContent = text;
}

function setStatus(text, tone = 'neutral') {
  const statusEl = document.getElementById('send-status');
  const toneClass = {
    neutral: 'text-stone-500',
    success: 'text-ink-700',
    error: 'text-red-600',
  }[tone];
  statusEl.className = `min-h-[1.25rem] text-sm ${toneClass}`;
  statusEl.textContent = text;
}

async function loadGallery() {
  if (!cfg.apiUrl || cfg.apiUrl.includes('REPLACE-ME')) {
    showGalleryMessage('Set apiUrl in web/config.js (copy it from config.example.js) and redeploy.');
    return;
  }

  const deviceId = document.getElementById('filter-device').value.trim();
  const url = new URL(apiUrl('/paintings'));
  if (deviceId) url.searchParams.set('deviceId', deviceId);

  showGalleryMessage('Loading…');

  try {
    const res = await fetch(url);
    const data = await res.json();

    if (!data.paintings || data.paintings.length === 0) {
      showGalleryMessage('No paintings yet.');
      return;
    }

    const gallery = document.getElementById('gallery');
    gallery.className = GALLERY_GRID_CLASS;
    gallery.innerHTML = '';

    for (const p of data.paintings) {
      const card = document.createElement('div');
      card.className = 'group overflow-hidden rounded-xl border border-stone-200 bg-white shadow-sm transition hover:shadow-md';

      const img = document.createElement('img');
      img.src = p.url;
      img.alt = `${p.deviceId} / ${p.paintingId}`;
      img.loading = 'lazy';
      img.className = 'aspect-[4/5] w-full bg-stone-100 object-cover';

      const caption = document.createElement('p');
      caption.textContent = `${p.deviceId} - ${new Date(p.createdAt).toLocaleString()}`;
      caption.className = 'truncate border-t border-stone-100 px-3 py-2 text-xs text-stone-500';

      card.appendChild(img);
      card.appendChild(caption);
      gallery.appendChild(card);
    }
  } catch (err) {
    showGalleryMessage('Failed to load gallery: ' + err.message);
  }
}

document.getElementById('refresh-btn').addEventListener('click', loadGallery);

document.getElementById('send-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const deviceId = document.getElementById('device-id').value.trim();
  const webKey = document.getElementById('web-key').value;
  const file = document.getElementById('image-file').files[0];

  if (!deviceId || !webKey || !file) return;

  setStatus('Requesting upload URL…');
  try {
    const presignRes = await fetch(
      apiUrl(`/devices/${encodeURIComponent(deviceId)}/inbox/presign?contentType=${encodeURIComponent(file.type)}`),
      { method: 'POST', headers: { 'x-web-key': webKey } },
    );
    if (!presignRes.ok) {
      const errBody = await presignRes.json().catch(() => ({}));
      throw new Error(errBody.error || `presign failed (${presignRes.status})`);
    }
    const { uploadUrl } = await presignRes.json();

    setStatus('Uploading…');
    const putRes = await fetch(uploadUrl, {
      method: 'PUT',
      headers: { 'Content-Type': file.type },
      body: file,
    });
    if (!putRes.ok) throw new Error(`upload failed (${putRes.status})`);

    setStatus('Sent! The device will pick it up next time it polls its inbox.', 'success');
  } catch (err) {
    setStatus('Error: ' + err.message, 'error');
  }
});

loadGallery();
