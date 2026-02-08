/**
 *  GalleryItemColor.h
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#pragma once

#include "SARibbonGallery.h"

#include "util/BrickColor.h"

#define GALLERY_ITEM_USER_DATA 100

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class StudioGalleryItem : public SARibbonGalleryItem
{
public:
	StudioGalleryItem()
	{}

protected:
	void draw(QPainter* pPainter, SARibbonGallery* pGallery, QRect rectItem, bool enabled, bool selected, bool pressed, bool checked);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GalleryItemColor : public SARibbonGalleryItem
{
public:
	static void addStandardColors(SARibbonGalleryGroup* items);
	
	QColor getColor() const { return m_color; }
	RBX::BrickColor getBrickColor() const { return m_brickColor; }

private:
	GalleryItemColor(const RBX::BrickColor& color);
	void draw(QPainter* pPainter, SARibbonGallery* pGallery, QRect rectItem, bool enabled, bool selected, bool pressed, bool checked);

	QColor          m_color;
	RBX::BrickColor m_brickColor;
};