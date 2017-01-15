module.exports = class ReferenceBufferOffsetIndex {
  constructor () {
    this.lineLengths = []
    this.charactersCount = 0
    this.longestColumn = 0
  }

  getLineCount () {
    return this.lineLengths.length
  }

  getLongestColumn () {
    return this.longestColumn
  }

  getCharactersCount () {
    return this.charactersCount
  }

  splice (startRow, deletedLinesCount, newLineLengths) {
    this.lineLengths.splice(startRow, deletedLinesCount, ...newLineLengths)
    this.charactersCount = this.lineLengths.reduce((a, b) => a + b, 0)
    this.longestColumn = this.lineLengths.reduce((a, b) => Math.max(a, b), 0)
  }

  position_for_character_index (index) {
    let charIndex = 0
    let row = 0
    for (const lineLength of this.lineLengths) {
      if (charIndex + lineLength > index) {
        break
      } else if (row + 1 < this.lineLengths.length) {
        charIndex += lineLength
        row++
      }
    }
    const lineLength = this.lineLengths[row]
    return {row, column: Math.min(index - charIndex, lineLength)}
  }

  character_index_for_position (position) {
    const charIndex = this.lineLengths.slice(0, position.row).reduce((a, b) => a + b, 0)
    const lineLength = this.lineLengths[position.row]
    if (lineLength == null) {
      return Math.max(0, charIndex)
    } else {
      return charIndex + Math.min(position.column, lineLength)
    }
  }
}
