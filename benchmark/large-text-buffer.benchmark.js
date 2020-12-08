const { TextBuffer } = require('..')

const fs = require('fs')
const {promisify} = require('util')
const readFile = promisify(fs.readFile)
const path = require('path')
const download = require('download')
const {performance} = require("perf_hooks")

async function getText() {
  const filePath = path.join(__dirname, '1000000 Sales Records.csv')
  if (!fs.existsSync(filePath)) {
    // 122MB file
    await download(
      'http://eforexcel.com/wp/wp-content/uploads/2017/07/1000000%20Sales%20Records.zip',
      __dirname,
      {extract: true}
    )
  }
  return await readFile(filePath)
}

getText().then(txt => {
  const buffer = new TextBuffer()

  console.log('\n running findWordsWithSubsequence tests... \n')

  const sizes = [ ['100b', 100], ['1kb', 1000], ['1MB', 1000000], ['51MB', 100000000], ['119MB', txt.length]]

  const test = (word, size) => {
    buffer.setText(txt.slice(0, size[1]))
    const ti = performance.now()
    return buffer.findWordsWithSubsequence(word, '', 100).then(sugs => {
      const tf = performance.now()
      console.log(`Time to find "${word}" in ${size[0]} file: ${(tf-ti).toFixed(5)} ms`)
    })
  }

  for (const word of ["Morocco", "Austria", "France", "Liechtenstein", "Republic of the Congo", "Antigua and Barbuda", "Japan"]) {
    sizes.reduce((promise, size) => {
      return promise.then(() => test(word, size))
    }, Promise.resolve())
  }
}).then(() => {
  console.log('findWordsWithSubsequence tests finished \n')
})
