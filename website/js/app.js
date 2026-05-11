// app.js — Scroll reveal, clipboard, utilities
window.NFS = window.NFS || {};

(function() {
  'use strict';

  // --- Scroll Reveal ---
  NFS.scrollReveal = function() {
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      document.querySelectorAll('.reveal').forEach(function(el) {
        el.classList.add('visible');
      });
      return;
    }
    var observer = new IntersectionObserver(function(entries) {
      entries.forEach(function(entry) {
        if (entry.isIntersecting) {
          entry.target.classList.add('visible');
          observer.unobserve(entry.target);
        }
      });
    }, { threshold: 0.1, rootMargin: '0px 0px -40px 0px' });
    document.querySelectorAll('.reveal').forEach(function(el, i) {
      el.style.transitionDelay = (el.dataset.delay || (i * 30)) + 'ms';
      observer.observe(el);
    });
  };

  // --- Staggered Reveal (for grids/lists) ---
  NFS.staggerReveal = function(selector, baseDelay) {
    baseDelay = baseDelay || 50;
    document.querySelectorAll(selector).forEach(function(el, i) {
      el.classList.add('reveal');
      el.dataset.delay = i * baseDelay;
    });
  };

  // --- Copy to Clipboard ---
  NFS.copyToClipboard = function(text, btn) {
    navigator.clipboard.writeText(text).then(function() {
      var orig = btn.textContent;
      btn.textContent = 'Copied!';
      btn.classList.add('copied');
      setTimeout(function() {
        btn.textContent = orig;
        btn.classList.remove('copied');
      }, 1500);
    });
  };

  // --- Slugify ---
  NFS.slugify = function(num, title) {
    var s = title.toLowerCase()
      .replace(/[^a-z0-9\s-]/g, '')
      .replace(/\s+/g, '-')
      .replace(/-+/g, '-')
      .replace(/^-|-$/g, '');
    return String(num).padStart(2, '0') + '-' + s;
  };

  // --- Lesson URL builder ---
  NFS.lessonUrl = function(phaseIdx, lessonIdx) {
    return 'lesson.html?p=' + phaseIdx + '&l=' + lessonIdx;
  };

  // --- GitHub raw URL for lesson README ---
  NFS.lessonRawUrl = function(phaseSlug, lessonSlug) {
    if (!NFS.CONFIG) return '';
    return 'https://raw.githubusercontent.com/' +
      NFS.CONFIG.repoOwner + '/' + NFS.CONFIG.repoName +
      '/main/phases/' + phaseSlug + '/' + lessonSlug + '/README.md';
  };

  // --- Language display ---
  NFS.LANG_LABELS = { c: 'C', py: 'Python', bpf: 'eBPF', lua: 'Lua' };
  NFS.LANG_SHORT = { c: 'C', py: 'PY', bpf: 'BPF', lua: 'LUA' };
  NFS.TYPE_LABELS = { theory: 'Theory', implement: 'Build', capstone: 'Capstone' };

  // --- Init scroll reveal after DOM is ready ---
  document.addEventListener('DOMContentLoaded', function() {
    NFS.scrollReveal();
  });
})();
