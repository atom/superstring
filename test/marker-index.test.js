import Random from 'random-seed'
import MarkerIndex from '../src/marker-index'

describe('MarkerIndex', () => {
  it('maintains correct marker positions during randomized insertions and mutations', () => {
    let seed = Date.now()
    let assertionMessage = `Random Seed: ${seed}`
    let random = new Random(seed)
    let markerIndex = new MarkerIndex(seed)
    let markers = []
    let idCounter = 1

    for (let j = 0; j < 20; j++) {
      performInsert()
      // let n = random(10)
      // if (n >= 4) { // 60% insert
      // } else if (n >= 2) { // 20% splice
      //   performSplice()
      // } else if (markers.length > 0) { // 20% delete
      //   performDelete()
      // }
      //
    }
    // document.write(markerIndex.toHTML())
    verifyRanges()
    testIntersectionQueries()

    function verifyRanges () {
      for (let marker of markers) {
        let range = markerIndex.getRange(marker.id)
        assert.equal(range[0], marker.start, assertionMessage)
        assert.equal(range[1], marker.end, assertionMessage)
      }
    }

    function testIntersectionQueries () {
      for (let i = 0; i < 10; i++) {
        let [start, end] = getRange()

        let expectedIds = new Set()
        for (let marker of markers) {
          if (marker.start <= end && start <= marker.end) {
            expectedIds.add(marker.id)
          }
        }

        let actualIds = new Set()
        markerIndex.findIntersecting(start, end, actualIds)

        assert.equal(actualIds.size, expectedIds.size, assertionMessage)
        for (let id of expectedIds) {
          assert(actualIds.has(id), `Expected ${id} to be in set. ` + assertionMessage)
        }
      }
    }

    function performInsert () {
      let id = String(idCounter++)
      let [start, end] = getRange()
      let exclusive = !!random(2)
      markerIndex.insert(id, start, end)
      // if (exclusive) markerIndex.setExclusive(id, true)
      markers.push({id, start, end, exclusive})
    }

    function performSplice () {
      let [start, oldExtent, newExtent] = getSplice()
      markerIndex.splice(start, oldExtent, newExtent)
      applySplice(markers, start, oldExtent, newExtent)
    }

    function performDelete () {
      let [{id}] = markers.splice(random(markers.length - 1), 1)
      markerIndex.delete(id)
    }

    function getRange () {
      let start = random(100)
      let end = start
      while (random(2)) {
        end += random.intBetween(-10, 10)
      }

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
        if (spliceStart < marker.start || marker.exclusive && spliceOldEnd === marker.start) {
          if (spliceOldEnd <= marker.start) { // splice precedes marker start
            marker.start += spliceDelta
          } else { // splice surrounds marker start
            marker.start = spliceNewEnd
          }
        }

        if (spliceStart < marker.end || !marker.exclusive && spliceOldEnd === marker.end) {
          if (spliceOldEnd <= marker.end) { // splice precedes marker end
            marker.end += spliceDelta
          } else { // splice surrounds marker end
            marker.end = spliceNewEnd
          }
        }
      }
    }
  })
})
