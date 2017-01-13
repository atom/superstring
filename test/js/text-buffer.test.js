// const assert = require('assert')
// const temp = require('temp').track()
// const {TextBuffer} = require('../..')
// const fs = require('fs')
//
// describe('TextBuffer', () => {
//   describe('load()', () => {
//     it.only('loads the contents of the given filePath in the background', () => {
//       const {path: filePath} = temp.openSync()
//       const content = 'a\nb\nc\n'
//       fs.writeFileSync(filePath, content)
//       const buffer = new TextBuffer({filePath})
//       assert.equal(buffer.getText(), content)
//     })
//   })
// })
