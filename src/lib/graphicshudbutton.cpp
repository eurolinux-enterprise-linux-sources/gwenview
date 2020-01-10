// vim: set tabstop=4 shiftwidth=4 expandtab:
/*
Gwenview: an image viewer
Copyright 2011 Aurélien Gâteau <agateau@kde.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "graphicshudbutton.moc"

// Local
#include <fullscreentheme.h>

// KDE
#include <KDebug>
#include <KGlobalSettings>
#include <KIconLoader>

// Qt
#include <QAction>
#include <QFontMetrics>
#include <QGraphicsSceneEvent>
#include <QIcon>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionGraphicsItem>

namespace Gwenview
{

struct LayoutInfo
{
    QRect iconRect;
    QRect textRect;
    QSize size;
};

struct GraphicsHudButtonPrivate
{
    GraphicsHudButton* q;
    QAction* mAction;

    QIcon mIcon;
    QString mText;

    bool mIsDown;

    void initLayoutInfo(LayoutInfo* info)
    {
        FullScreenTheme::RenderInfo renderInfo = FullScreenTheme::renderInfo(FullScreenTheme::ButtonWidget);
        const int padding = renderInfo.padding;

        QSize sh;
        if (!mIcon.isNull()) {
            int size = KIconLoader::global()->currentSize(KIconLoader::Small);
            info->iconRect = QRect(padding, padding, size, size);
        }
        if (!mText.isEmpty()) {
            QFont font = KGlobalSettings::generalFont();
            QFontMetrics fm(font);
            QSize size = fm.size(0, mText);
            info->textRect = QRect(padding, padding, size.width(), size.height());
            if (!info->iconRect.isNull()) {
                info->textRect.translate(info->iconRect.right(), 0);
            }
        }

        QRectF rect = info->iconRect | info->textRect;
        info->size = QSize(rect.right() + padding, rect.bottom() + padding);
    }

    void initFromAction()
    {
        Q_ASSERT(mAction);
        q->setIcon(mAction->icon());
        q->setText(mAction->text());
    }
};

GraphicsHudButton::GraphicsHudButton(QGraphicsItem* parent)
: QGraphicsWidget(parent)
, d(new GraphicsHudButtonPrivate)
{
    d->q = this;
    d->mAction = 0;
    d->mIsDown = false;
    setCursor(Qt::ArrowCursor);
    setAcceptHoverEvents(true);
}

GraphicsHudButton::~GraphicsHudButton()
{
    delete d;
}

void GraphicsHudButton::setIcon(const QIcon& icon)
{
    d->mIcon = icon;
    updateGeometry();
}

void GraphicsHudButton::setText(const QString& text)
{
    d->mText = text;
    updateGeometry();
}

void GraphicsHudButton::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    FullScreenTheme::State state;
    if (option->state.testFlag(QStyle::State_MouseOver)) {
        state = d->mIsDown ? FullScreenTheme::DownState : FullScreenTheme::MouseOverState;
    } else {
        state = FullScreenTheme::NormalState;
    }
    FullScreenTheme::RenderInfo renderInfo = FullScreenTheme::renderInfo(FullScreenTheme::ButtonWidget, state);

    painter->setPen(renderInfo.borderPen);
    painter->setBrush(renderInfo.bgBrush);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawRoundedRect(boundingRect().adjusted(.5, .5, -.5, -.5), renderInfo.borderRadius, renderInfo.borderRadius);

    LayoutInfo info;
    d->initLayoutInfo(&info);

    if (!d->mIcon.isNull()) {
        painter->drawPixmap(
            info.iconRect.topLeft(),
            d->mIcon.pixmap(info.iconRect.size())
        );
    }
    if (!d->mText.isEmpty()) {
        painter->setPen(renderInfo.textPen);
        painter->drawText(
            info.textRect,
            Qt::AlignCenter,
            d->mText);
    }
}

QSizeF GraphicsHudButton::sizeHint(Qt::SizeHint /*which*/, const QSizeF& /*constraint*/) const
{
    LayoutInfo info;
    d->initLayoutInfo(&info);
    return info.size;
}

void GraphicsHudButton::mousePressEvent(QGraphicsSceneMouseEvent*)
{
    d->mIsDown = true;
    update();
}

void GraphicsHudButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    d->mIsDown = false;
    update();
    if (boundingRect().contains(event->pos())) {
        clicked();
    }
}

void GraphicsHudButton::setDefaultAction(QAction* action)
{
    if (action != d->mAction) {
        d->mAction = action;
        if (!actions().contains(action)) {
            addAction(action);
        }
        d->initFromAction();
        connect(this, SIGNAL(clicked()),
            d->mAction, SLOT(trigger()));
    }
}

bool GraphicsHudButton::event(QEvent* event)
{
    if (event->type() == QEvent::ActionChanged) {
        d->initFromAction();
    }
    return QGraphicsWidget::event(event);
}

} // namespace
