let binding

if (process.env.SUPERSTRING_USE_BROWSER_VERSION) {
  binding = require('./browser');

  const {TextBuffer} = binding
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
    binding = require('./build/Release/superstring.node')
  } catch (e1) {
    try {
      binding = require('./build/Debug/superstring.node')
    } catch (e2) {
      throw e1
    }
  }

  const {TextBuffer, TextBuilder} = binding
  const {load, reload, save, search} = TextBuffer.prototype

  for (const methodName of ['load', 'reload']) {
    const method = TextBuffer.prototype[methodName]
    TextBuffer.prototype[methodName] = function (source, encoding, progressCallback) {
      if (typeof encoding !== 'string') {
        progressCallback = encoding
        encoding = 'UTF8'
      }

      return new Promise((resolve, reject) => {
        const completionCallback = (error, result) => {
          error ? reject(error) : resolve(result)
        }

        if (typeof source === 'string') {
          const filePath = source
          method.call(this, completionCallback, progressCallback, filePath, encoding)
        } else {
          const stream = source
          const textBuilder = new TextBuilder(encoding)
          stream.on('data', (data) => textBuilder.write(data))
          stream.on('error', reject)
          stream.on('end', () => {
            textBuilder.end()
            method.call(this, completionCallback, progressCallback, textBuilder)
          })
        }
      })
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

module.exports = {
  TextBuffer: binding.TextBuffer,
  Patch: binding.Patch,
  MarkerIndex: binding.MarkerIndex,
}