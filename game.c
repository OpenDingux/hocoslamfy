/*
 * Hocoslamfy, game code file
 * Copyright (C) 2014 Nebuleon Fumika <nebuleon@gcw-zero.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "SDL.h"

#include "main.h"
#include "init.h"
#include "game.h"
#include "score.h"
#include "text.h"

static uint32_t               Score;

static bool                   Boost;
static bool                   Pause;

// Where the player is. (Upper-left corner, meters.)
static float                  PlayerY;
// Where the player is going. (Meters per second.)
static float                  PlayerSpeed;

// What the player avoids.
static struct HocoslamfyRect* Rectangles     = NULL;
static uint32_t               RectangleCount = 0;

static float                  GenDistance;

void GameGatherInput(bool* Continue)
{
	SDL_Event ev;

	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
			case SDL_KEYDOWN:
				if (ev.key.keysym.sym == SDLK_LCTRL  /* GCW Zero: A */
				 || ev.key.keysym.sym == SDLK_LALT   /* GCW Zero: B */)
					Boost = true;
				if (ev.key.keysym.sym == SDLK_RETURN /* GCW Zero: Start */
				 || ev.key.keysym.sym == SDLK_SPACE  /* GCW Zero: Y */)
					Pause = !Pause;
				else if (ev.key.keysym.sym == SDLK_RETURN /* GCW Zero: Select */)
				{
					*Continue = false;
					return;
				}
				break;
			default:
				break;
		}
	}
}

void GameDoLogic(bool* Continue, bool* Error)
{
	if (!Pause)
	{
		// Scroll all rectangles to the left.
		int32_t i;
		bool PointAwarded = false;
		for (i = RectangleCount - 1; i >= 0; i--)
		{
			Rectangles[i].Left += FIELD_SCROLL;
			Rectangles[i].Right += FIELD_SCROLL;
			// If a rectangle is past the left side, remove it and award the
			// player with a point.
			if (Rectangles[i].Right < 0.0f)
			{
				RectangleCount--;
				memmove(&Rectangles[i], &Rectangles[i + 1], RectangleCount * sizeof(struct HocoslamfyRect));
				if (!PointAwarded)
				{
					Score++;
					PointAwarded = true;
				}
			}
		}
		// Generate a rectangle now if needed.
		if (RectangleCount == 0 || (SCREEN_WIDTH * (SCREEN_HEIGHT / FIELD_HEIGHT)) - Rectangles[RectangleCount - 1].Right >= GenDistance)
		{
			float Left;
			if (RectangleCount == 0)
				Left = FIELD_WIDTH - FIELD_SCROLL;
			else
			{
				Left = Rectangles[RectangleCount - 1].Right + GenDistance;
				GenDistance += RECT_GEN_SPEED;
			}
			Rectangles = realloc(Rectangles, (RectangleCount + 2) * sizeof(struct HocoslamfyRect));
			RectangleCount += 2;
			Rectangles[RectangleCount - 2].Left = Rectangles[RectangleCount - 1].Left = Left;
			Rectangles[RectangleCount - 2].Right = Rectangles[RectangleCount - 1].Right = Left + RECT_WIDTH;
			// Where's the place for the player to go through?
			float GapTop = GAP_HEIGHT + 1.0f + ((float) rand() / (float) RAND_MAX) * (FIELD_HEIGHT - GAP_HEIGHT - 2.0f);
			Rectangles[RectangleCount - 2].Top = FIELD_HEIGHT;
			Rectangles[RectangleCount - 2].Bottom = GapTop;
			Rectangles[RectangleCount - 1].Top = GapTop - GAP_HEIGHT;
			Rectangles[RectangleCount - 1].Bottom = 0.0f;
		}
		// Update the speed at which the player is going.
		PlayerSpeed += GRAVITY / 60;
		if (Boost)
		{
			// The player expects to rise a constant amount with each press of
			// the triggering key or button, so set his or her speed to
			// boost him or her from zero, even if the speed was positive.
			// For a more physically-realistic version of thrust, use
			// [PlayerSpeed += SPEED_BOOST;].
			PlayerSpeed = SPEED_BOOST;
			Boost = false;
		}
		// Update the player's position.
		// If the player's position has collided with the borders of the field,
		// the player's game is over.
		PlayerY += PlayerSpeed;
		if (PlayerY > FIELD_HEIGHT || PlayerY - PLAYER_SIZE < 0.0f)
		{
			ToScore(Score, FIELD_BORDER_COLLISION);
			return;
		}
		// Collision detection.
		for (i = 0; i < RectangleCount; i++)
		{
			if (((PlayerY               > Rectangles[i].Bottom
			   && PlayerY               < Rectangles[i].Top)
			  || (PlayerY - PLAYER_SIZE > Rectangles[i].Bottom
			   && PlayerY - PLAYER_SIZE < Rectangles[i].Top))
			 && ((0.0f                  > Rectangles[i].Left
			   && 0.0f                  < Rectangles[i].Right)
			  || (PLAYER_SIZE           > Rectangles[i].Left
			   && PLAYER_SIZE           < Rectangles[i].Right)))
			{
				ToScore(Score, RECTANGLE_COLLISION);
				return;
			}
		}
	}
}

