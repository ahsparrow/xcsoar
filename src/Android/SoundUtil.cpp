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

#include "Android/SoundUtil.hpp"
#include "Java/Class.hpp"
#include "Java/String.hpp"

namespace SoundUtil {
  static Java::TrivialClass cls;
  static jmethodID play_method;
  static jmethodID play_voice_method;
  static jmethodID set_voice_file_method;
}

void
SoundUtil::Initialise(JNIEnv *env)
{
  assert(!cls.IsDefined());
  assert(env != NULL);

  cls.Find(env, "org/xcsoar/SoundUtil");
  play_method = env->GetStaticMethodID(cls, "play",
                                       "(Landroid/content/Context;"
                                       "Ljava/lang/String;)Z");
  play_voice_method = env->GetStaticMethodID(cls, "playVoice",
                                             "(Landroid/content/Context;"
                                             "Ljava/lang/String;)Z");
  set_voice_file_method = env->GetStaticMethodID(cls, "setVoiceFile",
                                                "(Landroid/content/Context;"
                                                "Ljava/lang/String;)Z");
}

void
SoundUtil::Deinitialise(JNIEnv *env)
{
  cls.Clear(env);
}

bool
SoundUtil::Play(JNIEnv *env, jobject context, const char *name)
{
  Java::String paramName(env, name);
  return env->CallStaticBooleanMethod(cls, play_method, context,
                                      paramName.Get());
}

bool
SoundUtil::PlayVoice(JNIEnv *env, jobject context, const char *name)
{
  Java::String paramName(env, name);
  return env->CallStaticBooleanMethod(cls, play_voice_method, context,
                                      paramName.Get());
}

bool
SoundUtil::SetVoiceFile(JNIEnv *env, jobject context, const char *path)
{
  Java::String paramName(env, path);
  return env->CallStaticBooleanMethod(cls, set_voice_file_method, context,
                                      paramName.Get());
}
