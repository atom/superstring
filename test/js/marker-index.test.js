const Random = require('random-seed')
const {traverse, traversalDistance, compare, isZero, max, format: formatPoint} = require('./helpers/point-helpers')
const {assert} = require('chai')
const {MarkerIndex} = require('../..')

describe('MarkerIndex', () => {
  it('maintains correct marker positions during randomized insertions and mutations', function () {
    this.timeout(Infinity)

    let seed, seedMessage, random, markerIndex, markers, idCounter

    for (let i = 0; i < 1000; i++) {
      seed = 42;//Date.now()
      seedMessage = `Random Seed: ${seed}`
      random = new Random(seed)
      markerIndex = new MarkerIndex(seed)
      markers = []
      idCounter = 1

      for (let j = 0; j < 50; j++) {
        let n = random(10)
        if (n >= 4) { // 60% insert
          performInsert()
        } else if (n >= 2) { // 20% splice
          performSplice()
        } else if (markers.length > 0) {
          performDelete()
        }

        verifyRanges()
        verifyHighestPossiblePaths()
      }

      const verifications = [
        verifyRanges,
        testDump,
        testFindIntersecting,
        testFindContaining,
        testFindContainedIn,
        testFindStartingIn,
        testFindEndingIn,
        testFindStartingAt,
        testFindEndingAt,
      ].sort((a, b) => random.intBetween(-1, 1))

      verifications.forEach(verification => verification())
    }

    //  uncomment for debug output in electron (`npm run tdd`)
    function write (f) {
      // document.write(f())
    }

    function verifyRanges () {
      for (let marker of markers) {
        let range = markerIndex.getRange(marker.id)
        assert.deepEqual(range.start, marker.start, `Marker ${marker.id} start. ` + seedMessage)
        assert.deepEqual(range.end, marker.end, `Marker ${marker.id} end. ` + seedMessage)
      }
    }

    function testDump () {
      if (markers.length === 0) return

      let expectedSnapshot = {}

      for (let marker of markers) {
        expectedSnapshot[marker.id] = {start: marker.start, end: marker.end}
      }

      let actualSnapshot = markerIndex.dump()

      assert.deepEqual(actualSnapshot, expectedSnapshot, seedMessage)
    }

    function verifyHighestPossiblePaths (node, alreadySeen) {
      if (!node) {
        if (markerIndex.root) verifyHighestPossiblePaths(markerIndex.root, new Set())
        return
      }

      for (let markerId of node.leftMarkerIds) {
        assert(!alreadySeen.has(markerId), `Redundant path for ${markerId}. ` + seedMessage)
      }
      for (let markerId of node.rightMarkerIds) {
        assert(!alreadySeen.has(markerId), `Redundant paths for ${markerId}. ` + seedMessage)
      }

      if (node.left) {
        let alreadySeenOnLeft = new Set()
        for (let markerId of alreadySeen) {
          alreadySeenOnLeft.add(markerId)
        }
        for (let markerId of node.leftMarkerIds) {
          alreadySeenOnLeft.add(markerId)
        }
        verifyHighestPossiblePaths(node.left, alreadySeenOnLeft)
      }

      if (node.right) {
        let alreadySeenOnRight = new Set()
        for (let markerId of alreadySeen) {
          alreadySeenOnRight.add(markerId)
        }
        for (let markerId of node.rightMarkerIds) {
          alreadySeenOnRight.add(markerId)
        }
        verifyHighestPossiblePaths(node.right, alreadySeenOnRight)
      }
    }

    function verifyContinuousPaths () {
      let startedMarkers = new Set()
      let endedMarkers = new Set()

      let iterator = markerIndex.iterator
      iterator.reset()
      while (iterator.node && iterator.node.left) iterator.descendLeft()

      let node = iterator.node
      while (node) {
        for (let markerId of node.leftMarkerIds) {
          assert(!endedMarkers.has(markerId), `Marker ${markerId} in left markers, but already ended. ` + seedMessage)
          assert(startedMarkers.has(markerId), `Marker ${markerId} in left markers, but not yet started. ` + seedMessage)
        }

        for (let markerId of node.startMarkerIds) {
          assert(!endedMarkers.has(markerId), `Marker ${markerId} in start markers, but already ended. ` + seedMessage)
          assert(!startedMarkers.has(markerId), `Marker ${markerId} in start markers, but already started. ` + seedMessage)
          startedMarkers.add(markerId)
        }

        for (let markerId of node.endMarkerIds) {
          assert(startedMarkers.has(markerId), `Marker ${markerId} in end markers, but not yet started. ` + seedMessage)
          startedMarkers.delete(markerId)
          endedMarkers.add(markerId)
        }

        for (let markerId of node.rightMarkerIds) {
          assert(!endedMarkers.has(markerId), `Marker ${markerId} in right markers, but already ended. ` + seedMessage)
          assert(startedMarkers.has(markerId), `Marker ${markerId} in right markers, but not yet started. ` + seedMessage)
        }

        iterator.moveToSuccessor()
        node = iterator.node
      }
    }

    function testFindIntersecting () {
      for (let i = 0; i < 10; i++) {
        let [start, end] = getRange()

        let expectedIds = new Set()
        for (let marker of markers) {
          if (compare(marker.start, end) <= 0 && compare(start, marker.end) <= 0) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = markerIndex.findIntersecting(start, end)

        assert.equal(actualIds.size, expectedIds.size, seedMessage)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
        }
      }
    }

    function testFindContaining () {
      for (let i = 0; i < 10; i++) {
        let [start, end] = getRange()

        let expectedIds = new Set()
        for (let marker of markers) {
          if (compare(marker.start, start) <= 0 && compare(end, marker.end) <= 0) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = markerIndex.findContaining(start, end)
        assert.equal(actualIds.size, expectedIds.size, seedMessage)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
        }
      }
    }

    function testFindContainedIn () {
      for (let i = 0; i < 10; i++) {
        let [start, end] = getRange()

        let expectedIds = new Set()
        for (let marker of markers) {
          if (compare(start, marker.start) <= 0 && compare(marker.end, end) <= 0) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = markerIndex.findContainedIn(start, end)
        assert.equal(actualIds.size, expectedIds.size, seedMessage)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
        }
      }
    }

    function testFindStartingIn () {
      for (let i = 0; i < 10; i++) {
        let [start, end] = getRange()

        let expectedIds = new Set()
        for (let marker of markers) {
          if (compare(start, marker.start) <= 0 && compare(marker.start, end) <= 0) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = markerIndex.findStartingIn(start, end)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to start in (${formatPoint(start)}, ${formatPoint(end)}). ` + seedMessage)
        }
        assert.equal(actualIds.size, expectedIds.size, seedMessage)
      }
    }

    function testFindEndingIn () {
      for (let i = 0; i < 10; i++) {
        let [start, end] = getRange()

        let expectedIds = new Set()
        for (let marker of markers) {
          if (compare(start, marker.end) <= 0 && compare(marker.end, end) <= 0) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = markerIndex.findEndingIn(start, end)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
        }
        assert.equal(actualIds.size, expectedIds.size, seedMessage)
      }
    }

    function testFindStartingAt () {
      for (let i = 0; i < 10; i++) {
        let point = {row: random(100), column: random(100)}

        let expectedIds = new Set()
        for (let marker of markers) {
          if (compare(marker.start, point) === 0) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = markerIndex.findStartingAt(point)
        assert.equal(actualIds.size, expectedIds.size, seedMessage)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
        }
      }
    }

    function testFindEndingAt () {
      for (let i = 0; i < 10; i++) {
        let point = {row: random(100), column: random(100)}

        let expectedIds = new Set()
        for (let marker of markers) {
          if (compare(marker.end, point) === 0) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = markerIndex.findEndingAt(point)
        assert.equal(actualIds.size, expectedIds.size, seedMessage)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
        }
      }
    }

    function performInsert () {
      let id = idCounter++
      let [start, end] = getRange()
      let exclusive = !!random(2)
      write(() => `insert ${id}, ${formatPoint(start)}, ${formatPoint(end)}, exclusive: ${exclusive}`)
      assert(!markerIndex.has(id), `Expected marker index to not have ${id}. ` + seedMessage)
      markerIndex.insert(id, start, end)
      if (exclusive) markerIndex.setExclusive(id, true)
      markers.push({id, start, end, exclusive})
      assert(markerIndex.has(id), `Expected marker index to have ${id}. ` + seedMessage)
    }

    function performSplice () {
      let [start, oldExtent, newExtent] = getSplice()
      write(() => `splice ${formatPoint(start)}, ${formatPoint(oldExtent)}, ${formatPoint(newExtent)}`)
      let actualInvalidatedSets = markerIndex.splice(start, oldExtent, newExtent)
      let expectedInvalidatedSets = applySplice(markers, start, oldExtent, newExtent)
      checkInvalidatedSets(actualInvalidatedSets, expectedInvalidatedSets)
    }

    function performDelete () {
      let [{id}] = markers.splice(random(markers.length), 1)
      write(() => `delete ${id}`)
      assert(markerIndex.has(id), `Expected marker index to have ${id}. ` + seedMessage)
      markerIndex.delete(id)
      assert(!markerIndex.has(id), `Expected marker index to not have ${id}. ` + seedMessage)
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
      let oldExtent = traversalDistance(oldEnd, start)
      let newExtent = {row: 0, column: 0}
      while (random(2)) {
        newExtent = traverse(newExtent, {row: random(10), column: random(10)})
      }
      return [start, oldExtent, newExtent]
    }

    function applySplice (markers, spliceStart, oldExtent, newExtent) {
      if (isZero(oldExtent) && isZero(newExtent)) return

      let spliceOldEnd = traverse(spliceStart, oldExtent)
      let spliceNewEnd = traverse(spliceStart, newExtent)
      let spliceDelta = traversalDistance(newExtent, oldExtent)
      let isInsertion = isZero(oldExtent)

      let invalidated = {
        touch: new Set,
        inside: new Set,
        overlap: new Set,
        surround: new Set
      }

      for (let marker of markers) {
        let isEmpty = compare(marker.start, marker.end) === 0

        if (compare(spliceStart, marker.end) <= 0 && compare(marker.start, spliceOldEnd) <= 0) {
          let invalidateInside = compare(spliceStart, marker.end) < 0 && compare(spliceOldEnd, marker.start) > 0
          let markerStartsWithinSplice, markerEndsWithinSplice

          if (marker.exclusive) {
            markerStartsWithinSplice =
              (compare(spliceStart, marker.start) < 0 || (!isEmpty && compare(spliceStart, marker.start) === 0)) &&
                compare(spliceOldEnd, marker.start) > 0
            markerEndsWithinSplice =
              compare(spliceStart, marker.end) < 0 &&
                (compare(spliceOldEnd, marker.end) > 0 || (!isEmpty && compare(spliceOldEnd, marker.end) === 0))
          } else {
            invalidateInside = invalidateInside || ((!isEmpty || isInsertion) && (compare(spliceStart, marker.start) === 0 || compare(spliceOldEnd, marker.end) === 0))
            markerStartsWithinSplice = compare(spliceStart, marker.start) < 0 && compare(marker.start, spliceOldEnd) < 0
            markerEndsWithinSplice = compare(spliceStart, marker.end) < 0 && compare(marker.end, spliceOldEnd) < 0
          }

          invalidated.touch.add(marker.id)
          if (invalidateInside) {
            invalidated.inside.add(marker.id)
          }
          if (markerStartsWithinSplice || markerEndsWithinSplice) {
            invalidated.overlap.add(marker.id)
          }
          if (markerStartsWithinSplice && markerEndsWithinSplice) {
            invalidated.surround.add(marker.id)
          }
        }

        let moveMarkerStart =
          (compare(spliceStart, marker.start) < 0) ||
            (marker.exclusive && (!isEmpty || isInsertion) && compare(spliceStart, marker.start) === 0)

        let moveMarkerEnd =
          moveMarkerStart ||
            (compare(spliceStart, marker.end) < 0) ||
              (!marker.exclusive && compare(spliceOldEnd, marker.end) === 0)

        if (moveMarkerStart) {
          if (compare(spliceOldEnd, marker.start) <= 0) { // splice precedes marker start
            marker.start = traverse(spliceNewEnd, traversalDistance(marker.start, spliceOldEnd))
          } else { // splice surrounds marker start
            marker.start = spliceNewEnd
          }
        }

        if (moveMarkerEnd) {
          if (compare(spliceOldEnd, marker.end) <= 0) { // splice precedes marker end
            marker.end = traverse(spliceNewEnd, traversalDistance(marker.end, spliceOldEnd))
          } else { // splice surrounds marker end
            marker.end = spliceNewEnd
          }
        }
      }

      return invalidated
    }

    function checkInvalidatedSets (actualSets, expectedSets) {
      for (let strategy in expectedSets) {
        let expectedSet = expectedSets[strategy]
        let actualSet = actualSets[strategy]

        assert.equal(actualSet.size, expectedSet.size, `Strategy: ${strategy}. Expected: [${Array.from(expectedSet)}], Actual: [${Array.from(actualSet)}]. Seed ${seed}.`)
        for (let markerId of expectedSet) {
          assert(actualSet.has(markerId), `Expected marker ${markerId} to be invalidated via ${strategy} strategy. Seed ${seed}.`)
        }
      }
    }
  })

  it('can compare marker ranges', function () {
    let index = new MarkerIndex()
    index.insert(1, {row: 1, column: 2}, {row: 3, column: 4})
    index.insert(2, {row: 1, column: 2}, {row: 3, column: 4})
    index.insert(3, {row: 2, column: 2}, {row: 3, column: 4})
    index.insert(4, {row: 1, column: 2}, {row: 3, column: 5})

    assert.equal(index.compare(1, 2), 0)
    assert.equal(index.compare(2, 1), 0)
    assert.equal(index.compare(1, 3), -1)
    assert.equal(index.compare(3, 1), 1)
    assert.equal(index.compare(1, 4), 1)
    assert.equal(index.compare(4, 1), -1)
  })

  it('handles range queries involving Infinity', () => {
    let index = new MarkerIndex()
    index.insert(1, {row: 10, column: 10}, {row: 20, column: 20})
    let result = index.findEndingIn({row: 0, column: 0}, {row: Infinity, column: Infinity})
    assert(result.has(1))
  })
})
