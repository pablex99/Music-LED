var colorPicker;

function ensureIroLoaded(timeoutMs) {
  return new Promise((resolve, reject) => {
    var waited = 0;
    var iv = setInterval(() => {
      if (typeof iro !== 'undefined') {
        clearInterval(iv);
        resolve(true);
      }
      waited += 50;
      if (waited >= timeoutMs) {
        clearInterval(iv);
        reject(new Error('iro.js not loaded'));
      }
    }, 50);
  });
}

document.addEventListener("DOMContentLoaded", function() {
  // Do not initialize color picker or select a mode by default.
  // The UI will show controls only after the user selects a mode.
  highlightModeButton();
  // initialize slider displays to match current input values
  var beat = document.getElementById('beatSlider');
  if (beat) document.getElementById('beatValDisplay').innerText = beat.value;
  var rs = document.getElementById('rainSpeed');
  if (rs) document.getElementById('rainSpeedVal').innerText = rs.value;
  var rb = document.getElementById('rainBrightness');
  if (rb) document.getElementById('rainBrightVal').innerText = rb.value;
  // ensure sections start hidden (CSS does this, but be explicit in JS if needed)
  var manual = document.getElementById('manualSection');
  var beatSec = document.getElementById('beatSection');
  var rainbow = document.getElementById('rainbowSection');
  if (manual) manual.style.display = 'none';
  if (beatSec) beatSec.style.display = 'none';
  if (rainbow) rainbow.style.display = 'none';
});

function initColorPickerSequence() {
  var msgEl = document.getElementById('colorMsg');
  if (msgEl) msgEl.textContent = 'Inicializando rueda de color...';

  initColorPickerIfNeeded().then(() => {
    if (msgEl) msgEl.textContent = '';
  }).catch(err => {
    console.warn('No se pudo inicializar iro.js desde /iro.min.js:', err);
    // Try a CDN fallback
    if (msgEl) msgEl.textContent = 'Cargando rueda desde CDN...';
    return loadIroScript('https://cdn.jsdelivr.net/npm/@jaames/iro@5', 5000)
      .then(initColorPickerIfNeeded)
      .then(() => { if (msgEl) msgEl.textContent = ''; })
      .catch(err2 => {
        console.error('Fallo al cargar iro.js desde CDN:', err2);
        if (msgEl) msgEl.textContent = 'Rueda de color no disponible';
      });
  });
}

function initColorPickerIfNeeded() {
  return new Promise((resolve, reject) => {
    if (typeof iro === 'undefined') {
      // try to load local iro script
      loadIroScript('/iro.min.js', 3000).then(() => {
        try { initColorPicker(); resolve(true); } catch (e) { reject(e); }
      }).catch(err => {
        reject(err);
      });
    } else {
      try { initColorPicker(); resolve(true); } catch (e) { reject(e); }
    }
  });
}

function initColorPicker() {
  if (typeof iro === 'undefined') throw new Error('iro not present');
  if (colorPicker) return; // already initialized
  colorPicker = new iro.ColorPicker("#colorWheel", {
    width: 200,
    wheelLightness: false
  });
}

function loadIroScript(src, timeoutMs){
  return new Promise((resolve, reject) => {
    // if already present, resolve
    if (typeof iro !== 'undefined') return resolve(true);
    var s = document.createElement('script');
    var done = false;
    var timer = setTimeout(() => {
      if (!done) {
        done = true;
        s.onerror = s.onload = null;
        reject(new Error('timeout loading ' + src));
      }
    }, timeoutMs || 3000);
    s.src = src;
    s.onload = function(){ if (done) return; done = true; clearTimeout(timer); resolve(true); };
    s.onerror = function(e){ if (done) return; done = true; clearTimeout(timer); reject(e || new Error('error loading ' + src)); };
    document.head.appendChild(s);
  });
}

// Esta función es la que realmente envía el color a la ESP32
function applySelectedColor() {
  var msgEl = document.getElementById('colorMsg');
  var btn = document.getElementById('applyColorBtn');
  if (btn) btn.disabled = true;
  if (!colorPicker) {
    console.warn('Color picker no inicializado');
    if (msgEl) msgEl.textContent = 'Rueda no disponible';
    if (btn) btn.disabled = false;
    return;
  }
  let rgb = colorPicker.color.rgb;
  if (msgEl) msgEl.textContent = 'Enviando color...';
  fetch(`/setColor?R=${rgb.r}&G=${rgb.g}&B=${rgb.b}`)
    .then(resp => resp.text())
    .then(txt => {
      if (msgEl) msgEl.textContent = 'Color aplicado';
      console.log("Respuesta ESP32:", txt);
      setTimeout(()=>{ if (msgEl) msgEl.textContent = ''; }, 1200);
    })
    .catch(err => {
      if (msgEl) msgEl.textContent = 'Error al enviar color';
      console.error("Error enviando color:", err);
    }).finally(()=>{ if (btn) btn.disabled = false; });
}

