#ifndef SPRITES_H
#define SPRITES_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <fabgl.h>

#include "agon.h"
#include "agon_screen.h"

uint16_t		currentBitmap = 0;				// Current bitmap ID
std::unordered_map<uint16_t, std::shared_ptr<Bitmap>> bitmaps;	// Storage for our bitmaps
uint8_t			numsprites = 0;					// Number of sprites on stage
uint8_t			current_sprite = 0;				// Current sprite number
Sprite			sprites[MAX_SPRITES];			// Sprite object storage

std::shared_ptr<Bitmap> getBitmap(uint16_t id = currentBitmap) {
	if (bitmaps.find(id) != bitmaps.end()) {
		return bitmaps[id];
	}
	return nullptr;
}

inline void setCurrentBitmap(uint8_t b) {
	currentBitmap = b + BUFFERED_BITMAP_BASEID;
}

inline void setCurrentBitmap(uint16_t b) {
	currentBitmap = b;
}

inline uint16_t getCurrentBitmapId() {
	return currentBitmap;
}

void clearBitmap(uint16_t b = currentBitmap) {
	if (bitmaps.find(b) == bitmaps.end()) {
		return;
	}
	bitmaps.erase(b);
}

void drawBitmap(uint16_t x, uint16_t y) {
	auto bitmap = getBitmap();
	if (bitmap) {
		canvas->drawBitmap(x, y, bitmap.get());
		waitPlotCompletion();
	} else {
		debug_log("drawBitmap: bitmap %d not found\n\r", currentBitmap);
	}
}

void resetBitmaps() {
	bitmaps.clear();
}

Sprite * getSprite(uint8_t sprite = current_sprite) {
	return &sprites[sprite];
}

inline void setCurrentSprite(uint8_t s) {
	current_sprite = s;
}

inline uint8_t getCurrentSprite() {
	return current_sprite;
}

void clearSpriteFrames(uint8_t s = current_sprite) {
	auto sprite = getSprite(s);
	sprite->visible = false;
	sprite->setFrame(0);
	sprite->clearBitmaps();
}

void addSpriteFrame(uint8_t bitmapId) {
	auto sprite = getSprite();
	auto bitmap = getBitmap(bitmapId + BUFFERED_BITMAP_BASEID);
	if (!bitmap) {
		debug_log("addSpriteFrame: bitmap %d not found\n\r", bitmapId);
		return;
	}
	// TODO track that we're using this bitmap for a sprite
	sprite->addBitmap(bitmap.get());
}

void activateSprites(uint8_t n) {
	/*
	* Sprites 0-(numsprites-1) will be activated on-screen
	* Make sure all sprites have at least one frame attached to them
	*/
	numsprites = n;

	if (numsprites) {
		_VGAController->setSprites(sprites, numsprites);
	} else {
		_VGAController->removeSprites();
	}
	waitPlotCompletion();
}

inline bool hasActiveSprites() {
	return numsprites > 0;
}

void nextSpriteFrame() {
	auto sprite = getSprite();
	sprite->nextFrame();
}

void previousSpriteFrame() {
	auto sprite = getSprite();
	auto frame = sprite->currentFrame;
	sprite->setFrame(frame ? frame - 1 : sprite->framesCount - 1);
}

void setSpriteFrame(uint8_t n) {
	auto sprite = getSprite();
	if (n < sprite->framesCount) {
		sprite->setFrame(n);
	}
}

void showSprite() {
	auto sprite = getSprite();
	sprite->visible = 1;
}

void hideSprite() {
	auto sprite = getSprite();
	sprite->visible = 0;
}

void moveSprite(int x, int y) {
	auto sprite = getSprite();
	sprite->moveTo(x, y);
}

void moveSpriteBy(int x, int y) {
	auto sprite = getSprite();
	sprite->moveBy(x, y);
}

void refreshSprites() {
	if (numsprites) {
		_VGAController->refreshSprites();
	}
}

void resetSprites() {
	for (auto n = 0; n < MAX_SPRITES; n++) {
		clearSpriteFrames(n);
	}
	waitPlotCompletion();
}

#endif // SPRITES_H
