#ifndef OBJECTITEM_H
#define OBJECTITEM_H

#include <QAbstractItemModel>
#include <QVariant>
#include <QString>
#include <QList>
#include <QIcon>


class FileTypeModel;
class ObjectItem
{
public:
    ObjectItem(FileTypeModel *model);
    ~ObjectItem();

    ObjectItem *parent() const;
    ObjectItem *child(int row) const;
    ObjectItem* findChild(const QString &text) const;
    int childCount() const;
    int indexOfChild(const ObjectItem *child) const;
    void addChild(ObjectItem *child);
    void insertChild(int row, ObjectItem *child);
    void removeChild(ObjectItem *child);
    void addChildren(const QList<ObjectItem*> &children);
    void insertChildren(int row, const QList<ObjectItem*> &children);

    QVariant data(int role) const;
    void setData(const QVariant &value, int role);

    QString text() const;
    void setText(const QString &text);

    QIcon icon() const;
    void setIcon(const QIcon &icon);

private:
    QList<ObjectItem*> _children;
    ObjectItem *_parent;
    QMap<int, QVariant> _values;
    FileTypeModel *_model;
};


#endif // OBJECTITEM_H