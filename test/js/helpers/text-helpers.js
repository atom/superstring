const NEWLINE_REG_EXP = /\n/g

exports.getExtent = function getExtent (text) {
  let lastLineStartIndex = 0
  let row = 0
  NEWLINE_REG_EXP.lastIndex = 0
  while (NEWLINE_REG_EXP.exec(text)) {
    row++
    lastLineStartIndex = NEWLINE_REG_EXP.lastIndex
  }
  let column = text.length - lastLineStartIndex
  return {row, column}
}

exports.getPrefix = function getPrefix (text, prefixExtent) {
  return text.substring(0, characterIndexForPoint(text, prefixExtent))
}

exports.getSuffix = function getSuffix (text, prefixExtent) {
  return text.substring(characterIndexForPoint(text, prefixExtent))
}

exports.characterIndexForPoint = function characterIndexForPoint(text, point) {
  let {row, column} = point
  NEWLINE_REG_EXP.lastIndex = 0
  while (row-- > 0) {
    let matches = NEWLINE_REG_EXP.exec(text)
    if (matches == null) {
      return text.length
    }
  }
  return NEWLINE_REG_EXP.lastIndex + column
}
