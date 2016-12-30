try {
  module.exports = require('./build/Release/superstring.node')
} catch (e) {
  module.exports = require('./build/Debug/superstring.node')
}
