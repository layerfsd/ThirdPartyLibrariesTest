#ifndef FILETYPEMODEL_H
#define FILETYPEMODEL_H

#include <QAbstractItemModel>
#include <QIcon>
#include <QList>


class ObjectItem;
class FileTypeModel : public QAbstractItemModel
{
    friend class ObjectItem;

public:
    FileTypeModel(QObject *parent = 0);
    ~FileTypeModel();

public:
    int	rowCount(const QModelIndex &parent = QModelIndex()) const;
    int	columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QModelIndex index(const ObjectItem *item, int column) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    
    void insertItem(int row, ObjectItem *item);
    void addItem(ObjectItem *item);
    ObjectItem* item(int row) const;
    ObjectItem* item(const QModelIndex &index) const;
    ObjectItem* findItem(const QString &text) const;
    void clear();
    void setHorizontalHeaderLabels(const QStringList &labels);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
        int role = Qt::EditRole);

private:
    void clearData();

protected:
    void beginInsertItems(ObjectItem *parent, int row, int count);
    void endInsertItems();
    void beginRemoveItems(ObjectItem *parent, int row, int count);
    void endRemoveItems();
    void itemChanged(ObjectItem *item);

private:
    QList<ObjectItem*> _headerItems;
    ObjectItem *_rootItem;
};

#endif // FILETYPEMODEL_H