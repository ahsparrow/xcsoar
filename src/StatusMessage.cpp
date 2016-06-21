/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
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

#include "StatusMessage.hpp"
#include "Util/StringAPI.hxx"
#include "Util/Macros.hpp"
#include "Util/StaticArray.hxx"
#include "Util/StaticString.hxx"

#include <assert.h>

// Default status messages (auto-generated from Data/Status/defaults.xcs)
static constexpr StatusMessage default_status_messages[] = {
#include "Status_defaults.cpp"
};

// There are currently approximately 60 different popup messages in the
// source. It's very unlikely they would all need to be simultaneously
// configured
static const unsigned NUM_MSGS = 50;

// There are currently only six different sound resources defined.
static const unsigned NUM_SOUNDS = 20;

typedef StaticString<80> sm_key_t;
typedef StaticString<40> sm_sound_t;

static StaticArray<StatusMessage, NUM_MSGS> status_messages;
static StaticArray<sm_key_t, NUM_MSGS> keys;
static StaticArray<sm_sound_t, NUM_SOUNDS> sounds;

gcc_pure
const StatusMessage &
FindStatusMessage(const TCHAR *key)
{
  // Search configurable messages
  for (const auto &sm : status_messages) {
    if (StringIsEqual(key, sm.key))
      return sm;
  }

  // If not found then search defaults (default_status_messages[0] contains
  // the fallback setting)
  assert(ARRAY_SIZE(default_status_messages) > 0);

  unsigned i = ARRAY_SIZE(default_status_messages) - 1;
  for (; i > 0; --i)
    if (StringIsEqual(key, default_status_messages[i].key))
      break;

  return default_status_messages[i];
}

bool
SetStatusMessage(const TCHAR *key, const TCHAR *sound, bool visible,
                 unsigned delay_ms)
{
  sm_key_t sm_key;
  sm_key.SetASCII(key);
  bool new_key = !keys.contains(sm_key);

  bool null_sound = StringIsEqual(sound, "null") ||
                    StringIsEqual(sound, "NULL");

  sm_sound_t sm_sound;
  sm_sound.SetASCII(sound);
  bool new_sound = !null_sound && !sounds.contains(sm_sound);

  // Check there's room for new settings
  if ((new_key && keys.full()) || (new_sound && sounds.full()))
    return false;

  // Update lists of keys and sounds
  if (new_key)
    keys.append(sm_key);

  if (new_sound)
    sounds.append(sm_sound);

  // Create new status message configuration
  auto kp = std::find(keys.begin(), keys.end(), sm_key);
  StatusMessage status_msg = {kp->c_str(), nullptr, visible, delay_ms};

  if (!null_sound) {
    auto sp = std::find(sounds.begin(), sounds.end(), sm_sound);
    status_msg.sound = sp->c_str();
  }

  // Append or update status message array
  if (new_key)
    status_messages.append(status_msg);
  else
    for (auto &sm : status_messages)
      if (sm.key == sm_key)
        sm = status_msg;

  return true;
}

void
ResetStatusMessage()
{
  status_messages.clear();
  keys.clear();
  sounds.clear();
}

int
SizeStatusMessage()
{
  return status_messages.size();
}

int
MaxSizeStatusMessage()
{
  return status_messages.capacity();
}
