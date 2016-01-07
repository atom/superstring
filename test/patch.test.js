import Random from 'random-seed'
import Patch from '../src/patch'
import {traverse, traversalDistance, format as formatPoint, isZero as isZeroPoint} from '../src/point-helpers'
import TestDocument from './helpers/test-document'
import './helpers/add-to-html-methods'

describe('Patch', function () {
  it('correctly records basic non-overlapping splices', function () {
    let seed = 123
    let patch = new Patch(seed)
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello')
    patch.spliceWithText({row: 0, column: 10}, {row: 0, column: 5}, 'world')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, replacedExtent: {row: 0, column: 4}, replacementExtent: {row: 0, column: 5}, replacementText: 'hello'},
      {start: {row: 0, column: 10}, replacedExtent: {row: 0, column: 5}, replacementExtent: {row: 0, column: 5}, replacementText: 'world'}
    ])
  })

  it('correctly records basic overlapping splices', function () {
    let seed = 123
    let patch = new Patch(seed)
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello world')
    patch.spliceWithText({row: 0, column: 9}, {row: 0, column: 7}, 'sun')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, replacedExtent: {row: 0, column: 6}, replacementExtent: {row: 0, column: 9}, replacementText: 'hello sun'},
    ])
  })

  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 1000; i++) {
      let seed = Date.now()
      let seedMessage = `Random seed: ${seed}`
      let random = new Random(seed)
      let input = new TestDocument(seed)
      let output = input.clone()
      let combineChanges = Boolean(random(2))
      let patch = new Patch({seed, combineChanges})

      for (let j = 0; j < 10; j++) {
        if (random(10) < 2) { // 20% splice input
          let {start: inputStart, replacedExtent, replacementExtent, replacementText} = input.performRandomSplice()
          let outputStart = patch.translateInputPosition(inputStart)
          let outputReplacedExtent = traversalDistance(
            patch.translateInputPosition(traverse(inputStart, replacedExtent)),
            outputStart
          )
          output.splice(outputStart, outputReplacedExtent, replacementText)
          // document.write(`<div>after spliceInput(${formatPoint(inputStart)}, ${formatPoint(replacedExtent)}, ${formatPoint(replacementExtent)}, ${replacementText})</div>`)
          // document.write(patch.toHTML())
          // document.write('<hr>')
          let result = patch.spliceInput(inputStart, replacedExtent, replacementExtent)
          assert.deepEqual(result.start, outputStart, seedMessage)
          assert.deepEqual(result.replacedExtent, outputReplacedExtent, seedMessage)
          assert.deepEqual(result.replacementExtent, replacementExtent, seedMessage)
        } else { // 80% normal splice
          let {start, replacedExtent, replacementExtent, replacementText} = output.performRandomSplice()
          patch.spliceWithText(start, replacedExtent, replacementText)
          // document.write(`<div>after splice(${formatPoint(start)}, ${formatPoint(replacedExtent)}, ${formatPoint(replacementExtent)}, ${replacementText})</div>`)
          // document.write(patch.toHTML())
          // document.write('<hr>')
        }

        verifyPatch(patch, input.clone(), output, random, seedMessage)
      }
    }

    function verifyPatch (patch, input, output, random, seedMessage) {
      verifyInputPositionTranslation(patch, input, output, seedMessage, random)
      verifyOutputPositionTranslation(patch, input, output, seedMessage, random)

      let synthesizedOutput = ''
      patch.iterator.rewind()
      do {
        if (patch.iterator.inChange()) {
          assert(!(isZeroPoint(patch.iterator.getInputExtent()) && isZeroPoint(patch.iterator.getOutputExtent())), "Empty region found. " + seedMessage);
          synthesizedOutput += patch.iterator.getReplacementText()
        } else {
          synthesizedOutput += input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd())
        }
      } while (patch.iterator.moveToSuccessor())

      assert.equal(synthesizedOutput, output.getText(), seedMessage)

      for (let {start, replacedExtent, replacementText} of patch.getChanges()) {
        input.splice(start, replacedExtent, replacementText)
      }
      assert.equal(input.getText(), output.getText(), seedMessage)
    }

    function verifyInputPositionTranslation (patch, input, output, seedMessage) {
      let row = 0
      for (let line of input.getLines()) {
        let column = 0
        for (let character of line) {
          let inputPosition = {row, column}
          if (!patch.isChangedAtInputPosition(inputPosition)) {
            let outputPosition = patch.translateInputPosition(inputPosition)
            assert.equal(output.characterAtPosition(outputPosition), character, seedMessage)
          }
          column++
        }
        row++
      }
    }

    function verifyOutputPositionTranslation (patch, input, output, seedMessage) {
      let row = 0
      for (let line of output.getLines()) {
        let column = 0
        for (let character of line) {
          let outputPosition = {row, column}
          if (!patch.isChangedAtOutputPosition(outputPosition)) {
            let inputPosition = patch.translateOutputPosition(outputPosition)
            assert.equal(input.characterAtPosition(inputPosition), character, seedMessage)
          }
          column++
        }
        row++
      }
    }
  })
})
