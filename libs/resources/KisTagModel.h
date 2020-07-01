/*
 * Copyright (c) 2018 boud <boud@valdyas.org>
 * Copyright (c) 2020 Agata Cacko <cacko.azh@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef KisAllTagsModel_H
#define KisAllTagsModel_H

#include <QObject>
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

#include <KisTag.h>
#include <KoResource.h>

#include "kritaresources_export.h"

class KisAbstractTagModel
{
public:

    virtual ~KisAbstractTagModel() {}

    virtual QModelIndex indexForTag(KisTagSP tag) const = 0;
    virtual KisTagSP tagForIndex(QModelIndex index = QModelIndex()) const = 0;
    virtual KisTagSP addEmptyTag(const QString &tagName, QVector<KoResourceSP> taggedResouces)  = 0;
    virtual bool addTag(const KisTagSP tag, QVector<KoResourceSP> taggedResouces = QVector<KoResourceSP>()) = 0;
    virtual bool setTagActive(const KisTagSP tag) = 0;
    virtual bool setTagInactive(const KisTagSP tag) = 0;
    virtual bool tagResource(const KisTagSP tag, const KoResourceSP resource) = 0;
    virtual bool untagResource(const KisTagSP tag, const KoResourceSP resource) = 0;
    virtual bool renameTag(const KisTagSP tag) = 0;
    virtual bool changeTagActive(const KisTagSP tag, bool active) = 0;
    virtual QVector<KisTagSP> tagsForResource(int resourceId) const = 0;
};


class KRITARESOURCES_EXPORT KisAllTagsModel : public QAbstractTableModel, public KisAbstractTagModel
{
    Q_OBJECT

private:

    friend class KisTagModel;

    KisAllTagsModel(const QString &resourceType, QObject *parent = 0);

public:

    enum Columns {
        Id = 0,
        Url,
        Name,
        Comment,
        ResourceType,
        Active,
        KisTagRole,
    };

    enum Ids {
        All = -2, // so it gets on top in the combobox
        AllUntagged = -1,
    };


    ~KisAllTagsModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

// KisAllTagsModel API

    QModelIndex indexForTag(KisTagSP tag) const override;
    KisTagSP tagForIndex(QModelIndex index = QModelIndex()) const override;

    // TODO: replace ALL occurrences of KoResourceSP here with the resource id's.
    KisTagSP addEmptyTag(const QString &tagName, QVector<KoResourceSP> taggedResouces) override;
    bool addTag(const KisTagSP tag, QVector<KoResourceSP> taggedResouces = QVector<KoResourceSP>()) override;
    bool setTagActive(const KisTagSP tag) override;
    bool setTagInactive(const KisTagSP tag) override;
    bool tagResource(const KisTagSP tag, const KoResourceSP resource) override;
    bool untagResource(const KisTagSP tag, const KoResourceSP resource) override;

    bool renameTag(const KisTagSP tag) override;
    bool changeTagActive(const KisTagSP tag, bool active) override;

    // TODO: Like here
    QVector<KisTagSP> tagsForResource(int resourceId) const override;

private:

    bool tagResourceByUrl(const QString& tagUrl, const int resourceId);
    bool tagResourceById(const int tagId, const int resource);

    KisTagSP tagByUrl(const QString& tagUrl) const;

    bool resetQuery();

    struct Private;
    Private* const d;

};

class KRITARESOURCES_EXPORT KisTagModel : public QSortFilterProxyModel, KisAbstractTagModel
{

    Q_OBJECT

private:

    friend class KisTagModelProvider;
    friend class TestTagModel;

    KisTagModel(const QString &type, QObject *parent = 0);

public:

    ~KisTagModel() override;

    enum TagFilter {
        ShowInactiveTags = 0,
        ShowActiveTags,
        ShowAllTags
    };

    void setTagFilter(TagFilter filter);

    enum StorageFilter {
        ShowInactiveStorages = 0,
        ShowActiveStorages,
        ShowAllStorages
    };

    void setStorageFilter(StorageFilter filter);


    // KisAllTagsModel API

    QModelIndex indexForTag(KisTagSP tag) const override;
    KisTagSP tagForIndex(QModelIndex index = QModelIndex()) const override;
    KisTagSP addEmptyTag(const QString &tagName, QVector<KoResourceSP> taggedResouces) override;
    bool addTag(const KisTagSP tag, QVector<KoResourceSP> taggedResouces = QVector<KoResourceSP>()) override;
    bool setTagInactive(const KisTagSP tag) override;
    bool setTagActive(const KisTagSP tag) override;
    bool tagResource(const KisTagSP tag, const KoResourceSP resource) override;
    bool untagResource(const KisTagSP tag, const KoResourceSP resource) override;
    bool renameTag(const KisTagSP tag) override;
    bool changeTagActive(const KisTagSP tag, bool active) override;
    QVector<KisTagSP> tagsForResource(int resourceId) const override;

protected:

    bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:

    friend class DlgDbExplorer;
    friend class TestTagModel;


    struct Private;
    Private *const d;

    Q_DISABLE_COPY(KisTagModel)

};

typedef QSharedPointer<KisAllTagsModel> KisAllTagsModelSP;

#endif // KisAllTagsModel_H
