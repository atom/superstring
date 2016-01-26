import Random from 'random-seed'
import WORDS from './words'
import * as pointHelpers from '../../src/point-helpers'
import * as textHelpers from '../../src/text-helpers'

export default class TestDocument {
  constructor (randomSeed, text) {
    this.random = new Random(randomSeed)
    this.lines = this.buildRandomLines(10)
  }

  clone () {
    let clone = Object.create(Object.getPrototypeOf(this))
    clone.random = this.random
    clone.lines = this.lines.slice()
    return clone
  }

  getLines () {
    return this.lines.slice()
  }

  getText () {
    return this.lines.join('\n')
  }

  getTextInRange (start, end) {
    let endRow = Math.min(end.row, this.lines.length - 1)
    if (start.row === endRow) {
      return this.lines[start.row].substring(start.column, end.column)
    } else {
      let text = this.lines[start.row].substring(start.column) + '\n'
      for (let row = start.row + 1; row < endRow; row++) {
        text += this.lines[row] + '\n'
      }
      text += this.lines[endRow].substring(0, end.column)
      return text
    }
  }

  performRandomSplice () {
    let range = this.buildRandomRange()
    let start = range.start
    let oldExtent = pointHelpers.traversalDistance(range.end, range.start)
    let newText = this.buildRandomLines(2, true).join('\n')
    let newExtent = textHelpers.getExtent(newText)
    this.splice(start, oldExtent, newText)
    return {start, oldExtent, newExtent, newText}
  }

  splice (start, oldExtent, newText) {
    let end = pointHelpers.traverse(start, oldExtent)
    let replacementLines = newText.split('\n')

    replacementLines[0] =
      this.lines[start.row].substring(0, start.column) + replacementLines[0]
    replacementLines[replacementLines.length - 1] =
      replacementLines[replacementLines.length - 1] + this.lines[end.row].substring(end.column)

    this.lines.splice(start.row, oldExtent.row + 1, ...replacementLines)
  }

  characterAtPosition ({row, column}) {
    return this.lines[row][column]
  }

  buildRandomLines (max, upperCase) {
    let lineCount = this.random.intBetween(1, max - 1)
    let lines = []
    for (let i = 0; i < lineCount; i++) {
      lines.push(this.buildRandomLine(upperCase))
    }
    return lines
  }

  buildRandomLine (upperCase) {
    let wordCount = this.random(5)
    let words = []
    for (let i = 0; i < wordCount; i++) {
      words.push(this.buildRandomWord(upperCase))
    }
    return words.join(' ')
  }

  buildRandomWord (upperCase) {
    let word = WORDS[this.random(WORDS.length)]
    if (upperCase) word = word.toUpperCase()
    return word
  }

  buildRandomRange () {
    let a = this.buildRandomPoint()
    let b = a
    while (this.random(10) < 5) {
      b = this.clipPosition(
        pointHelpers.traverse(b, {
          row: this.random.intBetween(-10, 10),
          column: this.random.intBetween(0, 10)
        })
      )
    }

    if (pointHelpers.compare(a, b) <= 0) {
      return {start: a, end: b}
    } else {
      return {start: b, end: a}
    }
  }

  buildRandomPoint () {
    let row = this.random(this.lines.length)
    let column = this.random(this.lines[row].length)
    return {row, column}
  }

  clipPosition ({row, column}) {
    row = Math.min(Math.max(row, 0), this.lines.length - 1)
    column = Math.min(Math.max(column, 0), this.lines[row].length)
    return {row, column}
  }
}
