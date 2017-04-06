if (process.env.SUPERSTRING_USE_BROWSER_VERSION) {
  module.exports = require('./browser');
} else {
  try {
    module.exports = require('./build/Release/superstring.node')
  } catch (e) {
    module.exports = require('./build/Debug/superstring.node')
  }

  const {TextBuffer} = module.exports
  const {load, save, search} = TextBuffer.prototype

  TextBuffer.prototype.load = function (filePath, encoding, progressCallback) {
    if (typeof encoding !== 'string') {
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

  TextBuffer.prototype.save = function (filePath, encoding = 'UTF8') {
    return new Promise((resolve, reject) =>
      save.call(
        this,
        filePath,
        encoding,
        (result) => {
          if (result) {
            resolve()
          } else {
            reject(new Error(`Invalid encoding name: ${encoding}`))
          }
        }
      )
    )
  }

  TextBuffer.prototype.search = function (pattern) {
    return new Promise((resolve, reject) => {
      search.call(this, pattern, (error, result) => {
        if (error) {
          reject(error)
        } else {
          resolve(result)
        }
      })
    })
  }
}
