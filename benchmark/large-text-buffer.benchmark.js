const { TextBuffer } = require('..')

const fs = require('fs')
const {promisify} = require('util')
const readFile = promisify(fs.readFile)
const path = require('path')
const download = require('download')

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

const timer = size => `Time to find "cat" in ${size} file`

getText().then(txt => {
  const buffer = new TextBuffer()

  console.log('running findWordsWithSubsequence tests...')

  const sizes = [['10b', 10], ['100b', 100], ['1kb', 1000], ['1MB', 1000000], ['51MB', 100000000], ['119MB', txt.length]]

  const test = size => {
    const _timer = timer(size[0])
    buffer.setText(txt.slice(0, size[1]))
    console.time(_timer)
    return buffer.findWordsWithSubsequence('cat', '', 100).then(sugs => {
      console.timeEnd(_timer)
    })
  }

  return sizes.reduce((promise, size) => {
    return promise.then(() => test(size))
  }, Promise.resolve())
}).then(() => {
  console.log('finished')
})
