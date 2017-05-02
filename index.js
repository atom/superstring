if (process.env.SUPERSTRING_USE_BROWSER_VERSION) {
  module.exports = require('./browser');
} else {
  try {
    module.exports = require('./build/Release/superstring.node')
  } catch (e1) {
    try {
      module.exports = require('./build/Debug/superstring.node')
    } catch (e2) {
      throw e1
    }
  }
}
