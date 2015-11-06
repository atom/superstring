#ifndef POINT_H_
#define POINT_H_

struct Point {
  unsigned row;
  unsigned column;

  static Point Max();

  Point();
  Point(unsigned row, unsigned column);

  int Compare(const Point &other) const;
  bool IsZero() const;
  Point Traverse(const Point &other) const;
  Point Traversal(const Point &other) const;

  bool operator==(const Point &other) const;
  bool operator<(const Point &other) const;
  bool operator<=(const Point &other) const;
  bool operator>(const Point &other) const;
  bool operator>=(const Point &other) const;
};

#endif // POINT_H_
