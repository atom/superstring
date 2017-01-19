# Superstring
[![macOS Build Status](https://travis-ci.org/atom/superstring.svg?branch=master)](https://travis-ci.org/atom/superstring)
[![Windows Build Status](https://ci.appveyor.com/api/projects/status/n5pack4yk7w80fso/branch/master?svg=true)](https://ci.appveyor.com/project/Atom/superstring/branch/master)
[![Dependency Status](https://david-dm.org/atom/superstring.svg)](https://david-dm.org/atom/superstring)



Native library at the core of Atom's text editor.

## Components:

### Patch

This data structure represents a transformation from input to output text, and it's useful for combining changes that occur at different points in time and space.

Example:
```js
const patch = new Patch

// At column 5, replace the string 'abc' with '1234':
patch.splice({row: 0, column: 5}, {row: 0, column: 3}, {row: 0, column: 4}, 'abc', '1234')

// Then at column 7, replace 3 characters with 4 characters:
patch.splice({row: 0, column: 7}, {row: 0, column: 3}, {row: 0, column: 4}, '34d', '5678')

// Retrieve the consolidated changes:
assert.deepEqual(patch.getHunks(), [
  {
    oldStart: {row: 0, column: 5},
    oldEnd: {row: 0, column: 9},
    oldText: 'abcd',
    newStart: {row: 0, column: 5},
    newEnd: {row: 0, column: 11},
    newText: '125678'
  }
])
```

### BufferOffsetIndex

This data allows conversion between 2-dimensional row/column positions and 1-dimensional string indices.

Example:

```js
const bufferIndex = new BufferOffsetIndex

// Specify that there are three lines with lengths 1, 3, and 5:
bufferIndex.splice(0, 0, [1, 3, 5]) // a|bcd|efghi

// Convert a 1-dimensional string index to a row/column position:
assert.deepEqual(bufferIndex.positionForCharacterIndex(0), {row: 0, column: 0})

// Convert a row/column position to a 1-dimensional string index:
assert.equal(bufferIndex.characterIndexForPosition({row: 0, column: 0}), 0)
```

### MarkerIndex

This data structure is used to track logical locations in a text buffer as the contents of the buffer are changed.

Example:

```js
const index = new MarkerIndex

// Associate a marker id with two ordered start and end points
index.insert(1, {row: 2, column: 5}, {row: 4, column: 10})

// Splice represents a change to the text file
// you pass it a starting point, then points representing the old and new extent
index.splice({row: 3, column: 5}, {row: 0, column: 0}, {row: 1, column: 0})

// The marker's end point was updated by the splice
assert.deepEqual(index.getEnd(1), {row: 5, column: 10})
```

#### API

##### `insert (id, start, end)`

Associates the given non-negative integer with a range represented by two `{row: number, column: number}` objects.

##### `splice (start, oldExtent, newExtent)`

Update the locations of all markers based on the description of a change to the text. The range of the replaced text is described by *traversing* from `start` by `oldExtent`. The range of the new text is described by *traversing* from `start` to `newExtent`.

*Traversal* means that beginning with the `start` location, we arrive at a new location by performing X line feeds and carriage returns and then walk forward Y columns, where X is the `row` of the given traversal extent and Y is its `column`. So basically `start`, `oldExtent`, and `newExtent` describe two ranges in the file, basically the spatial before and after effects of a change.

This method returns an object that describes what markers were *invalidated* by the change based on various invalidation strategies. If a marker is in a set for a given strategy, it was invalidated according to that strategy. The strategies are as follows:

* `touch` Contains markers that the change touched in any way.
* `inside` Contains markers that the change touched, but not markers with endpoints immediately adjacent to the change.
* `overlap` Contains markers that had one or both of their endpoints surrounded by the change.
* `surround` Contains markers that had both endpoints surrounded by the change.

##### `setExclusive (markerId, boolean)`

This method allows to control the behavior of a marker when splices start and/or end at the marker's endpoints.

By default, we consider markers to be *inclusive*: that is, splices exactly at the beginning of the marked range will be considered to begin inside the marker (meaning that the marker's start position **will not** move), and splices exactly at the end of the marked range will be considered to end inside the marker (meaning that the marker's end position **will** move).

*Exclusive* markers, on the other hand, exhibit a slightly different behavior: in fact, splices exactly at the beginning of the marked range will be considered to begin outside the marker (meaning that the marker's start position **will** move), and splices exactly at the end of the marked range will be considered to end outside the marker (meaning that the marker's end position **will not** move).

Please note that, independently of whether a marker is inclusive or exclusive, its end **will always** be moved when its start gets moved as a result of a splice.

##### `isExclusive (markerId)`

Returns whether the given marker id has been set to behave exclusively via `setExclusive`.

##### `delete (markerId)`

Removes the specified marker from the index.

##### `getRange (markerId)`

Returns the range for the given marker id, in the form of an object with `start` and `end` points.

##### `getStart (markerId)`

Returns a `{row: number, column: number}` object representing the start of the specified marker.

##### `getEnd (markerId)`

Returns a `{row: number, column: number}` object representing the end of the specified marker.

##### `dump ()`

Returns the current location of every marker in the index, represented as an object mapping marker ids to range objects. For example:

```js
{
  '1': {start: {row: 2, column: 5}, end: {row: 5, column: 10}},
  '2': {start: {row: 4, column: 10}, end: {row: 6, column: 3}}
}
```

##### `findIntersecting (start, end = start)`

Returns a set with the ids of all markers intersecting the specified point range.

##### `findContaining (start, end = start)`

Returns a set with the ids of all markers intersecting the specified point range.

##### `findContainedIn (start, end)`

Returns a set with the ids of all markers contained in the specified point range.

##### `findStartingIn (start, end)`

Returns a set with the ids of all markers starting in the specified point range.

##### `findEndingIn (start, end)`

Returns a set with the ids of all markers ending in the specified point range.

##### `findStartingAt (position)`

Returns a set with the ids of all markers starting at the specified point.

##### `findEndingAt (position)`

Returns a set with the ids of all markers ending at the specified point.
