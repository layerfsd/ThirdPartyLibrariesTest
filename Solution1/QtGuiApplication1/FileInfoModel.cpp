#include "FileInfoModel.h"

FileInfoModel::FileInfoModel(QObject *parent) : QAbstractItemModel(parent)
{
    m_totalRowCount = 0;
    m_totalColumnCount = 0;
    m_itemList.clear();
    m_columnHeaderItems.clear();
}

FileInfoModel::~FileInfoModel()
{
    clearData();
}

int FileInfoModel::rowCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
    return m_totalRowCount;
}

int FileInfoModel::columnCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
    return m_totalColumnCount;
}

QVariant FileInfoModel::data(const QModelIndex &index, int role /*= Qt::DisplayRole*/) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    FileInfoItem* item = itemFromIndex(index);

    if (m_valuesMap.find(item) == m_valuesMap.end())
    {
        return QVariant();
    }

    if (m_valuesMap[item].find(role) == m_valuesMap[item].end())
    {
        return QVariant();
    }

    return m_valuesMap[item][role];
}

QModelIndex FileInfoModel::index(int row, int column, const QModelIndex &parent /*= QModelIndex()*/) const
{
    FileInfoItem *item = NULL;
    int i = childIndex(row, column);
    if (-1 != i)
    {
        item = m_itemList.at(i);
    }

    return hasIndex(row, column, parent) ? createIndex(row, column, item) : QModelIndex();
}

QModelIndex FileInfoModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

void FileInfoModel::setItemData(int row, int column, const QVariant &value, int role /*= Qt::EditRole*/)
{
    FileInfoItem* item_ = item(row, column);
    m_valuesMap[item_][role] = value;
}

void FileInfoModel::setItemIcon(int row, int column, const QIcon &icon)
{
    FileInfoItem* item_ = item(row, column);
    m_valuesMap[item_][Qt::DecorationRole] = QVariant(icon);
}

void FileInfoModel::setItem(int row, int column, const QString& text, const QIcon &icon /*= QIcon()*/)
{
    if (m_totalRowCount <= row)
    {
        beginInsertRows(QModelIndex(), m_totalRowCount > 0 ? m_totalRowCount - 1 : 0, m_totalRowCount);
        m_totalRowCount += 1;
        endInsertRows();
    }

    if (m_totalColumnCount <= column)
    {
        beginInsertColumns(QModelIndex(), m_totalColumnCount > 0 ? m_totalColumnCount - 1 : 0, m_totalColumnCount);
        m_totalColumnCount += 1;
        endInsertColumns();
    }

    int i = childIndex(row, column);
    FileInfoItem* item = new FileInfoItem();
    if (item)
    {
        item->text = text;
        item->row = row;
        item->column = column;
        m_itemList.insert(i, item);
        m_valuesMap[item][Qt::DecorationRole] = QVariant(icon);
        m_valuesMap[item][Qt::DisplayRole] = QVariant(text);
        m_valuesMap[item][Qt::UserRole] = QVariant((unsigned int)item);

        QModelIndex index = indexFromItem(item);
        emit dataChanged(index, index);
    }
}

FileInfoItem* FileInfoModel::item(int row, int column)
{
    int i = childIndex(row, column);
    if (-1 == i)
    {
        return NULL;
    }

    return m_itemList.at(i);
}

FileInfoItem* FileInfoModel::itemFromIndex(const QModelIndex &index) const
{
    int i = childIndex(index.row(), index.column());
    if (-1 == i)
    {
        return NULL;
    }

    return m_itemList.at(i);
}

QModelIndex FileInfoModel::indexFromItem(const FileInfoItem *item) const
{
    if (!item)
    {
        return QModelIndex();
    }

    return index(item->row, item->column);
}

void FileInfoModel::clear()
{
    beginResetModel();
    clearData();
    endResetModel();
}

void FileInfoModel::setHorizontalHeaderLabels(const QStringList &labels)
{
    m_columnHeaderItems.clear();
    m_columnHeaderItems = labels;
    m_totalColumnCount = labels.size();
}

QVariant FileInfoModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
{
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    if (orientation == Qt::Horizontal)
    {
        return m_columnHeaderItems.at(section);
    }
    else
    {
        return section + 1;
    }
}

void FileInfoModel::clearData()
{
    for (QList<FileInfoItem*>::iterator iter = m_itemList.begin();
        iter != m_itemList.end();
        ++iter)
    {
        if (*iter)
        {
            delete (*iter);
            (*iter) = NULL;
        }
    }

    m_itemList.clear();
    m_valuesMap.clear();
    m_totalRowCount = 0;
}

int FileInfoModel::childIndex(int row, int column) const
{
    if ((row < 0) || (column < 0)
        || (row >= m_totalRowCount) || (column >= m_totalColumnCount))
    {
        return -1;
    }

    return (row * m_totalColumnCount) + column;
}
