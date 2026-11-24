/* Offline copy of the CrossPlay (PaperMono) version of Severance. The game is one file with
   everything inside it, so once index.html is cached it works offline; the page is fetched fresh when online. */
var CACHE = 'cardgames-severance-v12';
var FILES = ['./', 'index.html', 'manifest.webmanifest', 'icon-severance-180.png', 'icon-severance-512.png'];
self.addEventListener('install', function (e) {
  e.waitUntil(caches.open(CACHE).then(function (c) { return c.addAll(FILES); }).then(function () { return self.skipWaiting(); }));
});
self.addEventListener('activate', function (e) {
  e.waitUntil(caches.keys().then(function (keys) {
    return Promise.all(keys.filter(function (k) { return k.indexOf('cardgames-severance-') === 0 && k !== CACHE; })
      .map(function (k) { return caches.delete(k); }));
  }).then(function () { return self.clients.claim(); }));
});
self.addEventListener('fetch', function (e) {
  if (e.request.method !== 'GET') return;
  if (e.request.mode === 'navigate') {
    // The page itself: the network first, so a new version shows on the first open,
    // and the cached copy when offline.
    e.respondWith(fetch(e.request).then(function (r) {
      if (r.ok) { var copy = r.clone(); caches.open(CACHE).then(function (c) { c.put('index.html', copy); }); }
      return r;
    }).catch(function () { return caches.match('index.html'); }));
    return;
  }
  e.respondWith(caches.match(e.request, { ignoreSearch: true }).then(function (hit) {
    return hit || fetch(e.request).catch(function () { return Response.error(); });
  }));
});
