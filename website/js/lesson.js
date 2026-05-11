// lesson.js — Fetch lesson README from GitHub, render with marked.js
window.NFS = window.NFS || {};

(function() {
  'use strict';

  var state = { phaseIdx: -1, lessonIdx: -1, phase: null, lesson: null };

  function parseParams() {
    var params = new URLSearchParams(window.location.search);
    state.phaseIdx = parseInt(params.get('p'), 10);
    state.lessonIdx = parseInt(params.get('l'), 10);
    if (isNaN(state.phaseIdx) || isNaN(state.lessonIdx)) return false;
    if (!NFS.PHASES || !NFS.PHASES[state.phaseIdx]) return false;
    state.phase = NFS.PHASES[state.phaseIdx];
    if (!state.phase.lessons[state.lessonIdx]) return false;
    state.lesson = state.phase.lessons[state.lessonIdx];
    return true;
  }

  function renderBreadcrumb() {
    var el = document.getElementById('breadcrumb');
    if (!el) return;
    var num = String(state.lessonIdx + 1).padStart(2, '0');
    el.innerHTML =
      '<a href="index.html">Home</a> &rsaquo; ' +
      '<a href="index.html#' + state.phase.id + '">' + state.phase.id + ': ' + state.phase.title + '</a> &rsaquo; ' +
      '<span>' + num + '. ' + state.lesson[0] + '</span>';
  }

  function renderSidebar() {
    var el = document.getElementById('sidebar-lessons');
    if (!el) return;
    var html = '';
    state.phase.lessons.forEach(function(l, i) {
      var num = String(i + 1).padStart(2, '0');
      var isDone = NFS.progress && NFS.progress.isDone(state.phaseIdx, i);
      var isActive = i === state.lessonIdx;
      var cls = (isActive ? ' active' : '') + (isDone ? ' done' : '');
      html += '<a href="lesson.html?p=' + state.phaseIdx + '&l=' + i + '" class="sidebar-item' + cls + '">' +
        '<span class="sidebar-dot' + (isDone ? ' done' : '') + '"></span>' +
        '<span class="sidebar-num">' + num + '</span>' +
        '<span class="sidebar-title">' + l[0] + '</span>' +
        '</a>';
    });
    el.innerHTML = html;
    document.getElementById('sidebar-phase-title').textContent = state.phase.id + ': ' + state.phase.title;
  }

  function renderPrevNext() {
    var el = document.getElementById('prev-next');
    if (!el) return;
    var html = '<div class="prev-next">';
    if (state.lessonIdx > 0) {
      var prev = state.phase.lessons[state.lessonIdx - 1];
      html += '<a href="lesson.html?p=' + state.phaseIdx + '&l=' + (state.lessonIdx - 1) + '" class="pn-link prev">' +
        '<span class="pn-arrow">&larr;</span>' +
        '<span class="pn-label">Previous</span>' +
        '<span class="pn-title">' + prev[0] + '</span></a>';
    } else {
      html += '<span></span>';
    }
    if (state.lessonIdx < state.phase.lessons.length - 1) {
      var next = state.phase.lessons[state.lessonIdx + 1];
      html += '<a href="lesson.html?p=' + state.phaseIdx + '&l=' + (state.lessonIdx + 1) + '" class="pn-link next">' +
        '<span class="pn-arrow">&rarr;</span>' +
        '<span class="pn-label">Next</span>' +
        '<span class="pn-title">' + next[0] + '</span></a>';
    }
    html += '</div>';
    el.innerHTML = html;
  }

  function renderCompletion() {
    var el = document.getElementById('completion');
    if (!el || !NFS.progress) return;
    var isDone = NFS.progress.isDone(state.phaseIdx, state.lessonIdx);
    el.innerHTML =
      '<label class="complete-toggle">' +
      '<input type="checkbox" ' + (isDone ? 'checked' : '') + ' onchange="NFS.lessonToggleComplete()">' +
      '<span>Mark as complete</span>' +
      '</label>';
  }

  NFS.lessonToggleComplete = function() {
    NFS.progress.toggle(state.phaseIdx, state.lessonIdx);
    renderCompletion();
    renderSidebar();
  };

  function showLoading() {
    var el = document.getElementById('lesson-content');
    el.innerHTML =
      '<div class="skeleton"><div class="sk-line sk-w80"></div>' +
      '<div class="sk-line sk-w60"></div><div class="sk-line sk-w90"></div>' +
      '<div class="sk-block"></div>' +
      '<div class="sk-line sk-w70"></div><div class="sk-line sk-w50"></div></div>';
  }

  function showError() {
    var el = document.getElementById('lesson-content');
    var num = String(state.lessonIdx + 1).padStart(2, '0');
    var lessonTitle = state.lesson ? state.lesson[0] : 'Unknown';
    var lang = state.lesson ? (NFS.LANG_LABELS[state.lesson[1]] || state.lesson[1]) : '';
    var type = state.lesson ? (NFS.TYPE_LABELS[state.lesson[2]] || state.lesson[2]) : '';
    var phaseTitle = state.phase ? state.phase.title : '';

    el.innerHTML =
      '<article class="lesson-article">' +
      '<h1>' + num + '. ' + lessonTitle + '</h1>' +
      '<p class="mono text-muted" style="margin-bottom:24px">' +
        state.phase.id + ' &middot; ' + phaseTitle + ' &middot; ' + type + ' &middot; ' + lang +
      '</p>' +
      '<div class="lesson-coming-soon">' +
      '<p style="font-size:1.1rem;margin-bottom:16px">This lesson is planned but not written yet.</p>' +
      '<p>The curriculum is open source — anyone can write a lesson. Each one follows a ' +
      '<a href="' + NFS.CONFIG.repoUrl + '/blob/main/docs/style-guide.md">six-section format</a>: ' +
      'Problem, Theory, Math/Spec, Code, Tests, Exercises.</p>' +
      '<div style="margin-top:24px;display:flex;gap:12px;flex-wrap:wrap">' +
      '<a href="' + NFS.CONFIG.repoUrl + '/blob/main/CONTRIBUTING.md" class="btn">Contribute this lesson</a>' +
      '<a href="catalog.html" class="btn btn-outline">Browse catalog</a>' +
      '</div>' +
      '</div>' +
      '</article>';
  }

  function renderMarkdown(md) {
    var el = document.getElementById('lesson-content');
    if (typeof marked !== 'undefined' && marked.parse) {
      marked.setOptions({
        gfm: true,
        breaks: false,
        headerIds: true
      });
      el.innerHTML = '<article class="lesson-article">' + marked.parse(md) + '</article>';
      // Style code blocks with language labels
      el.querySelectorAll('pre code').forEach(function(block) {
        var lang = '';
        block.classList.forEach(function(c) {
          if (c.startsWith('language-')) lang = c.replace('language-', '');
        });
        if (lang) {
          var label = document.createElement('span');
          label.className = 'code-lang';
          label.textContent = lang;
          block.parentElement.style.position = 'relative';
          block.parentElement.insertBefore(label, block);
        }
      });
    } else {
      el.innerHTML = '<pre style="white-space:pre-wrap">' + md.replace(/</g, '&lt;') + '</pre>';
    }
  }

  function initScrollProgress() {
    var bar = document.getElementById('scroll-progress');
    if (!bar) return;
    window.addEventListener('scroll', function() {
      var h = document.documentElement.scrollHeight - window.innerHeight;
      var pct = h > 0 ? (window.scrollY / h) * 100 : 0;
      bar.style.width = pct + '%';
    });
  }

  function initSidebarToggle() {
    var toggle = document.getElementById('sidebar-toggle');
    var sidebar = document.getElementById('sidebar');
    if (!toggle || !sidebar) return;
    toggle.addEventListener('click', function() {
      sidebar.classList.toggle('open');
      toggle.classList.toggle('open');
    });
  }

  function init() {
    if (!parseParams()) {
      var el = document.getElementById('lesson-content');
      el.innerHTML = '<div class="lesson-error"><h2>Lesson not found</h2><p>Check the URL or browse the <a href="catalog.html">catalog</a>.</p></div>';
      return;
    }
    document.title = state.lesson[0] + ' — Networking from Scratch';
    renderBreadcrumb();
    renderSidebar();
    renderPrevNext();
    renderCompletion();
    showLoading();
    initScrollProgress();
    initSidebarToggle();

    var url = NFS.lessonRawUrl(state.phase.slug, state.lesson[3]);
    fetch(url)
      .then(function(r) {
        if (!r.ok) throw new Error(r.status);
        return r.text();
      })
      .then(renderMarkdown)
      .catch(function() { showError(); });
  }

  document.addEventListener('DOMContentLoaded', init);
})();
