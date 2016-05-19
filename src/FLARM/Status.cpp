/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2013 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

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
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "FLARM/Status.hpp"
#include "Audio/Sound.hpp"
#include <stdlib.h>

#define AHEAD_ANGLE  15
#define FRONT_ANGLE  45
#define BACK_ANGLE   100
#define BEHIND_ANGLE 150

static void
Alarm(int bearing)
{
  if (abs(bearing) <= AHEAD_ANGLE)
    PlayVoice("ahead");
  else if (abs(bearing) > BEHIND_ANGLE)
    PlayVoice("behind");
  else if (bearing > BACK_ANGLE)
    PlayVoice("right-back");
  else if (bearing > FRONT_ANGLE)
    PlayVoice("right");
  else if (bearing > 0)
    PlayVoice("right-front");
  else if (bearing < -BACK_ANGLE)
    PlayVoice("left-back");
  else if (bearing < -FRONT_ANGLE)
    PlayVoice("left");
  else
    PlayVoice("left-front");
}

void
FlarmStatus::Update(const FlarmStatus &other, int bearing, fixed clock)
{
  if (other.alarm_level > alarm_level)
  {
    Alarm(bearing);
  }

  available.Update(clock);

  rx = other.rx;
  tx = other.tx;
  gps = other.gps;
  alarm_level = other.alarm_level;
}