// Cambiar modo de operación y actualizar UI
function setMode(mode){
  var msgEl = document.getElementById('colorMsg');
  fetch(`/setMode?m=${mode}`)
    .then(() => {
      highlightModeButton(mode);
      // Mostrar/ocultar elementos según modo
      var manual = document.getElementById('manualSection');
      var beat = document.getElementById('beatSection');
      var rainbow = document.getElementById('rainbowSection');
      if (mode === 'manual') {
        if (manual) manual.style.display = 'flex';
        if (beat) beat.style.display = 'none';
        if (rainbow) rainbow.style.display = 'none';
        // initialize color picker only when user requests manual mode
        if (typeof colorPicker === 'undefined' || !colorPicker) {
          initColorPickerSequence();
        }
            // ensure button applies manual color
            var btn = document.getElementById('applyColorBtn');
            if (btn) { btn.textContent = 'Aplicar color'; btn.onclick = applySelectedColor; }
      } else if (mode === 'music') {
            if (manual) manual.style.display = 'flex';
            if (beat) beat.style.display = 'block';
            if (rainbow) rainbow.style.display = 'none';
            // initialize color picker when opening music mode so user can pick the blink color
            if (typeof colorPicker === 'undefined' || !colorPicker) {
              initColorPickerSequence();
            }
            // make apply button set music color instead of manual color
            var btn2 = document.getElementById('applyColorBtn');
            if (btn2) { btn2.textContent = 'Establecer color (modo Música)'; btn2.onclick = applyMusicColor; }
      } else if (mode === 'rainbow') {
        if (manual) manual.style.display = 'none';
        if (beat) beat.style.display = 'none';
        if (rainbow) rainbow.style.display = 'flex';
        // ensure displayed labels reflect current slider values
        var rs = document.getElementById('rainSpeed');
        var rb = document.getElementById('rainBrightness');
        if (rs) document.getElementById('rainSpeedVal').innerText = rs.value;
        if (rb) document.getElementById('rainBrightVal').innerText = rb.value;
      }
      if (msgEl) { msgEl.textContent = 'Modo ' + mode + ' activado'; setTimeout(()=>{ msgEl.textContent = ''; }, 900); }
    })
    .catch(err => {
      console.error('Error setMode:', err);
      if (msgEl) { msgEl.textContent = 'Error cambiando modo'; setTimeout(()=>{ msgEl.textContent = ''; }, 1200); }
    });
}

function highlightModeButton(mode) {
  var btns = document.getElementsByClassName('modeBtn');
  for (var i=0;i<btns.length;i++) btns[i].classList.remove('active');
  if (mode === 'manual') document.getElementById('modeManual').classList.add('active');
  if (mode === 'music') document.getElementById('modeMusic').classList.add('active');
  if (mode === 'rainbow') document.getElementById('modeRainbow').classList.add('active');
}

// Actualizar umbral de detección de beat
function updateBeatDisplay(v) {
  document.getElementById('beatValDisplay').innerText = v;

  if (window._beatTimeout) clearTimeout(window._beatTimeout);

  window._beatTimeout = setTimeout(function(){
    fetch('/setBeatThreshold?value=' + encodeURIComponent(v))
      .catch(err => console.log('Error enviando beat threshold', err));
  }, 120);
}

// Actualizar velocidad del arcoíris (ms por paso) con debounce
function updateRainbowSpeed(v) {
  var el = document.getElementById('rainSpeedVal');
  if (el) el.innerText = v;

  if (window._rainSpeedTimeout) clearTimeout(window._rainSpeedTimeout);

  window._rainSpeedTimeout = setTimeout(function(){
    fetch('/setRainbowSpeed?value=' + encodeURIComponent(v))
      .catch(err => console.log('Error enviando rainbow speed', err));
  }, 120);
}

// Actualizar brillo del arcoíris (0-100%) con debounce
function updateRainbowBrightness(v) {
  var el = document.getElementById('rainBrightVal');
  if (el) el.innerText = v;

  if (window._rainBrightTimeout) clearTimeout(window._rainBrightTimeout);

  window._rainBrightTimeout = setTimeout(function(){
    fetch('/setRainbowBrightness?value=' + encodeURIComponent(v))
      .catch(err => console.log('Error enviando rainbow brightness', err));
  }, 120);
}

// Enviar color seleccionado para que el modo Música lo use al parpadear
function applyMusicColor() {
  var msgEl = document.getElementById('colorMsg');
  var btn = document.getElementById('applyColorBtn');
  if (btn) btn.disabled = true;
  if (!colorPicker) {
    if (msgEl) msgEl.textContent = 'Rueda no disponible';
    if (btn) btn.disabled = false;
    return;
  }
  let rgb = colorPicker.color.rgb;
  if (msgEl) msgEl.textContent = 'Enviando color para modo Música...';
  fetch(`/setMusicColor?R=${rgb.r}&G=${rgb.g}&B=${rgb.b}`)
    .then(resp => resp.text())
    .then(txt => {
      if (msgEl) msgEl.textContent = 'Color de música establecido';
      console.log('Respuesta ESP32:', txt);
      setTimeout(()=>{ if (msgEl) msgEl.textContent = ''; }, 1200);
    })
    .catch(err => {
      if (msgEl) msgEl.textContent = 'Error al enviar color';
      console.error('Error enviando color de música:', err);
    }).finally(()=>{ if (btn) btn.disabled = false; });
}