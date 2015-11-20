import NativeMarkerIndex from '../../build/Release/marker_index'

NativeMarkerIndex.prototype.splice = function (start, oldExtent, newExtent) {
  let invalidated = this._splice(start, oldExtent, newExtent);

  invalidated.touch = new Set(invalidated.touch);
  invalidated.inside = new Set(invalidated.inside);
  invalidated.overlap = new Set(invalidated.overlap);
  invalidated.surround = new Set(invalidated.surround);

  return invalidated;
}

NativeMarkerIndex.prototype.getRange = function (id) {
  return [this.getStart(id), this.getEnd(id)]
}

NativeMarkerIndex.prototype.findIntersecting = function (start, end = start) {
  return new Set(this._findIntersecting(start, end))
}

NativeMarkerIndex.prototype.findContaining = function (start, end = start) {
  end = end || start
  return new Set(this._findContaining(start, end))
}

NativeMarkerIndex.prototype.findContainedIn = function (start, end) {
  return new Set(this._findContainedIn(start, end))
}

NativeMarkerIndex.prototype.findStartingIn = function (start, end) {
  return new Set(this._findStartingIn(start, end))
}

NativeMarkerIndex.prototype.findEndingIn = function (start, end) {
  return new Set(this._findEndingIn(start, end))
}

NativeMarkerIndex.prototype.findStartingAt = function (position) {
  return new Set(this._findStartingIn(position, position))
}

NativeMarkerIndex.prototype.findEndingAt = function (position) {
  return new Set(this._findEndingIn(position, position))
}

export default NativeMarkerIndex
