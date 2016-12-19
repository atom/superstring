'use strict';

require('babel/register')()
const Random = require('random-seed')
const NativeMarkerIndex = require('../src/native/marker-index')
const JSMarkerIndex = require('../src/js/marker-index')
const PointHelpers = require('../src/js/point-helpers')
const traverse = PointHelpers.traverse
const traversal = PointHelpers.traversal
const compare = PointHelpers.compare

let random = new Random(1)
let markerIds = []
let idCounter = 1
let nativeMarkerIndex = new NativeMarkerIndex()
let jsMarkerIndex = new JSMarkerIndex()
let insertOperations = []
let spliceOperations = []
let deleteOperations = []
let rangeQueryOperations = []

function runBenchmark () {
  for (let i = 0; i < 20000; i++) {
    enqueueInsert()
    enqueueSplice()
    enqueueDelete()
  }

  for (let i = 0; i < 500; i++) {
    enqueueRangeQuery()
  }

  profileOperations('inserts', insertOperations)
  profileOperations('rangeQueries', rangeQueryOperations)
  profileOperations('splices', spliceOperations)
  profileOperations('deletes', deleteOperations)
}

function profileOperations (description, operations) {
  let name = 'native: ' + description
  console.time(name)
  for (let operation of operations) {
    nativeMarkerIndex[operation[0]].apply(nativeMarkerIndex, operation[1])
  }
  console.timeEnd(name)

  name = 'js: ' + description
  console.time(name)
  for (let operation of operations) {
    jsMarkerIndex[operation[0]].apply(jsMarkerIndex, operation[1])
  }
  console.timeEnd(name)
}

function enqueueInsert () {
  let id = (idCounter++).toString()
  let range = getRange()
  let start = range[0]
  let end = range[1]
  let exclusive = Boolean(random(2))
  markerIds.push(id)
  insertOperations.push(['insert', [id, start, end]])
  insertOperations.push(['setExclusive', [id, exclusive]])
}

function enqueueSplice () {
  spliceOperations.push(['splice', getSplice()])
}

function enqueueRangeQuery() {
  rangeQueryOperations.push(['findIntersecting', getRange()])
}

function enqueueDelete () {
  let id = markerIds.splice(random(markerIds.length), 1)
  deleteOperations.push(['delete', [id]])
}

function getRange () {
  let start = {row: random(100), column: random(100)}
  let end = start
  while (random(3) > 0) {
    end = traverse(end, {row: random.intBetween(-10, 10), column: random.intBetween(-10, 10)})
  }
  end.row = Math.max(end.row, 0)
  end.column = Math.max(end.column, 0)

  if (compare(start, end) <= 0) {
    return [start, end]
  } else {
    return [end, start]
  }
}

function getSplice () {
  let range = getRange()
  let start = range[0]
  let oldEnd = range[1]
  let oldExtent = traversal(oldEnd, start)
  let newExtent = {row: 0, column: 0}
  while (random(2)) {
    newExtent = traverse(newExtent, {row: random(10), column: random(10)})
  }
  return [start, oldExtent, newExtent]
}

runBenchmark()
