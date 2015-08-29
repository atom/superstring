import {getPrefix, getSuffix} from '../../src/text-helpers'

describe('textHelpers.getPrefix(text, point)', () => {
  it('returns the prefix preceding the given position', () => {
    expect(getPrefix('hello world', {row: 0, column: 5})).to.equal('hello')
    expect(getPrefix('hello\nworld', {row: 1, column: 0})).to.equal('hello\n')
    expect(getPrefix('hello\nworld', {row: 1, column: 3})).to.equal('hello\nwor')
  })
})

describe('textHelpers.getPrefix(text, point)', () => {
  it('returns the suffix following the given position', () => {
    expect(getSuffix('hello world', {row: 0, column: 5})).to.equal(' world')
    expect(getSuffix('hello\nworld', {row: 1, column: 0})).to.equal('world')
    expect(getSuffix('hello\nworld', {row: 1, column: 3})).to.equal('ld')
  })
})
