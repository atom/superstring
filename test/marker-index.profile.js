import Random from 'random-seed'
import MarkerIndex from '../src/marker-index'
import {traverse, traversal, compare} from '../src/point-helpers'

describe('MarkerIndex', () => {
  it('profile random insertion', function () {
    let random = new Random(1)
    let operations = []
    let markerIds = []
    let idCounter = 1

    for (let i = 0; i < 10000; i++) {
      let n = random(10)
      if (n >= 4) { // 60% insert
        enqueueInsert()
      } else if (n >= 2) { // 20% splice
        enqueueSplice()
      } else if (markerIds.length > 0) { // 20% delete
        enqueueDelete()
      }
    }

    let markerIndex = new MarkerIndex()

    console.profile()
    console.time('operations')
    for (let [method, args] of operations) {
      markerIndex[method](...args)
    }
    console.profileEnd()
    console.timeEnd('operations')

    function enqueueInsert () {
      let id = String.fromCharCode(idCounter++)
      let [start, end] = getRange()
      let exclusive = Boolean(random(2))
      markerIds.push(id)
      operations.push(['insert', [id, start, end]])
      operations.push(['setExclusive', [id, exclusive]])
    }

    function enqueueSplice () {
      operations.push(['splice', getSplice()])
    }

    function enqueueDelete () {
      let id = markerIds.splice(random(markerIds.length), 1)
      operations.push(['delete', [id]])
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
      let [start, oldEnd] = getRange()
      let oldExtent = traversal(oldEnd, start)
      let newExtent = {row: 0, column: 0}
      while (random(2)) {
        newExtent = traverse(newExtent, {row: random(10), column: random(10)})
      }
      return [start, oldExtent, newExtent]
    }
  })
})
