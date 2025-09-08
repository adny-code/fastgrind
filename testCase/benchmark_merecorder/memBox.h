#include <deque>

struct memPoint
{
    int _x;
    int _y;

    memPoint(int x, int y) : _x(x), _y(y)
    {
    }
    memPoint() : _x(0), _y(0)
    {
    }

    memPoint(const memPoint &other) : _x(other._x), _y(other._y)
    {
    }
    memPoint &operator=(const memPoint &other)
    {
        if (this != &other)
        {
            _x = other._x;
            _y = other._y;
        }
        return *this;
    }

    bool operator==(const memPoint &other) const
    {
        return _x == other._x && _y == other._y;
    }
    bool operator!=(const memPoint &other) const
    {
        return !(*this == other);
    }

    bool operator<(const memPoint &other) const
    {
        if (_x != other._x)
            return _x < other._x;
        return _y < other._y;
    }
    bool operator<=(const memPoint &other) const
    {
        return *this < other || *this == other;
    }

    bool operator+=(const memPoint &other)
    {
        _x += other._x;
        _y += other._y;
        return true;
    }

    bool operator-=(const memPoint &other)
    {
        _x -= other._x;
        _y -= other._y;
        return true;
    }

    bool x() const
    {
        return _x;
    }

    bool y() const
    {
        return _y;
    }
};

class memBox
{
  public:
    memBox() : _leftBottom(0, 0), _rightTop(0, 0)
    {
    }

    memBox(const memPoint &leftBottom, const memPoint &rightTop) : _leftBottom(leftBottom), _rightTop(rightTop){};

    memBox(int left, int bottom, int right, int top) : _leftBottom(left, bottom), _rightTop(right, top)
    {
    }

    memBox(const memBox &other) : _leftBottom(other._leftBottom), _rightTop(other._rightTop)
    {
    }

    memBox &operator=(const memBox &other);

    bool operator==(const memBox &other) const
    {
        return _leftBottom == other._leftBottom && _rightTop == other._rightTop;
    }

    bool operator<(const memBox &other) const;

    bool operator!=(const memBox &other) const
    {
        return !(*this == other);
    }

    memBox operator|=(const memBox &other);

    bool overlap(const memBox &other, bool proper = true) const;

    int width() const
    {
        return _rightTop._x - _leftBottom._x;
    }

    int height() const
    {
        return _rightTop._y - _leftBottom._y;
    }

    bool valid() const
    {
        return _leftBottom._x < _rightTop._x && _leftBottom._y < _rightTop._y;
    }

    memPoint leftBottom() const
    {
        return _leftBottom;
    }

    memPoint rightTop() const
    {
        return _rightTop;
    }

  protected:
    memPoint _leftBottom;
    memPoint _rightTop;
};