import Random from 'random-seed'
import Patch from '../src/patch'
import {INFINITY_POINT, traverse, traversalDistance, format as formatPoint, isZero as isZeroPoint} from '../src/point-helpers'
import TestDocument from './helpers/test-document'
import './helpers/add-to-html-methods'

describe('Patch', function () {
  it('correctly serializes and deserializes changes', function () {
    let patch = new Patch
    patch.spliceWithText({row: 0, column: 3}, 'abcd', 'hello')
    patch.spliceWithText({row: 1, column: 1}, '', 'hey')
    patch.splice({row: 0, column: 15}, {row: 0, column: 10}, {row: 0, column: 0})
    patch.splice({row: 0, column: 0}, {row: 0, column: 0}, {row: 3, column: 0})
    patch.spliceWithText({row: 4, column: 2}, 'hi', 'ho')

    let deserializedPatch = Patch.deserialize(patch.serialize())
    assert.deepEqual(patch.getChanges(), deserializedPatch.getChanges())
    assert.throws(() => deserializedPatch.splice({row: 0, column: 0}, {row: 1, column: 0}, {row: 0, column: 3}))
  })

  it('provides a read-only view on the composition of multiple patches', function () {
    let patches = [new Patch(), new Patch(), new Patch()]
    patches[0].spliceWithText({row: 0, column: 3}, 'ciao', 'hello')
    patches[0].spliceWithText({row: 1, column: 1}, '', 'hey')
    patches[1].splice({row: 0, column: 15}, {row: 0, column: 10}, {row: 0, column: 0})
    patches[1].splice({row: 0, column: 0}, {row: 0, column: 0}, {row: 3, column: 0})
    patches[2].spliceWithText({row: 4, column: 2}, 'so', 'ho')

    let composedPatch = Patch.compose(patches)
    assert.deepEqual(composedPatch.getChanges(), [
      {oldStart: {row: 0, column: 0}, newStart: {row: 0, column: 0}, oldExtent: {row: 0, column: 0}, newExtent: {row: 3, column: 0}},
      {oldStart: {row: 0, column: 3}, newStart: {row: 3, column: 3}, oldExtent: {row: 0, column: 4}, newExtent: {row: 0, column: 5}, newText: 'hello', oldText: 'ciao'},
      {oldStart: {row: 0, column: 14}, newStart: {row: 3, column: 15}, oldExtent: {row: 0, column: 10}, newExtent: {row: 0, column: 0}},
      {oldStart: {row: 1, column: 1}, newStart: {row: 4, column: 1}, oldExtent: {row: 0, column: 0}, newExtent: {row: 0, column: 3}, newText: 'hho', oldText: ''}
    ])

    assert.throws(() => composedPatch.splice({row: 0, column: 0}, {row: 1, column: 0}, {row: 2, column: 0}))
  })

  it('provides a read-only view on the inverse of a patch', function () {
    let patch = new Patch()
    patch.spliceWithText({row: 0, column: 3}, 'ciao', 'hello')
    patch.spliceWithText({row: 0, column: 10}, 'quick', 'world')

    let invertedPatch = Patch.invert(patch)
    assert.deepEqual(invertedPatch.getChanges(), [
      {oldStart: {row: 0, column: 3}, newStart: {row: 0, column: 3}, oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 4}, newText: 'ciao', oldText: 'hello'},
      {oldStart: {row: 0, column: 10}, newStart: {row: 0, column: 9}, oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 5}, newText: 'quick', oldText: 'world'}
    ])
    assert.throws(() => invertedPatch.splice({row: 0, column: 0}, {row: 1, column: 0}, {row: 2, column: 0}))
  })

  it('provides a read-only view of a patch representing a single hunk', function () {
    let hunk = Patch.hunk({
      newStart: {row: 0, column: 3},
      oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 7},
      newText: 'ciao', oldText: 'hello'
    })
    assert.deepEqual(hunk.getChanges(), [
      {oldStart: {row: 0, column: 3}, newStart: {row: 0, column: 3}, oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 7}, newText: 'ciao', oldText: 'hello'}
    ])
    assert.throws(() => hunk.splice({row: 0, column: 0}, {row: 1, column: 0}, {row: 2, column: 0}))
  })

  it('correctly records basic non-overlapping splices', function () {
    let patch = new Patch()
    patch.spliceWithText({row: 0, column: 3}, 'ciao', 'hello')
    patch.spliceWithText({row: 0, column: 10}, 'quick', 'world')
    assert.deepEqual(patch.getChanges(), [
      {oldStart: {row: 0, column: 3}, newStart: {row: 0, column: 3}, oldExtent: {row: 0, column: 4}, newExtent: {row: 0, column: 5}, newText: 'hello', oldText: 'ciao'},
      {oldStart: {row: 0, column: 9}, newStart: {row: 0, column: 10}, oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 5}, newText: 'world', oldText: 'quick'}
    ])
  })

  it('correctly records basic overlapping splices', function () {
    let patch = new Patch()
    patch.spliceWithText({row: 0, column: 3}, 'hola', 'hello world')
    patch.spliceWithText({row: 0, column: 9}, 'zapping', 'sun')
    assert.deepEqual(patch.getChanges(), [
      {oldStart: {row: 0, column: 3}, newStart: {row: 0, column: 3}, oldExtent: {row: 0, column: 6}, newExtent: {row: 0, column: 9}, newText: 'hello sun', oldText: 'holang'}
    ])
  })

  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 100; i++) {
      let seed = Date.now()
      let seedMessage = `Random seed: ${seed}`
      let random = new Random(seed)
      let input = new TestDocument(seed)
      let output = input.clone()
      let patch = new Patch()

      for (let j = 0; j < 10; j++) {
        let {start, oldText, oldExtent, newExtent, newText} = output.performRandomSplice()
        patch.spliceWithText(start, oldText, newText)
        // document.write(`<div>after splice(${formatPoint(start)}, ${formatPoint(oldExtent)}, ${formatPoint(newExtent)}, ${JSON.stringify(newText)}, ${JSON.stringify(oldText)})</div>`)
        // document.write(patch.toHTML())
        // document.write('<hr>')

        let shouldRebalance = Boolean(random(2))
        if (shouldRebalance) patch.rebalance()

        verifyOutputChanges(patch, input, output, seedMessage)
        verifyInputChanges(patch, input, output, seedMessage)
      }
    }

    function verifyOutputChanges (patch, input, output, seedMessage) {
      let synthesizedOutput = ''
      let synthesizedInput = ''
      patch.iterator.moveToBeginning()
      do {
        if (patch.iterator.inChange()) {
          assert(!(isZeroPoint(patch.iterator.getInputExtent()) && isZeroPoint(patch.iterator.getOutputExtent())), "Empty region found. " + seedMessage);
          synthesizedOutput += patch.iterator.getNewText()
          synthesizedInput += patch.iterator.getOldText()
        } else {
          synthesizedOutput += input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd())
          synthesizedInput += input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd())
        }
      } while (patch.iterator.moveToSuccessor())

      assert.equal(synthesizedInput, input.getText(), seedMessage)
      assert.equal(synthesizedOutput, output.getText(), seedMessage)

      synthesizedInput = output.clone()
      synthesizedOutput = input.clone()
      for (let {oldStart, newStart, oldExtent, newExtent, oldText, newText} of patch.getChanges()) {
        synthesizedInput.splice(oldStart, newExtent, oldText)
        synthesizedOutput.splice(newStart, oldExtent, newText)
      }

      assert.equal(synthesizedInput.getText(), input.getText(), seedMessage)
      assert.equal(synthesizedOutput.getText(), output.getText(), seedMessage)
    }

    function verifyInputChanges (patch, input, output, seedMessage) {
      patch.iterator.moveToEnd()
      let synthesizedInput = input.getTextInRange(patch.iterator.getInputEnd(), INFINITY_POINT)
      let synthesizedOutput = input.getTextInRange(patch.iterator.getInputEnd(), INFINITY_POINT)
      do {
        if (patch.iterator.inChange()) {
          assert(!(isZeroPoint(patch.iterator.getInputExtent()) && isZeroPoint(patch.iterator.getOutputExtent())), "Empty region found. " + seedMessage);
          synthesizedOutput = patch.iterator.getNewText() + synthesizedOutput
          synthesizedInput = patch.iterator.getOldText() + synthesizedInput
        } else {
          synthesizedOutput = input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd()) + synthesizedOutput
          synthesizedInput = input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd()) + synthesizedInput
        }
      } while (patch.iterator.moveToPredecessor())

      assert.equal(synthesizedInput, input.getText(), seedMessage)
      assert.equal(synthesizedOutput, output.getText(), seedMessage)

      synthesizedOutput = input.clone()
      synthesizedInput = output.clone()
      for (let {oldStart, newStart, oldExtent, newExtent, oldText, newText} of patch.getChanges().slice().reverse()) {
        synthesizedOutput.splice(oldStart, oldExtent, newText)
        synthesizedInput.splice(newStart, newExtent, oldText)
      }

      assert.equal(synthesizedOutput.getText(), output.getText(), seedMessage)
      assert.equal(synthesizedInput.getText(), input.getText(), seedMessage)
    }
  })
})
