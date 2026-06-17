(function () {
  var frame = document.getElementById('frame'),
      loading = document.getElementById('loading'),
      side = document.getElementById('side'),
      sideRight = document.getElementById('side-right');
  // BASE  = the deployed resource (used for the header "Resource page" link)
  // BASE2 = our re-hosted, chrome-free documentation pages (the home view too)
  var BASE = '`BASE`', BASE2 = '`BASE2`', HOME = '`HOME`', cur = null;

  // A re-hosted page may carry the resource's own header / nav sidebar. Hide
  // those (same-origin) so only the content shows; our chrome-free pages are a
  // no-op for this.
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

  // id is 'guide/X' | 'ref/Y' | 'tutorial/Z' -> our re-hosted, chrome-free page
  function pageURL(id) { return BASE2 + '/' + id + '.html'; }
  function load(id, push) {
    if (!/^(guide|ref|tutorial)\/[A-Za-z0-9]+$/.test(id)) return false;
    show();
    frame.src = pageURL(id);
    setActive(findLink(id));
    if (push && location.hash !== '#' + id) history.pushState(null, '', '#' + id);
    return true;
  }
  function goHome(push) {
    show();
    frame.src = HOME;
    setActive(findLink('guide/' + HOME.replace(/.*\/guide\//, '').replace(/\.html.*/, '')));
    if (push && location.hash !== '') history.pushState(null, '', '#');
  }

  // Route in-content links (same-origin) through the loader so navigation stays in
  // our shell. Matches both our re-hosted pages and any resource Documentation links.
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
})();
