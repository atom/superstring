import NativeMarkerIndex from '../../build/Release/marker_index'

export default class MarkerIndex {
  constructor (seed = Date.now()) {
    this.nativeIndex = new NativeMarkerIndex(seed)
  }

  insert (id, start, end) {
    this.nativeIndex.insert(id, start.row, start.column, end.row, end.column)
  }
}
