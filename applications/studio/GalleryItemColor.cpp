/**
 *  GalleryItemColor.cpp
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#include "SARibbonGalleryItem.h"
#include "stdafx.h"
#include "GalleryItemColor.h"

// Qt headers
#include <QPainter>
#include <QStyleOption>
#include <QColor>

// Roblox headers
#include "util/BrickColor.h"

// Roblox Studio Headers
#include "QtUtilities.h"

	void StudioGalleryItem::draw(QPainter* pPainter, SARibbonGallery* pGallery, QRect rectItem, bool enabled, bool selected, bool pressed, bool checked)
	{


	rectItem.adjust(2, 2, -2, -2);
	pPainter->setPen(QPen(Qt::black, 1));
	pPainter->drawRect(rectItem);

	if (!enabled)
	{
		pPainter->drawPixmap(rectItem, icon().pixmap(rectItem.size(), QIcon::Disabled));
		return;
	}

	pPainter->drawPixmap(rectItem, icon().pixmap(rectItem.size()));

	if (selected || checked)
	{
		rectItem.adjust(0, 0, -1, -1);

		if (selected)
			pPainter->fillRect(rectItem, QColor(242, 148, 54));
		else
			pPainter->fillRect(rectItem, QColor(239, 72, 16));

		rectItem.adjust(1, 1, -1, -1);
		pPainter->fillRect(rectItem, QColor(255, 226, 148));
	}
}

void GalleryItemColor::addStandardColors(SARibbonGalleryGroup* pGalleryGroup)
{
	std::vector<RBX::BrickColor>::const_iterator iter = RBX::BrickColor::colorPalette().begin();
	while (iter != RBX::BrickColor::colorPalette().end())
	{
		GalleryItemColor* pItem = new GalleryItemColor(*iter);
		pItem->setData(Qt::SizeHintRole, pGalleryGroup->gridSize());
		pItem->setToolTip(iter->name().c_str());
		pItem->setData(GALLERY_ITEM_USER_DATA, iter->number);

		pGalleryGroup->addItem(pItem);
		iter++;
	}
}

GalleryItemColor::GalleryItemColor(const RBX::BrickColor& color)
: m_brickColor(color)
{
	m_color = QtUtilities::toQColor(color.color3());
}

void GalleryItemColor::draw(QPainter* pPainter, SARibbonGallery* pGallery, QRect rectItem, bool enabled, bool selected, bool pressed, bool checked)
{
	// set margin
	rectItem.adjust(2, 2, -2, -2);
	pPainter->setPen(QPen(Qt::black, 1));
	pPainter->drawRect(rectItem);

	if (!enabled)
	{
		// draw disabled
		int grayScale = qGray(m_color.rgb());
		pPainter->fillRect(rectItem, QColor(grayScale, grayScale, grayScale));
		return;
	}

	// fill the item color
	pPainter->fillRect(rectItem, m_color);

	if (selected || checked)
	{
		rectItem.adjust(0, 0, -1, -1);

		if (selected)
			pPainter->fillRect(rectItem, QColor(242, 148, 54));
		else
			pPainter->fillRect(rectItem, QColor(239, 72, 16));

		rectItem.adjust(1, 1, -1, -1);
		pPainter->fillRect(rectItem, QColor(255, 226, 148));
	}
	else
	{
		pPainter->fillRect(rectItem.left(), rectItem.top(), rectItem.width(), 1, m_color);
		pPainter->fillRect(rectItem.left(), rectItem.top(), 1, rectItem.height(), m_color);
		pPainter->fillRect(rectItem.right(), rectItem.top(), 1, rectItem.height(), m_color);
		pPainter->fillRect(rectItem.left(), rectItem.bottom(), rectItem.width(), 1, m_color);
	}
}
