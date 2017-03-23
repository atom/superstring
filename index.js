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
module.exports.TextBuffer.prototype.load =
function (filePath, encoding, progressCallback) {
  if (typeof encoding === 'function') {
    progressCallback = encoding
    encoding = 'UTF8'
  }

  return new Promise((resolve, reject) =>
    load.call(
      this,
      filePath,
      encoding,
      (result) => {
        if (result) {
          resolve()
        } else {
          reject(new Error(`Invalid encoding name: ${encoding}`))
        }
      },
      progressCallback
    )
  )
}