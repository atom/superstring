let binding

if (process.env.SUPERSTRING_USE_BROWSER_VERSION) {
  binding = require('./browser');

  const {TextBuffer} = binding
  const {find, findSync, findAllSync} = TextBuffer.prototype

  TextBuffer.prototype.findSync = function (pattern) {
    if (pattern.source) pattern = pattern.source
    const result = findSync.call(this, pattern)
    if (typeof result === 'string') {
      throw new Error(result);
    } else {
      return result
    }
  }

  TextBuffer.prototype.findAllSync = function (pattern) {
    if (pattern.source) pattern = pattern.source
    const result = findAllSync.call(this, pattern)
    if (typeof result === 'string') {
      throw new Error(result);
    } else {
      return result
    }
  }

  TextBuffer.prototype.find = function (pattern) {
    return new Promise(resolve => resolve(this.findSync(pattern)))
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

  const {TextBuffer, TextWriter, TextReader} = binding
  const {load, save, find, findAllSync} = TextBuffer.prototype

  TextBuffer.prototype.load = function (source, options, progressCallback) {
    if (typeof options !== 'object') {
      progressCallback = options
      options = {}
    }

    const computePatch = options.patch === false ? false : true
    const discardChanges = options.force === true ? true : false
    const encoding = normalizeEncoding(options.encoding || 'UTF-8')

    return new Promise((resolve, reject) => {
      const completionCallback = (error, result) => {
        error ? reject(error) : resolve(result)
      }

      if (typeof source === 'string') {
        const filePath = source
        load.call(
          this,
          completionCallback,
          progressCallback,
          discardChanges,
          computePatch,
          filePath,
          encoding
        )
      } else {
        const stream = source
        const writer = new TextWriter(encoding)
        stream.on('data', (data) => writer.write(data))
        stream.on('error', reject)
        stream.on('end', () => {
          writer.end()
          load.call(
            this,
            completionCallback,
            progressCallback,
            discardChanges,
            computePatch,
            writer
          )
        })
      }
    })
  }

  TextBuffer.prototype.save = function (destination, encoding = 'UTF8') {
    const CHUNK_SIZE = 10 * 1024

    encoding = normalizeEncoding(encoding)

    return new Promise((resolve, reject) => {
      if (typeof destination === 'string') {
        const filePath = destination
        save.call(this, filePath, encoding, (error) => {
          error ? reject(error) : resolve()
        })
      } else {
        const stream = destination
        stream.on('error', reject)

        const reader = new TextReader(this, encoding)
        const buffer = Buffer.allocUnsafe(CHUNK_SIZE)
        writeToStream()

        function writeToStream () {
          const bytesRead = reader.read(buffer)
          if (bytesRead > 0) {
            stream.write(buffer.slice(0, bytesRead), writeToStream)
          } else {
            stream.end(() => {
              reader.end()
              resolve()
            })
          }
        }
      }
    })
  }

  TextBuffer.prototype.find = function (pattern) {
    return new Promise((resolve, reject) => {
      find.call(this, pattern, (error, result) => {
        error ?
          reject(error) :
          resolve(result)
      })
    })
  }

  TextBuffer.prototype.findAllSync = function (pattern) {
    const rawData = findAllSync.call(this, pattern)
    const result = new Array(rawData.length / 4)
    let rawIndex = 0
    for (let matchIndex = 0, n = result.length; matchIndex < n; matchIndex++) {
      result[matchIndex] = {
        start: {row: rawData[rawIndex++], column: rawData[rawIndex++]},
        end: {row: rawData[rawIndex++], column: rawData[rawIndex++]}
      }
    }
    return result
  }
}

function normalizeEncoding(encoding) {
  return encoding.toUpperCase()
    .replace(/[^A-Z\d]/g, '')
    .replace(/^(UTF|UCS|ISO|WINDOWS|KOI8|EUC)(\w)/, '$1-$2')
    .replace(/^(ISO-8859)(\d)/, '$1-$2')
    .replace(/^(SHIFT)(\w)/, '$1_$2')
}

module.exports = {
  TextBuffer: binding.TextBuffer,
  Patch: binding.Patch,
  MarkerIndex: binding.MarkerIndex,
  BufferOffsetIndex: binding.BufferOffsetIndex,
}