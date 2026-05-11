/* ================================================================
 *  progress.js — localStorage progress tracking for NFS
 * ================================================================ */

window.NFS = window.NFS || {};

NFS.progress = {
  _key: 'nfs_done',

  _get: function() {
    return JSON.parse(localStorage.getItem(this._key) || '{}');
  },

  _set: function(d) {
    localStorage.setItem(this._key, JSON.stringify(d));
  },

  isDone: function(pIdx, lIdx) {
    return !!this._get()[pIdx + '-' + lIdx];
  },

  toggle: function(pIdx, lIdx) {
    var d = this._get(), k = pIdx + '-' + lIdx;
    if (d[k]) delete d[k]; else d[k] = true;
    this._set(d);
  },

  reset: function() {
    localStorage.removeItem(this._key);
  },

  stats: function() {
    var d = this._get(), total = 0, done = 0, started = 0;
    NFS.PHASES.forEach(function(p, pi) {
      var pd = 0;
      p.lessons.forEach(function(_, li) { total++; if (d[pi + '-' + li]) { pd++; done++; } });
      if (pd > 0) started++;
    });
    return { totalLessons: total, totalDone: done, phasesStarted: started, totalPhases: NFS.PHASES.length };
  },

  phaseDone: function(pIdx) {
    var d = this._get(), pd = 0;
    NFS.PHASES[pIdx].lessons.forEach(function(_, li) { if (d[pIdx + '-' + li]) pd++; });
    return pd;
  }
};
