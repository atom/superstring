const http = require('http')
const fs = require('fs')
const unzip = require('unzip')
const { TextBuffer } = require('..')

const unzipper = unzip.Parse()

const getText = () => {
  return new Promise(resolve => {
    console.log('fetching text file...')
    const req = http.get({
      hostname: 'www.acleddata.com',
      port: 80,
      // 51 MB text file
      path: '/wp-content/uploads/2017/01/ACLED-Version-7-All-Africa-1997-2016_csv_dyadic-file.zip',
      agent: false
    }, res => {
      res
        .pipe(unzipper)
        .on('entry', entry => {
          let data = '';
          entry.on('data', chunk => data += chunk);
          entry.on('end', () => {
            resolve(data)
          });
        })
    })

    req.end()
  })
}

const timer = size => `Time to find "cat" in ${size} file`

getText().then(txt => {
  const buffer = new TextBuffer()

  console.log('running findWordsWithSubsequence tests...')

  const sizes = [['10b', 10], ['100b', 100], ['1kb', 1000], ['1MB', 1000000], ['51MB', 100000000]]

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
