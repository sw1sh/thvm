/* Baked into every re-hosted documentation page: click-to-copy on input cells.
   The cloud's static render drops the live notebook's copy handler, so wire our
   own. Input cells render their text as "In[n]:=<code>" with line breaks encoded
   as zero-width spaces; strip the label and restore newlines to get clean,
   pasteable Wolfram Language input. */
(function () {
  var ZW = '[\\u200b\\u200c\\u200d\\ufeff]';
  var LABEL = new RegExp('^[\\s\\u200b\\u200c\\u200d\\ufeff]*In\\[[0-9]*\\]:=\\s*');
  function clean(t) {
    return (t || '')
      .replace(LABEL, '')                           // drop the In[n]:= label
      .replace(new RegExp(ZW + '+', 'g'), '\n')     // zero-width breaks -> newlines
      .replace(/ /g, ' ')                      // nbsp -> space
      .replace(/\n{3,}/g, '\n\n')
      .trim();
  }
  function isInput(el) {
    return (el.textContent || '').replace(/\s/g, '').indexOf('In[') === 0;
  }
  function flash(el) {
    var f = el.querySelector('.thvm-copied');
    if (!f) { f = document.createElement('span'); f.className = 'thvm-copied'; el.appendChild(f); }
    f.textContent = 'Copied';
    f.classList.add('on');
    setTimeout(function () { f.classList.remove('on'); }, 1000);
  }
  function copy(text, el) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(function () { flash(el); }, function () { fallback(text, el); });
    } else { fallback(text, el); }
  }
  function fallback(text, el) {
    var ta = document.createElement('textarea');
    ta.value = text; ta.style.position = 'fixed'; ta.style.opacity = '0';
    document.body.appendChild(ta); ta.focus(); ta.select();
    try { document.execCommand('copy'); flash(el); } catch (e) {}
    document.body.removeChild(ta);
  }
  function wire() {
    var cells = document.querySelectorAll('.cell-wrapper');
    for (var i = 0; i < cells.length; i++) {
      (function (c) {
        if (c.__thvmcopy || !isInput(c)) return;
        c.__thvmcopy = 1;
        c.classList.add('thvm-input');
        c.title = 'Click to copy input';
        c.addEventListener('click', function (e) {
          if (e.target.closest && e.target.closest('a[href]')) return; // keep real links clickable
          copy(clean(c.textContent), c);
        });
      })(cells[i]);
    }
  }
  if (document.readyState !== 'loading') wire(); else document.addEventListener('DOMContentLoaded', wire);
  var n = 0, t = setInterval(function () { wire(); if (++n > 8) clearInterval(t); }, 500);
})();
