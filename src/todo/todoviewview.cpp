/*
  This file is part of KOrganizer.

  Copyright (c) 2008 Thomas Thrainer <tom_t@gmx.at>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  As a special exception, permission is given to link this program
  with any edition of Qt, and distribute the resulting executable,
  without including the source code for Qt in the source distribution.
*/

#include "todoviewview.h"

#include <KLocalizedString>
#include <QMenu>

#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHeaderView>
#include <QMouseEvent>

TodoViewView::TodoViewView(QWidget *parent)
    : QTreeView(parent)
    , mHeaderPopup(nullptr)
    , mIgnoreNextMouseRelease(false)
{
    header()->installEventFilter(this);
    setAlternatingRowColors(true);
    connect(&mExpandTimer, &QTimer::timeout, this, &TodoViewView::expandParent);
    mExpandTimer.setInterval(1000);
    header()->setStretchLastSection(false);
}

bool TodoViewView::isEditing(const QModelIndex &index) const
{
    return state() & QAbstractItemView::EditingState
           && currentIndex() == index;
}

bool TodoViewView::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    if (event->type() == QEvent::ContextMenu) {
        QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);

        if (!mHeaderPopup) {
            mHeaderPopup = new QMenu(this);
            mHeaderPopup->setTitle(i18n("View Columns"));
            // First entry can't be disabled
            for (int i = 1; i < model()->columnCount(); ++i) {
                QAction *tmp
                    = mHeaderPopup->addAction(model()->headerData(i, Qt::Horizontal).toString());
                tmp->setData(QVariant(i));
                tmp->setCheckable(true);
                mColumnActions << tmp;
            }

            connect(mHeaderPopup, &QMenu::triggered, this, &TodoViewView::toggleColumnHidden);
        }

        foreach (QAction *action, mColumnActions) {
            int column = action->data().toInt();
            action->setChecked(!isColumnHidden(column));
        }

        mHeaderPopup->popup(mapToGlobal(e->pos()));
        return true;
    }

    return false;
}

void TodoViewView::toggleColumnHidden(QAction *action)
{
    if (action->isChecked()) {
        showColumn(action->data().toInt());
    } else {
        hideColumn(action->data().toInt());
    }

    Q_EMIT visibleColumnCountChanged();
}

QModelIndex TodoViewView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    QModelIndex current = currentIndex();
    if (!current.isValid()) {
        return QTreeView::moveCursor(cursorAction, modifiers);
    }

    switch (cursorAction) {
    case MoveNext:
    {
        // try to find an editable item right of the current one
        QModelIndex tmp = getNextEditableIndex(
            current.sibling(current.row(), current.column() + 1), 1);
        if (tmp.isValid()) {
            return tmp;
        }

        // check if the current item is expanded, and find an editable item
        // just below it if so
        current = current.sibling(current.row(), 0);
        if (isExpanded(current)) {
            tmp = getNextEditableIndex(model()->index(0, 0, current), 1);
            if (tmp.isValid()) {
                return tmp;
            }
        }

        // find an editable item in the item below the currently edited one
        tmp = getNextEditableIndex(current.sibling(current.row() + 1, 0), 1);
        if (tmp.isValid()) {
            return tmp;
        }

        // step back a hierarchy level, and search for an editable item there
        while (current.isValid()) {
            current = current.parent();
            tmp = getNextEditableIndex(current.sibling(current.row() + 1, 0), 1);
            if (tmp.isValid()) {
                return tmp;
            }
        }
        return QModelIndex();
    }
    case MovePrevious:
    {
        // try to find an editable item left of the current one
        QModelIndex tmp = getNextEditableIndex(
            current.sibling(current.row(), current.column() - 1), -1);
        if (tmp.isValid()) {
            return tmp;
        }

        int lastCol = model()->columnCount(QModelIndex()) - 1;

        // search on top of the item, also include expanded items
        tmp = current.sibling(current.row() - 1, 0);
        while (tmp.isValid() && isExpanded(tmp)) {
            tmp = model()->index(model()->rowCount(tmp) - 1, 0, tmp);
        }
        if (tmp.isValid()) {
            tmp = getNextEditableIndex(tmp.sibling(tmp.row(), lastCol), -1);
            if (tmp.isValid()) {
                return tmp;
            }
        }

        // step back a hierarchy level, and search for an editable item there
        current = current.parent();
        return getNextEditableIndex(current.sibling(current.row(), lastCol), -1);
    }
    default:
        break;
    }

    return QTreeView::moveCursor(cursorAction, modifiers);
}

QModelIndex TodoViewView::getNextEditableIndex(const QModelIndex &cur, int inc)
{
    if (!cur.isValid()) {
        return QModelIndex();
    }

    QModelIndex tmp;
    int colCount = model()->columnCount(QModelIndex());
    int end = inc == 1 ? colCount : -1;

    for (int c = cur.column(); c != end; c += inc) {
        tmp = cur.sibling(cur.row(), c);
        if ((tmp.flags() & Qt::ItemIsEditable) && !isIndexHidden(tmp)) {
            return tmp;
        }
    }
    return QModelIndex();
}

void TodoViewView::mouseReleaseEvent(QMouseEvent *event)
{
    mExpandTimer.stop();

    if (mIgnoreNextMouseRelease) {
        mIgnoreNextMouseRelease = false;
        return;
    }

    if (!indexAt(event->pos()).isValid()) {
        clearSelection();
        event->accept();
    } else {
        QTreeView::mouseReleaseEvent(event);
    }
}

void TodoViewView::mouseMoveEvent(QMouseEvent *event)
{
    mExpandTimer.stop();
    QTreeView::mouseMoveEvent(event);
}

void TodoViewView::mousePressEvent(QMouseEvent *event)
{
    mExpandTimer.stop();
    QModelIndex index = indexAt(event->pos());
    if (index.isValid() && event->button() == Qt::LeftButton) {
        mExpandTimer.start();
    }

    QTreeView::mousePressEvent(event);
}

void TodoViewView::expandParent()
{
    QModelIndex index = indexAt(viewport()->mapFromGlobal(QCursor::pos()));
    if (index.isValid()) {
        mIgnoreNextMouseRelease = true;
        QKeyEvent keyEvent = QKeyEvent(QEvent::KeyPress, Qt::Key_Asterisk, Qt::NoModifier);
        QTreeView::keyPressEvent(&keyEvent);
    }
}
