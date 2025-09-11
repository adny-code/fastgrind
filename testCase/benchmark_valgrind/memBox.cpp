#include "memBox.h"

memBox &memBox::operator=(const memBox &other)
{
    if (this != &other)
    {
        _leftBottom = other._leftBottom;
        _rightTop = other._rightTop;
    }
    return *this;
}

bool memBox::operator<(const memBox &other) const
{
    if (_leftBottom != other._leftBottom)
        return _leftBottom < other._leftBottom;
    return _rightTop < other._rightTop;
}

bool memBox::overlap(const memBox &other, bool proper) const
{
    if (proper)
    {
        if (_leftBottom._x >= other._rightTop._x || _rightTop._x <= other._leftBottom._x ||
            _leftBottom._y >= other._rightTop._y || _rightTop._y <= other._leftBottom._y)
        {
            return false;
        }
        return true;
    }
    else
    {
        if (_leftBottom._x > other._rightTop._x || _rightTop._x < other._leftBottom._x ||
            _leftBottom._y > other._rightTop._y || _rightTop._y < other._leftBottom._y)
        {
            return false;
        }
        return true;
    }
}

memBox memBox::operator|=(const memBox &other)
{
    if (!other.valid())
        return *this;

    if (!valid())
    {
        _leftBottom = other._leftBottom;
        _rightTop = other._rightTop;
        return *this;
    }

    if (other._leftBottom._x < _leftBottom._x)
        _leftBottom._x = other._leftBottom._x;
    if (other._leftBottom._y < _leftBottom._y)
        _leftBottom._y = other._leftBottom._y;

    if (other._rightTop._x > _rightTop._x)
        _rightTop._x = other._rightTop._x;
    if (other._rightTop._y > _rightTop._y)
        _rightTop._y = other._rightTop._y;

    return *this;
}