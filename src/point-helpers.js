export const ZERO_POINT = Object.freeze({row: 0, column: 0})
export const INFINITY_POINT = Object.freeze({row: Infinity, column: Infinity})

export function compare (a, b) {
  if (a.row === b.row) {
    return compareNumbers(a.column, b.column)
  } else {
    return compareNumbers(a.row, b.row)
  }
}

export function isZero (point) {
  return (point.row === 0 && point.column === 0)
}

export function min (a, b) {
  if (compare(a, b) <= 0) {
    return a
  } else {
    return b
  }
}

export function traverse (start, distance) {
  if (distance.row === 0) {
    return {
      row: start.row,
      column: start.column + distance.column
    }
  } else {
    return {
      row: start.row + distance.row,
      column: distance.column
    }
  }
}

export function traversalDistance (end, start) {
  if (end.row === start.row) {
    return {row: 0, column: end.column - start.column}
  } else {
    return {row: end.row - start.row, column: end.column}
  }
}

export function format (point) {
  return `(${point.row}, ${point.column})`
}

function compareNumbers (a, b) {
  if (a < b) {
    return -1
  } else if (a > b) {
    return 1
  } else {
    return 0
  }
}
