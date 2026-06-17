(function () {
  var frame = document.getElementById('frame'),
      loading = document.getElementById('loading'),
      side = document.getElementById('side'),
      sideRight = document.getElementById('side-right'),
      resLink = document.getElementById('resourcelink');

  // point the "Resource page" button at the cloud twin of whatever is open
  function setTwin(id) {
    if (resLink) resLink.href = id ? (BASE + '/Documentation/' + id + '.html') : (BASE + '/');
  }
  // BASE = the deployed resource; we frame its live Documentation embed pages,
  // which carry the full notebook viewer (collapse, copy, cross-links, every section).
  // HOME = the landing page framed first.
  var BASE = '`BASE`', HOME = '`HOME`', cur = null;

  // The framed embed pages (and the home page) carry the resource's own header
  // and nav sidebar; hide those (same-origin) so only the content shows in our shell.
  var HIDE = [
    '#pg-header,#pac-nav-sidebar,#pac-nav-sidebar-frame,.page-sidebar-frame,.page-sidebar,',
    '.page-sidebar-toggle,#pageSidebar,#pageSidebarFrame{display:none!important}',
    '.shingle-content,main.shingle-content{margin-left:0!important;max-width:none!important;width:auto!important}'
  ].join('');

  function injectHide() {
    try {
      var d = frame.contentDocument;
      if (!d || !d.head || d.getElementById('thvm-hide-chrome')) return;
      var s = d.createElement('style');
      s.id = 'thvm-hide-chrome';
      s.textContent = HIDE;
      d.head.appendChild(s);
    } catch (e) {}
  }

  function openAnc(a) {
    var n = a.parentNode;
    while (n && n !== side) { if (n.tagName === 'DETAILS') n.open = true; n = n.parentNode; }
  }
  function setActive(a) {
    if (cur) cur.classList.remove('active');
    cur = a;
    if (a) { a.classList.add('active'); openAnc(a); a.scrollIntoView({ block: 'nearest' }); }
  }
  function findLink(id) {
    var e = document.querySelectorAll('#side a[data-id], #side-right a[data-id]');
    for (var i = 0; i < e.length; i++) { if (e[i].getAttribute('data-id') === id) return e[i]; }
    return null;
  }
  function show() { loading.classList.add('on'); }

  // id is 'guide/X' | 'ref/Y' | 'tutorial/Z' -> the live cloud embed page
  function pageURL(id) { return BASE + '/Documentation/' + id + '.html'; }
  function load(id, push) {
    if (!/^(guide|ref|tutorial)\/[A-Za-z0-9]+$/.test(id)) return false;
    show();
    frame.src = pageURL(id);
    setActive(findLink(id));
    setTwin(id);
    if (push && location.hash !== '#' + id) history.pushState(null, '', '#' + id);
    return true;
  }
  function goHome(push) {
    show();
    frame.src = HOME;
    setActive(null);
    setTwin(null);
    if (push && location.hash !== '') history.pushState(null, '', '#');
  }

  // Route in-content links (same-origin) through the loader so navigation stays in
  // our shell. Matches the resource's Documentation cross-links.
  function hookFrame() {
    try {
      var d = frame.contentDocument;
      if (!d) return;
      var as = d.querySelectorAll('a[href]');
      for (var i = 0; i < as.length; i++) {
        (function (a) {
          if (a.__thvm) return;
          var h = a.getAttribute('href') || '';
          var m = h.match(/\/(guide|ref|tutorial)\/([A-Za-z0-9]+)\.(?:html|nb)(?:[?#].*)?$/);
          if (m) {
            a.__thvm = 1;
            a.addEventListener('click', function (e) { e.preventDefault(); load(m[1] + '/' + m[2], true); });
          }
        })(as[i]);
      }
    } catch (e) {}
  }

  frame.addEventListener('load', function () {
    injectHide();
    hookFrame();
    loading.classList.remove('on');
    var n = 0, t = setInterval(function () { injectHide(); hookFrame(); if (++n > 10) clearInterval(t); }, 400);
  });

  function navClick(e) {
    var a = e.target.closest && e.target.closest('a[data-id]');
    if (!a) return;
    e.preventDefault();
    load(a.getAttribute('data-id'), true);
  }
  side.addEventListener('click', navClick);
  if (sideRight) sideRight.addEventListener('click', navClick);
  var brand = document.getElementById('homelink');
  if (brand) brand.addEventListener('click', function (e) { e.preventDefault(); goHome(true); });
  window.addEventListener('popstate', function () {
    var id = location.hash.slice(1);
    if (id) load(id, false); else goHome(false);
  });

  var s = location.hash.slice(1);
  if (!(s && load(s, false))) goHome(false);

  // Speed: the embed pages pull a ~535 KB notebook-renderer bundle (cacheable for
  // a year). Prefetch it up front via the embedding resolve so the first page
  // click doesn't pay that cost.
  (function preloadEmbedder() {
    try {
      var i = BASE.indexOf('/obj/');
      if (i < 0) return;
      var origin = BASE.slice(0, i), path = BASE.slice(i + 5) + '/Documentation/guide/THVMLink.nb';
      var x = new XMLHttpRequest();
      x.open('GET', origin + '/notebooks/embedding?path=' + encodeURIComponent(path), true);
      x.onload = function () {
        if (x.status !== 200) return;
        try {
          var d = JSON.parse(x.responseText);
          [d.mainScript].concat(d.otherScripts || []).forEach(function (sc) {
            var l = document.createElement('link');
            l.rel = 'prefetch'; l.href = origin + sc;
            document.head.appendChild(l);
          });
        } catch (e) {}
      };
      x.send();
    } catch (e) {}
  })();
})();
