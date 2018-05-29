#include "ObjectItem.h"
#include "FileTypeModel.h"



ObjectItem::ObjectItem(FileTypeModel *model)
: _model(model), _parent(0)
{
}

ObjectItem::~ObjectItem()
{
    qDeleteAll(_children.begin(), _children.end());
    _children.clear();
    _values.clear();
}

ObjectItem * ObjectItem::parent() const
{
    return _parent;
}

ObjectItem * ObjectItem::child(int row) const
{
    if (row < 0 || row >= _children.size())
        return 0;

    return _children.at(row);
}

ObjectItem* ObjectItem::findChild(const QString &text) const
{
    for (QList<ObjectItem*>::const_iterator iter = _children.begin();
        iter != _children.end();
        ++iter)
    {
        if (*iter)
        {
            QString compareText = (*iter)->data(Qt::UserRole + 1).toString();
            if (0 == text.compare(compareText, Qt::CaseSensitive))
            {
                return *iter;
            }
        }
    }

    return 0;
}

int ObjectItem::childCount() const
{
    return _children.size();
}

int ObjectItem::indexOfChild(const ObjectItem *child) const
{
    if (!child)
        return -1;

    int index = 0;
    for (QList<ObjectItem*>::const_iterator iter = _children.begin();
        iter != _children.end();
        ++iter, ++index)
    {
        if (*iter && *iter == child)
        {
            return index;
        }
    }

    return -1;
}

void ObjectItem::addChild(ObjectItem *child)
{
    insertChild(_children.size(), child);
}

void ObjectItem::insertChild(int row, ObjectItem *child)
{
    if (row < 0 ||
        row > _children.count() ||
        0 == child ||
        child->_parent != 0)
        return;

   
    if (_model)
    {
        _model->beginInsertItems(this, row, 1);
    }

    child->_parent = this;
    _children.insert(row, child);

    if (_model)
        _model->endInsertItems();
}

void ObjectItem::removeChild(ObjectItem *child)
{
    int row = _children.indexOf(child);
    if (-1 == row)
        return;

    if (_model)
    {
        _model->beginRemoveItems(this, row, 1);
    }

    _children.removeAt(row);

    if (_model)
        _model->endRemoveItems();
}

void ObjectItem::addChildren(const QList<ObjectItem*> &children)
{
    insertChildren(_children.size(), children);
}

void ObjectItem::insertChildren(int row, const QList<ObjectItem*> &children)
{
    if (_model)
    {
        _model->beginInsertItems(this, row, children.size());
    }

    for (int n = 0; n < children.size(); ++n) {
        _children.insert(row + n, children.at(n));
    }

    if (_model)
        _model->endInsertItems();
}

QVariant ObjectItem::data(int role) const
{
    return _values.value(role, QVariant());
}

void ObjectItem::setData(const QVariant &value, int role)
{
    _values[role] = value;

    if (_model)
        _model->itemChanged(this);
}

QString ObjectItem::text() const
{
    return _values.value(Qt::DisplayRole).value<QString>();
}

void ObjectItem::setText(const QString &text)
{
    setData(text, Qt::DisplayRole);
}

QIcon ObjectItem::icon() const
{
    return _values.value(Qt::DecorationRole).value<QIcon>();
}

void ObjectItem::setIcon(const QIcon &icon)
{
    setData(icon, Qt::DecorationRole);
}
