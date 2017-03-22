if (process.env.SUPERSTRING_USE_BROWSER_VERSION) {
  module.exports = require('./browser');
} else {
  try {
    module.exports = require('./build/Release/superstring.node')
  } catch (e) {
    module.exports = require('./build/Debug/superstring.node')
  }
}

const {load} = module.exports.TextBuffer.prototype
module.exports.TextBuffer.prototype.load = function (filePath, progressCallback) {
  return new Promise(resolve => {
    load.call(this, filePath, resolve, progressCallback || function() {})
  });
}