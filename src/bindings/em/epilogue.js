
  Module.TextBuffer.prototype.search = function (pattern) {
    return Promise.resolve(this.searchSync(pattern))
  }

  return Module;
}));
