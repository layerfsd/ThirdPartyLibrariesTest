#include "FileTypeModel.h"
#include "ObjectItem.h"


FileTypeModel::FileTypeModel(QObject *parent) : QAbstractItemModel(parent)
{
    _rootItem = new ObjectItem(this);
}

FileTypeModel::~FileTypeModel()
{
    clearData();
}

int FileTypeModel::rowCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
    if (!parent.isValid())
        return _rootItem->childCount();

    ObjectItem *parentItem = item(parent);
    if (parentItem)
        return parentItem->childCount();

    return 0;
}

int FileTypeModel::columnCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
    return _headerItems.size();
}

QVariant FileTypeModel::data(const QModelIndex &index, int role /*= Qt::DisplayRole*/) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    ObjectItem *item_ = item(index);
    if (!item_)
    {
        return QVariant();
    }

    return item_->data(role);
}

QModelIndex FileTypeModel::index(int row, int column, const QModelIndex &parent /*= QModelIndex()*/) const
{
    if (row < 0 || column < 0 || column >= _headerItems.size())
        return QModelIndex();

    ObjectItem *parentItem = parent.isValid() ? item(parent) : _rootItem;
    if (parentItem && row < parentItem->childCount()) {
        ObjectItem *item_ = parentItem->child(row);
        if (item_)
            return createIndex(row, column, item_);
        return QModelIndex();
    }

    return QModelIndex();
}

QModelIndex FileTypeModel::index(const ObjectItem *item, int column) const
{
    if (!item)
        return QModelIndex();

    const ObjectItem *parent = _rootItem;
    if (item != _rootItem)
    {
        parent = item->parent();
    }
    
    if (!parent)
        return QModelIndex();
    
    int row = parent->indexOfChild(item);
    if (-1 == row)
        return QModelIndex();

    ObjectItem *item_ = const_cast<ObjectItem*>(item);
    return createIndex(row, column, item_);
}

QModelIndex FileTypeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();
    
    ObjectItem *item = static_cast<ObjectItem*>(child.internalPointer());
    if (!item)
        return QModelIndex();

    if (!item || _rootItem == item)
        return QModelIndex();

    ObjectItem *parentItem = item->parent();
    return index(parentItem, 0); 
}

void FileTypeModel::insertItem(int row, ObjectItem *item)
{
    if (!_rootItem)
        return;

    _rootItem->insertChild(row, item);
}

void FileTypeModel::addItem(ObjectItem *item)
{
    if (!_rootItem)
        return;

    _rootItem->addChild(item);
}

ObjectItem* FileTypeModel::item(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return static_cast<ObjectItem*>(index.internalPointer());
}

ObjectItem* FileTypeModel::item(int row) const
{
    if (!_rootItem)
        return 0;

    return _rootItem->child(row);
}

ObjectItem* FileTypeModel::findItem(const QString &text) const
{
    if (!_rootItem)
        return 0;

    return _rootItem->findChild(text);
}

void FileTypeModel::clear()
{
    beginResetModel();
    clearData();
    endResetModel();
}

void FileTypeModel::setHorizontalHeaderLabels(const QStringList &labels)
{
    int oldColumnCount = _headerItems.size();
    int newColumnCount = labels.size();
    
    beginRemoveColumns(QModelIndex(), 0, oldColumnCount);
    qDeleteAll(_headerItems.begin(), _headerItems.end());
    _headerItems.clear();
    endRemoveColumns();

    headerDataChanged(Qt::Horizontal, 0, oldColumnCount);

    beginInsertColumns(QModelIndex(), 0, newColumnCount);
    for (int i = 0; i < newColumnCount; ++i)
    {
        ObjectItem *headItem = new ObjectItem(this);
        if (headItem)
        {
            headItem->setText(labels.at(i));
            _headerItems << headItem;
        }
    }
    endInsertColumns();
    headerDataChanged(Qt::Horizontal, 0, newColumnCount);
}

QVariant FileTypeModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
{
    if (orientation == Qt::Horizontal)
    {
        ObjectItem *headItem = _headerItems[section];
        if (headItem)
        {
            return headItem->data(role);
        }
    }
    else if (orientation == Qt::Vertical)
    {
        return section + 1;
    }

    return QVariant();
}

bool FileTypeModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role /*= Qt::EditRole*/)
{
    if (orientation == Qt::Horizontal)
    {
        ObjectItem *headItem = _headerItems[section];
        if (headItem)
        {
            headItem->setData(value, role);
            headerDataChanged(orientation, section, section);
        }
    }

    return true;
}

void FileTypeModel::clearData()
{
    if (_rootItem)
        delete _rootItem;
}

void FileTypeModel::beginInsertItems(ObjectItem *parent, int row, int count)
{
    QModelIndex index_ = index(parent, 0);
    beginInsertRows(index_, row, row + count - 1);
}

void FileTypeModel::endInsertItems()
{
    endInsertRows();
}

void FileTypeModel::beginRemoveItems(ObjectItem *parent, int row, int count)
{
    QModelIndex index_ = index(parent, 0);
    beginRemoveRows(index_, row, row + count - 1);
}

void FileTypeModel::endRemoveItems()
{
    endRemoveRows();
}

void FileTypeModel::itemChanged(ObjectItem *item)
{
    QModelIndex bottomRight, topLeft;
   
    topLeft = index(item, 0);
    bottomRight = topLeft;
    emit dataChanged(topLeft, bottomRight);
}
