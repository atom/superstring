if (process.env.FORCE_BROWSER_FALLBACK) {
    module.exports = require('./browser');
} else {
    try {
        module.exports = require('./build/Release/superstring.node')
    } catch (e) {
        module.exports = require('./build/Debug/superstring.node')
    }
}
