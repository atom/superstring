'use strict';

console.log(' running marker-index tests... \n')

const Random = require('random-seed')
const {performance} = require("perf_hooks")

const {MarkerIndex} = require('..')
const {traverse, traversalDistance, compare} = require('../test/js/helpers/point-helpers')

let random = new Random(1)
let markerIds = []
let idCounter = 1
let lastInsertionEnd = {row: 0, column: 0}
let markerIndex = null
let sequentialInsertOperations = []
let insertOperations = []
let spliceOperations = []
let deleteOperations = []
let rangeQueryOperations = []

function runBenchmark () {
  for (let i = 0; i < 40000; i++) {
    enqueueSequentialInsert()
  }

  for (let i = 0; i < 40000; i++) {
    enqueueInsert()
    enqueueSplice()
    enqueueDelete()
  }

  for (let i = 0; i < 500; i++) {
    enqueueRangeQuery()
  }

  markerIndex = new MarkerIndex()
  profileOperations('sequential inserts', sequentialInsertOperations)

  markerIndex = new MarkerIndex()
  profileOperations('inserts', insertOperations)
  profileOperations('range queries', rangeQueryOperations)
  profileOperations('splices', spliceOperations)
  profileOperations('deletes', deleteOperations)
}

function profileOperations (name, operations) {
  const ti1 = performance.now()
  for (let i = 0, n = operations.length; i < n; i++) {
    const operation = operations[i]
    markerIndex[operation[0]].apply(markerIndex, operation[1])
  }
  const tf1 = performance.now()
  console.log(`${name} ${' '.repeat(80-name.length)} ${(tf1-ti1).toFixed(3)} ms`)
}

function enqueueSequentialInsert () {
  let id = (idCounter++).toString()
  let row, startColumn, endColumn
  if (random(10) < 3) {
    row = lastInsertionEnd.row + 1 + random(3)
    startColumn = random(100)
    endColumn = startColumn + random(20)
  } else {
    row = lastInsertionEnd.row
    startColumn = lastInsertionEnd.column + 1 + random(20)
    endColumn = startColumn + random(20)
  }
  lastInsertionEnd = {row, column: endColumn}
  sequentialInsertOperations.push(['insert', [id, {row, column: startColumn}, lastInsertionEnd]])
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
  let oldExtent = traversalDistance(oldEnd, start)
  let newExtent = {row: 0, column: 0}
  while (random(2)) {
    newExtent = traverse(newExtent, {row: random(10), column: random(10)})
  }
  return [start, oldExtent, newExtent]
}

runBenchmark()

console.log(' \n marker-index finished \n')
