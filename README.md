# marker-index

This module is used by Atom to efficiently track logical locations in a text buffer as the contents of the buffer are changed.

## Example

```js
import MarkerIndex from 'marker-index'

let index = new MarkerIndex

// associate a marker id with two ordered start and end points
index.insert(1, {row: 2, column: 5}, {row: 4, column: 10})
// splice represents a change to the text file
// you pass it a starting point, then points representing the old and new extent
index.splice({row: 3, column: 5}, {row: 0, column: 0}, {row: 1, column: 0})
// the marker's end point was updated by the splice
index.getEnd(1) // => {row: 5, column: 10}
```

## `MarkerIndex` API

### `insert (id, start, end)`

Associates the given non-negative integer with a range represented by two `{row: number, column: number}` objects.

### `splice (start, oldExtent, newExtent)`

Update the locations of all markers based on the description of a change to the text. The range of the replaced text is described by *traversing* from `start` by `oldExtent`. The range of the new text is described by *traversing* from `start` to `newExtent`.

*Traversal* means that beginning with the `start` location, we arrive at a new location by performing X line feeds and carriage returns and then walk forward Y columns, where X is the `row` of the given traversal extent and Y is its `column`. So basically `start`, `oldExtent`, and `newExtent` describe two ranges in the file, basically the spatial before and after effects of a change.

This method returns an object that describes what markers were *invalidated* by the change based on various invalidation strategies. If a marker is in a set for a given strategy, it was invalidated according to that strategy. The strategies are as follows:

* `touch` Contains markers that the change touched in any way.
* `inside` Contains markers that the change touched, but not markers with endpoints immediately adjacent to the change.
* `overlap` Contains markers that had one or both of their endpoints surrounded by the change.s
* `surround` Contains markers that had both endpoints surrounded by the change.

### `setExclusive (markerId, boolean)`

*Exclusive* markers have a different behavior with respect to *insertions*, which are splice operations with an `oldExtent` of `{row: 0, column: 0}`. Normally, insertions are assumed to occur inside markers. Exclusive markers will prefer to interpret changes as being outside their boundaries. That is, an insertion at a markers start point will push it over, but an insertion at a marker's endpoint will not change its location.

### `isExclusive (markerId)`

Returns whether the given marker id has been set to behave exclusively via `setExclusive`.

### `delete (markerId)`

Removes the specified marker from the index.

### `getRange (markerId)`

Returns the range for the given marker id, in the form of an object with `start` and `end` points.

### `getStart (markerId)`

Returns a `{row: number, column: number}` object representing the start of the specified marker.

### `getEnd (markerId)`

Returns a `{row: number, column: number}` object representing the end of the specified marker.

### `dump ()`

Returns the current location of every marker in the index, represented as an object mapping marker ids to range objects. For example:

```js
{
  '1': {start: {row: 2, column: 5}, end: {row: 5, column: 10}},
  '2': {start: {row: 4, column: 10}, end: {row: 6, column: 3}}
}
```

### `findIntersecting (start, end = start)`

Returns a set with the ids of all markers intersecting the specified point range.

### `findContaining (start, end = start)`

Returns a set with the ids of all markers intersecting the specified point range.

### `findContainedIn (start, end)`

Returns a set with the ids of all markers contained in the specified point range.

### `findStartingIn (start, end)`

Returns a set with the ids of all markers starting in the specified point range.

### `findEndingIn (start, end)`

Returns a set with the ids of all markers ending in the specified point range.

### `findStartingAt (position)`

Returns a set with the ids of all markers starting at the specified point.

### `findEndingAt (position)`

Returns a set with the ids of all markers ending at the specified point.