void GameOutputFrame()
{
	SDL_FillRect(Screen, NULL, SDL_MapRGB(Screen->format, 0, 0, 0));

	// Draw the rectangles.
	uint32_t i;
	for (i = 0; i < RectangleCount; i++)
	{
		SDL_Rect RectPixels = {
			.x = (int) (Rectangles[i].Left * SCREEN_WIDTH / FIELD_WIDTH),
			.y = SCREEN_HEIGHT - (int) (Rectangles[i].Top * SCREEN_HEIGHT / FIELD_HEIGHT),
			.w = (int) ((Rectangles[i].Right - Rectangles[i].Left) * SCREEN_WIDTH / FIELD_WIDTH),
			.h = (int) ((Rectangles[i].Top - Rectangles[i].Bottom) * SCREEN_HEIGHT / FIELD_HEIGHT)
		};
		SDL_FillRect(Screen, &RectPixels, SDL_MapRGB(Screen->format, 128, 128, 128));
	}

	// Draw the character.
	SDL_Rect PlayerPixels = {
		.x = 0,
		.y = SCREEN_HEIGHT - (PlayerY * SCREEN_HEIGHT / FIELD_HEIGHT),
		.w = (int) (PLAYER_SIZE * SCREEN_HEIGHT / FIELD_HEIGHT),
		.h = (int) (PLAYER_SIZE * SCREEN_HEIGHT / FIELD_HEIGHT)
	};
	SDL_FillRect(Screen, &PlayerPixels, SDL_MapRGB(Screen->format, 255, 255, 255));

	// Draw the player's current score.
	char ScoreString[17];
	sprintf(ScoreString, "Score%10" PRIu32, Score);
	if (SDL_MUSTLOCK(Screen))
		SDL_LockSurface(Screen);
	PrintStringOutline(ScoreString,
		SDL_MapRGB(Screen->format, 255, 255, 255),
		SDL_MapRGB(Screen->format, 0, 0, 0),
		Screen->pixels,
		Screen->pitch,
		0,
		0,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		RIGHT,
		TOP);
	if (SDL_MUSTLOCK(Screen))
		SDL_UnlockSurface(Screen);

	SDL_Flip(Screen);
}

void ToGame(void)
{
	Score = 0;
	Boost = false;
	Pause = false;
	PlayerY = (FIELD_HEIGHT - PLAYER_SIZE) / 2;
	PlayerSpeed = 0.0f;
	if (Rectangles != NULL)
	{
		free(Rectangles);
		Rectangles = NULL;
	}
	RectangleCount = 0;
	GenDistance = RECT_GEN_START;

	GatherInput = GameGatherInput;
	DoLogic     = GameDoLogic;
	OutputFrame = GameOutputFrame;
}