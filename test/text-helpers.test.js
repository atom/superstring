import {getPrefix, getSuffix} from '../src/text-helpers'

describe('textHelpers.getPrefix(text, point)', () => {
  it('returns the prefix preceding the given position', () => {
    assert.equal(getPrefix('hello world', {row: 0, column: 5}), 'hello')
    assert.equal(getPrefix('hello\nworld', {row: 1, column: 0}), 'hello\n')
    assert.equal(getPrefix('hello\nworld', {row: 1, column: 3}), 'hello\nwor')
  })
})

describe('textHelpers.getPrefix(text, point)', () => {
  it('returns the suffix following the given position', () => {
    assert.equal(getSuffix('hello world', {row: 0, column: 5}), ' world')
    assert.equal(getSuffix('hello\nworld', {row: 1, column: 0}), 'world')
    assert.equal(getSuffix('hello\nworld', {row: 1, column: 3}), 'ld')
  })
})
