let binding

if (process.env.SUPERSTRING_USE_BROWSER_VERSION) {
  binding = require('./browser');

  const {TextBuffer, Patch} = binding
  const {findSync, findAllSync, findWordsWithSubsequenceInRange} = TextBuffer.prototype
  const DEFAULT_RANGE = Object.freeze({start: {row: 0, column: 0}, end: {row: Infinity, column: Infinity}})

  TextBuffer.prototype.findInRangeSync = function (pattern, range) {
    if (pattern.source) pattern = pattern.source
    const result = findSync.call(this, pattern, range)
    if (typeof result === 'string') {
      throw new Error(result);
    } else {
      return result
    }
  }

  TextBuffer.prototype.findSync = function (pattern, range) {
    return this.findInRangeSync(pattern, DEFAULT_RANGE)
  }

  TextBuffer.prototype.findAllInRangeSync = function (pattern, range) {
    if (pattern.source) pattern = pattern.source
    const result = findAllSync.call(this, pattern, range)
    if (typeof result === 'string') {
      throw new Error(result);
    } else {
      return result
    }
  }

  TextBuffer.prototype.findAllSync = function (pattern, range) {
    return this.findAllInRangeSync(pattern, DEFAULT_RANGE)
  }

  TextBuffer.prototype.find = function (pattern) {
    return new Promise(resolve => resolve(this.findSync(pattern)))
  }

  TextBuffer.prototype.findInRange = function (pattern, range) {
    return new Promise(resolve => resolve(this.findInRangeSync(pattern, range)))
  }

  TextBuffer.prototype.findAll = function (pattern) {
    return new Promise(resolve => resolve(this.findAllSync(pattern)))
  }

  TextBuffer.prototype.findAllInRange = function (pattern, range) {
    return new Promise(resolve => resolve(this.findAllInRangeSync(pattern, range)))
  }

  TextBuffer.prototype.findWordsWithSubsequence = function (query, extraWordCharacters, maxCount) {
    const range = {start: {row: 0, column: 0}, end: this.getExtent()}
    return Promise.resolve(
      findWordsWithSubsequenceInRange.call(this, query, extraWordCharacters, range).slice(0, maxCount)
    )
  }

  TextBuffer.prototype.findWordsWithSubsequenceInRange = function (query, extraWordCharacters, maxCount, range) {
    return Promise.resolve(
      findWordsWithSubsequenceInRange.call(this, query, extraWordCharacters, range).slice(0, maxCount)
    )
  }

  const {compose} = Patch
  const {splice} = Patch.prototype

  Patch.compose = function (patches) {
    const result = compose.call(this, patches)
    if (!result) throw new Error('Patch does not apply')
    return result
  }

  Patch.prototype.splice = Object.assign(function () {
    if (!splice.apply(this, arguments)) {
      throw new Error('Patch does not apply')
    }
  }, splice)
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
  const {
    load, save, baseTextMatchesFile,
    find, findAll, findSync, findAllSync, findWordsWithSubsequenceInRange
  } = TextBuffer.prototype

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
        const reader = new TextReader(this, encoding)
        const buffer = Buffer.allocUnsafe(CHUNK_SIZE)
        writeToStream(null)

        stream.on('error', (error) => {
          reader.destroy()
          reject(error)
        })

        function writeToStream () {
          const bytesRead = reader.read(buffer)
          if (bytesRead > 0) {
            stream.write(buffer.slice(0, bytesRead), (error) => {
              if (!error) writeToStream()
            })
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
    return this.findInRange(pattern, null)
  }

  TextBuffer.prototype.findInRange = function (pattern, range) {
    return new Promise((resolve, reject) => {
      find.call(this, pattern, (error, result) => {
        error ? reject(error) : resolve(result.length > 0 ? interpretRange(result) : null)
      }, range)
    })
  }

  TextBuffer.prototype.findAll = function (pattern) {
    return this.findAllInRange(pattern, null)
  }

  TextBuffer.prototype.findAllInRange = function (pattern, range) {
    return new Promise((resolve, reject) => {
      findAll.call(this, pattern, (error, result) => {
        error ? reject(error) : resolve(interpretRangeArray(result))
      }, range)
    })
  }

  TextBuffer.prototype.findSync = function (pattern) {
    return this.findInRangeSync(pattern, null)
  }

  TextBuffer.prototype.findInRangeSync = function (pattern, range) {
    const result = findSync.call(this, pattern, range)
    return result.length > 0 ? interpretRange(result) : null
  }

  TextBuffer.prototype.findAllSync = function (pattern) {
    return interpretRangeArray(findAllSync.call(this, pattern, null))
  }

  TextBuffer.prototype.findAllInRangeSync = function (pattern, range) {
    return interpretRangeArray(findAllSync.call(this, pattern, range))
  }

  TextBuffer.prototype.findWordsWithSubsequence = function (query, extraWordCharacters, maxCount) {
    return this.findWordsWithSubsequenceInRange(query, extraWordCharacters, maxCount, {
      start: {row: 0, column: 0},
      end: this.getExtent()
    })
  }

  TextBuffer.prototype.findWordsWithSubsequenceInRange = function (query, extraWordCharacters, maxCount, range) {
    return new Promise(resolve =>
      findWordsWithSubsequenceInRange.call(this, query, extraWordCharacters, maxCount, range, (matches, positions) => {
        if (!matches) {
          resolve(null)
          return
        }

        let positionArrayIndex = 0
        for (let i = 0, n = matches.length; i < n; i++) {
          let positionCount = positions[positionArrayIndex++]
          matches[i].positions = interpretPointArray(positions, positionArrayIndex, positionCount)
          positionArrayIndex += 2 * positionCount
        }
        resolve(matches)
      })
    )
  }

  TextBuffer.prototype.baseTextMatchesFile = function (source, encoding = 'UTF8') {
    return new Promise((resolve, reject) => {
      const callback = (error, result) => {
        if (error) {
          reject(error)
        } else {
          resolve(result)
        }
      }

      if (typeof source === 'string') {
        baseTextMatchesFile.call(this, callback, source, encoding)
      } else {
        const stream = source
        const writer = new TextWriter(encoding)
        stream.on('data', (data) => writer.write(data))
        stream.on('error', reject)
        stream.on('end', () => {
          writer.end()
          baseTextMatchesFile.call(this, callback, writer)
        })
      }
    })
  }

  function interpretPointArray (rawData, startIndex, pointCount) {
    const points = []
    for (let i = 0; i < pointCount; i++) {
      points.push({row: rawData[startIndex++], column: rawData[startIndex++]})
    }
    return points
  }

  function interpretRangeArray (rawData) {
    const rangeCount = rawData.length / 4
    const ranges = new Array(rangeCount)
    let rawIndex = 0
    for (let rangeIndex = 0; rangeIndex < rangeCount; rangeIndex++) {
      ranges[rangeIndex] = interpretRange(rawData, rawIndex)
      rawIndex += 4
    }
    return ranges
  }

  function interpretRange (rawData, index = 0) {
    return {
      start: {
        row: rawData[index],
        column: rawData[index + 1]
      },
      end: {
        row: rawData[index + 2],
        column: rawData[index + 3]
      }
    }
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
}
