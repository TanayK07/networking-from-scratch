// header.js — Dark mode toggle, GitHub star counter, nav enhancement
window.NFS = window.NFS || {};

(function() {
  'use strict';

  // --- Dark Mode ---
  NFS.darkMode = {
    _key: 'nfs_theme',
    init: function() {
      var toggle = document.querySelector('.theme-toggle');
      if (!toggle) return;
      toggle.addEventListener('click', function() {
        var isDark = document.documentElement.classList.toggle('dark');
        localStorage.setItem(NFS.darkMode._key, isDark ? 'dark' : 'light');
        toggle.textContent = isDark ? '☀' : '☾';
        toggle.title = isDark ? 'Switch to light mode' : 'Switch to dark mode';
        toggle.setAttribute('aria-label', toggle.title);
      });
      var isDark = document.documentElement.classList.contains('dark');
      toggle.textContent = isDark ? '☀' : '☾';
      toggle.title = isDark ? 'Switch to light mode' : 'Switch to dark mode';
    }
  };

  // --- GitHub Stars ---
  NFS.stars = {
    _cacheKey: 'nfs_stars_cache',
    _ttl: 600000, // 10 minutes
    init: function() {
      var el = document.querySelector('.gh-stars');
      if (!el || !NFS.CONFIG) return;
      var cached = this._fromCache();
      if (cached !== null) {
        this._render(el, cached);
        return;
      }
      var url = 'https://api.github.com/repos/' + NFS.CONFIG.repoOwner + '/' + NFS.CONFIG.repoName;
      fetch(url)
        .then(function(r) { return r.json(); })
        .then(function(data) {
          if (data.stargazers_count !== undefined) {
            NFS.stars._cache(data.stargazers_count);
            NFS.stars._render(el, data.stargazers_count);
          }
        })
        .catch(function() { /* silently degrade */ });
    },
    _fromCache: function() {
      try {
        var raw = localStorage.getItem(this._cacheKey);
        if (!raw) return null;
        var obj = JSON.parse(raw);
        if (Date.now() - obj.t > this._ttl) return null;
        return obj.v;
      } catch(e) { return null; }
    },
    _cache: function(count) {
      localStorage.setItem(this._cacheKey, JSON.stringify({ v: count, t: Date.now() }));
    },
    _render: function(el, count) {
      el.textContent = count >= 1000 ? (count / 1000).toFixed(1) + 'k' : count;
      el.style.display = 'inline';
    }
  };

  // --- Init on DOMContentLoaded ---
  document.addEventListener('DOMContentLoaded', function() {
    NFS.darkMode.init();
    NFS.stars.init();
  });
})();
