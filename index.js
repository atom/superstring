if (process.env.SUPERSTRING_USE_BROWSER_VERSION) {
  module.exports = require('./browser');

  const {TextBuffer} = module.exports
  const {search, searchSync} = TextBuffer.prototype

  TextBuffer.prototype.searchSync = function (pattern) {
    const result = searchSync.call(this, pattern)
    if (typeof result === 'string') {
      throw new Error(result);
    } else {
      return result
    }
  }

  TextBuffer.prototype.search = function (pattern) {
    return new Promise(resolve => resolve(this.searchSync(pattern)))
  }

} else {
  try {
    module.exports = require('./build/Release/superstring.node')
  } catch (e1) {
    try {
      module.exports = require('./build/Debug/superstring.node')
    } catch (e2) {
      throw e1
    }
  }

  const {TextBuffer} = module.exports
  const {load, reload, save, search} = TextBuffer.prototype

  for (const methodName of ['load', 'reload']) {
    const nativeMethod = TextBuffer.prototype[methodName]
    TextBuffer.prototype[methodName] = function (filePath, encoding, progressCallback) {
      if (typeof encoding !== 'string') {
        progressCallback = encoding
        encoding = 'UTF8'
      }

      return new Promise((resolve, reject) =>
        nativeMethod.call(this, filePath, encoding, (error, result) => {
          error ?
            reject(error) :
            resolve(result)
        }, progressCallback)
      )
    }
  }

  TextBuffer.prototype.save = function (filePath, encoding = 'UTF8') {
    return new Promise((resolve, reject) =>
      save.call(this, filePath, encoding, (result) => {
        result ?
          resolve() :
          reject(new Error(`Invalid encoding name: ${encoding}`))
      })
    )
  }

  TextBuffer.prototype.search = function (pattern) {
    return new Promise((resolve, reject) => {
      search.call(this, pattern, (error, result) => {
        error ?
          reject(error) :
          resolve(result)
      })
    })
  }
}
