import Random from 'random-seed'
import Patch from '../src/patch'
import {traverse, traversalDistance, format as formatPoint, isZero as isZeroPoint} from '../src/point-helpers'
import TestDocument from './helpers/test-document'
import './helpers/add-to-html-methods'

describe('Patch', function () {
  it('correctly records basic non-overlapping splices', function () {
    let patch = new Patch({seed: 123})
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello')
    patch.spliceWithText({row: 0, column: 10}, {row: 0, column: 5}, 'world')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, oldExtent: {row: 0, column: 4}, newExtent: {row: 0, column: 5}, newText: 'hello'},
      {start: {row: 0, column: 10}, oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 5}, newText: 'world'}
    ])
  })

  it('correctly records basic overlapping splices', function () {
    let patch = new Patch({seed: 123})
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello world')
    patch.spliceWithText({row: 0, column: 9}, {row: 0, column: 7}, 'sun')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, oldExtent: {row: 0, column: 6}, newExtent: {row: 0, column: 9}, newText: 'hello sun'},
    ])
  })

  it('includes regions with empty input extents when splicing input', () => {
    for (let i = 0; i < 100; i++) {
      let patch = new Patch({seed: Date.now(), combineChanges: false})

      patch.spliceWithText({row: 0, column: 1}, {row: 0, column: 1}, 'X')
      patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 0}, 'hello')
      patch.spliceWithText({row: 0, column: 8}, {row: 0, column: 0}, ' world')
      patch.spliceWithText({row: 0, column: 16}, {row: 0, column: 1}, 'X')

      patch.spliceInput({row: 0, column: 3}, {row: 0, column: 0}, {row: 0, column: 0})

      assert.deepEqual(patch.getChanges(), [
        {start: {row: 0, column: 1}, oldExtent: {row: 0, column: 1}, newExtent: {row: 0, column: 1}, newText: 'X'},
        {start: {row: 0, column: 5}, oldExtent: {row: 0, column: 1}, newExtent: {row: 0, column: 1}, newText: 'X'}
      ])
    }
  })

  it('clips to the left of regions with empty input extents when translating input positions', () => {
    for (let i = 0; i < 100; i++) {
      let patch = new Patch({seed: Date.now(), combineChanges: false})

      patch.spliceWithText({row: 0, column: 1}, {row: 0, column: 1}, 'X')
      patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 0}, 'hello')
      patch.spliceWithText({row: 0, column: 8}, {row: 0, column: 0}, ' world')
      patch.spliceWithText({row: 0, column: 16}, {row: 0, column: 1}, 'X')

      assert.deepEqual(patch.translateInputPosition({row: 0, column: 3}), {row: 0, column: 3})
    }
  })

  it('allows metadata to be associated with splices', () => {
    let patch = new Patch({seed: 123})
    patch.splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5}, {metadata: {a: 1}})
    patch.splice({row: 0, column: 10}, {row: 0, column: 5}, {row: 0, column: 5}, {metadata: {b: 2}})

    let iterator = patch.buildIterator()
    iterator.rewind()
    assert(!iterator.inChange())

    iterator.moveToSuccessor()
    assert(iterator.inChange())
    assert.deepEqual(iterator.getMetadata(), {a: 1})

    iterator.moveToSuccessor()
    assert(!iterator.inChange())

    iterator.moveToSuccessor()
    assert(iterator.inChange())
    assert.deepEqual(iterator.getMetadata(), {b: 2})
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
      let batchMode = Boolean(random(2))
      let patch = new Patch({seed, combineChanges, batchMode})

      for (let j = 0; j < 10; j++) {
        if (random(10) < 2) { // 20% splice input
          let {start: inputStart, oldExtent, newExtent, newText} = input.performRandomSplice()
          let outputStart = patch.translateInputPosition(inputStart)
          let outputOldExtent = traversalDistance(
            patch.translateInputPosition(traverse(inputStart, oldExtent)),
            outputStart
          )
          output.splice(outputStart, outputOldExtent, newText)
          let result = patch.spliceInput(inputStart, oldExtent, newExtent)
          // document.write(`<div>after spliceInput(${formatPoint(inputStart)}, ${formatPoint(oldExtent)}, ${formatPoint(newExtent)}, ${newText})</div>`)
          // document.write(patch.toHTML())
          // document.write('<hr>')
          assert.deepEqual(result.start, outputStart, seedMessage)
          assert.deepEqual(result.oldExtent, outputOldExtent, seedMessage)
          assert.deepEqual(result.newExtent, newExtent, seedMessage)
        } else { // 80% normal splice
          let {start, oldExtent, newExtent, newText} = output.performRandomSplice()
          patch.spliceWithText(start, oldExtent, newText)
          // document.write(`<div>after splice(${formatPoint(start)}, ${formatPoint(oldExtent)}, ${formatPoint(newExtent)}, ${newText})</div>`)
          // document.write(patch.toHTML())
          // document.write('<hr>')
        }

        let shouldRebalance = Boolean(random(2))
        if (batchMode && shouldRebalance) patch.rebalance()

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
          synthesizedOutput += patch.iterator.getNewText()
        } else {
          synthesizedOutput += input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd())
        }
      } while (patch.iterator.moveToSuccessor())

      assert.equal(synthesizedOutput, output.getText(), seedMessage)

      for (let {start, oldExtent, newText} of patch.getChanges()) {
        input.splice(start, oldExtent, newText)
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
