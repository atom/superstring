'use strict'

require('babel/register')({
  optional: ['es7.asyncFunctions']
})

var chai = require('chai')
global.expect = chai.expect
global.assert = chai.assert
