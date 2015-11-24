import Random from 'random-seed'
import phonetic from 'phonetic'
import * as pointHelpers from '../../src/point-helpers'

export default class TestDocument {
  constructor (randomSeed, text) {
    this.random = new Random(randomSeed)
    this.lines = this.buildRandomLines(100)
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

  performRandomSplice () {
    let range = this.buildRandomRange()
    let start = range.start
    let replacedExtent = pointHelpers.traversalDistance(range.end, range.start)
    let replacementText = this.buildRandomLines(5, true).join('\n')
    this.splice(start, replacedExtent, replacementText)
    return {start, replacedExtent, replacementText}
  }

  splice (start, replacedExtent, replacementText) {
    let end = pointHelpers.traverse(start, replacedExtent)
    let replacementLines = replacementText.split('\n')

    replacementLines[0] =
      this.lines[start.row].substring(0, start.column) + replacementLines[0]
    replacementLines[replacementLines.length - 1] =
      replacementLines[replacementLines.length - 1] + this.lines[end.row].substring(end.column)

    this.lines.splice(start.row, replacedExtent.row + 1, ...replacementLines)
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
    let word = phonetic.generate({
      seed: this.random(1000),
      syllables: this.random(4) + 1,
      capFirst: false
    })
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
