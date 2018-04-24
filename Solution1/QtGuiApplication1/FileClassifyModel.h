#pragma once
#include <QAbstractItemModel>
#include <QIcon>
#include <QList>


struct FileItem
{
    QString text;
    int row;
    int column;
};


class FileClassifyModel : public QAbstractItemModel
{
public:
    FileClassifyModel(QObject *parent = 0);
    ~FileClassifyModel();

public:
    int	rowCount(const QModelIndex &parent = QModelIndex()) const;
    int	columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

    void setItemData(int row, int column, const QVariant &value, int role = Qt::EditRole);
    void setItemIcon(int row, int column, const QIcon &icon);
    void setItem(int row, int column, const QString& text, const QIcon &icon = QIcon());
    FileItem* item(int row, int column);
    FileItem* itemFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromItem(const FileItem *item) const;
    void clear();
    void setHorizontalHeaderLabels(const QStringList &labels);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

private:
    void clearData();
    int childIndex(int row, int column) const;

private:
    int m_totalRowCount;
    int m_totalColumnCount;
    QList<QString> m_columnHeaderItems;
    QList<FileItem*> m_itemList;
    QMap<FileItem*, QMap<int, QVariant>> m_valuesMap;
};
