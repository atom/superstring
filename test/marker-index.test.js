import Random from 'random-seed'
import MarkerIndex from '../src/marker-index'

describe('MarkerIndex', () => {
  it('maintains correct marker positions during randomized insertions and mutations', function () {
    this.timeout(Infinity)

    //  uncomment for debug output in electron (`npm run tdd`)
    function write (f) {
      // document.write(f())
    }

    for (let i = 0; i < 1000; i++) {
      let seed = Date.now()
      let seedMessage = `Random Seed: ${seed}`
      let random = new Random(seed)
      let markerIndex = new MarkerIndex(seed)
      let markers = []
      let idCounter = 65

      for (let j = 0; j < 50; j++) {
        let n = random(10)
        if (n >= 4) { // 60% insert
          performInsert()
        } else if (n >= 2) { // 20% splice
          performSplice()
        } else if (markers.length > 0) {
          performDelete()
        }
        write(() => markerIndex.toHTML())
        write(() => '<hr>')
        verifyHighestPossiblePaths()
        verifyContinuousPaths()
      }

      verifyRanges()
      testFindIntersecting()
      testFindContaining()
      testFindContainedIn()
      testFindStartingIn()
      testFindEndingIn()
      testFindStartingAt()
      testFindEndingAt()

      function verifyRanges () {
        for (let marker of markers) {
          let range = markerIndex.getRange(marker.id)
          assert.equal(range[0], marker.start, `Marker ${marker.id} start. ` + seedMessage)
          assert.equal(range[1], marker.end, `Marker ${marker.id} end. ` + seedMessage)
        }
      }

      function verifyHighestPossiblePaths (node, alreadySeen) {
        if (!node) {
          if (markerIndex.root) verifyHighestPossiblePaths(markerIndex.root, new Set())
          return
        }

        for (let markerId of node.leftMarkerIds) {
          assert(!alreadySeen.has(markerId), 'Redundant paths. ' + seedMessage)
        }
        for (let markerId of node.rightMarkerIds) {
          assert(!alreadySeen.has(markerId), 'Redundant paths. ' + seedMessage)
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
        let startedMarkers = new Set
        let endedMarkers = new Set

        let iterator = markerIndex.iterator
        iterator.reset()
        while (iterator.node && iterator.node.left) iterator.descendLeft()

        let node = iterator.node
        while(node) {
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
            assert(!endedMarkers.has(markerId), `Marker ${markerId} in right markers, but already ended. ` + seedMessage)
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
            if (marker.start <= end && start <= marker.end) {
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
            if (marker.start <= start && end <= marker.end) {
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
            if (start <= marker.start && marker.end <= end) {
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
            if (start <= marker.start && marker.start <= end) {
              expectedIds.add(marker.id)
            }
          }

          let actualIds = markerIndex.findStartingIn(start, end)
          assert.equal(actualIds.size, expectedIds.size, seedMessage)
          for (let id of expectedIds) {
            assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
          }
        }
      }

      function testFindEndingIn () {
        for (let i = 0; i < 10; i++) {
          let [start, end] = getRange()

          let expectedIds = new Set()
          for (let marker of markers) {
            if (start <= marker.end && marker.end <= end) {
              expectedIds.add(marker.id)
            }
          }

          let actualIds = markerIndex.findEndingIn(start, end)
          assert.equal(actualIds.size, expectedIds.size, seedMessage)
          for (let id of expectedIds) {
            assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
          }
        }
      }

      function testFindStartingAt () {
        for (let i = 0; i < 10; i++) {
          let offset = random(100)

          let expectedIds = new Set()
          for (let marker of markers) {
            if (marker.start === offset) {
              expectedIds.add(marker.id)
            }
          }

          let actualIds = markerIndex.findStartingAt(offset)
          assert.equal(actualIds.size, expectedIds.size, seedMessage)
          for (let id of expectedIds) {
            assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
          }
        }
      }

      function testFindEndingAt () {
        for (let i = 0; i < 10; i++) {
          let offset = random(100)

          let expectedIds = new Set()
          for (let marker of markers) {
            if (marker.end === offset) {
              expectedIds.add(marker.id)
            }
          }

          let actualIds = markerIndex.findEndingAt(offset)
          assert.equal(actualIds.size, expectedIds.size, seedMessage)
          for (let id of expectedIds) {
            assert(actualIds.has(id), `Expected ${id} to be in set. ` + seedMessage)
          }
        }
      }

      function performInsert () {
        let id = String.fromCharCode(idCounter++)
        let [start, end] = getRange()
        let exclusive = !!random(2)
        write(() => `insert ${id}, ${start}, ${end}, exclusive: ${exclusive}`)
        markerIndex.insert(id, start, end)
        if (exclusive) markerIndex.setExclusive(id, true)
        markers.push({id, start, end, exclusive})
      }

      function performSplice () {
        let [start, oldExtent, newExtent] = getSplice()
        write(() => `splice ${start}, ${oldExtent}, ${newExtent}`)
        markerIndex.splice(start, oldExtent, newExtent)
        applySplice(markers, start, oldExtent, newExtent)
      }

      function performDelete () {
        let [{id}] = markers.splice(random(markers.length - 1), 1)
        write(() => `delete ${id}`)
        markerIndex.delete(id)
      }

      function getRange () {
        let start = random(100)
        let end = start
        while (random(3) > 0) {
          end += random.intBetween(-10, 10)
        }
        end = Math.max(end, 0)

        if (start <= end) {
          return [start, end]
        } else {
          return [end, start]
        }
      }

      function getSplice () {
        let [start, oldEnd] = getRange()
        let oldExtent = oldEnd - start
        let newExtent = 0
        while (random(2)) {
          newExtent += random(10)
        }
        return [start, oldExtent, newExtent]
      }

      function applySplice (markers, spliceStart, oldExtent, newExtent) {
        let spliceOldEnd = spliceStart + oldExtent
        let spliceNewEnd = spliceStart + newExtent
        let spliceDelta = newExtent - oldExtent

        for (let marker of markers) {
          let isEmpty = marker.start == marker.end

          if (spliceStart < marker.start || marker.exclusive && spliceOldEnd === marker.start) {
            if (spliceOldEnd <= marker.start) { // splice precedes marker start
              marker.start += spliceDelta
            } else { // splice surrounds marker start
              marker.start = spliceNewEnd
            }
          }

          if (spliceStart < marker.end || (!marker.exclusive || isEmpty) && spliceOldEnd === marker.end) {
            if (spliceOldEnd <= marker.end) { // splice precedes marker end
              marker.end += spliceDelta
            } else { // splice surrounds marker end
              marker.end = spliceNewEnd
            }
          }
        }
      }
    }
  })
})
