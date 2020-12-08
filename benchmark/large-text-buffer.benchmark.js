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

getText().then(async (txt) => {
  const buffer = new TextBuffer()

  console.log('\n running findWordsWithSubsequence tests... \n')

  const sizes = [ ['100b', 100], ['1kb', 1000], ['1MB', 1000000], ['51MB', 100000000], ['119MB', txt.length]]

  const test = async (word, size) => {
    buffer.setText(txt.slice(0, size[1]))
    const ti = performance.now()
    await buffer.findWordsWithSubsequence(word, '', 100)
    const tf = performance.now()
    console.log(`In ${size[0]} file, time to find "${word}" was: ${' '.repeat(50-word.length-size[0].length)} ${(tf-ti).toFixed(5)} ms`)
  }
  for (const size of sizes) {
    for (const word of ["Morocco", "Austria", "France", "Liechtenstein", "Republic of the Congo", "Antigua and Barbuda", "Japan"]) {
      await test(word, size)
    }
    console.log('\n')
  }
}).then(() => {
  console.log('findWordsWithSubsequence tests finished \n')
})
