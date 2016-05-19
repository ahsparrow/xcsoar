/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2015 The XCSoar Project
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

package org.xcsoar;

import java.util.HashMap;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import android.media.MediaPlayer;
import android.content.Context;

public class SoundUtil {
  private static HashMap<String, Integer> resources = new HashMap<String, Integer>();

  static {
    resources.put("IDR_FAIL", R.raw.fail);
    resources.put("IDR_INSERT", R.raw.insert);
    resources.put("IDR_REMOVE", R.raw.remove);
    resources.put("IDR_WAV_BEEPBWEEP", R.raw.beep_bweep);
    resources.put("IDR_WAV_CLEAR", R.raw.beep_clear);
    resources.put("IDR_WAV_DRIP", R.raw.beep_drip);
  }

  public static boolean play(Context context, String name) {
    Integer id = resources.get(name);
    if (id == null)
      return false;

    MediaPlayer mp = MediaPlayer.create(context, id);
    if (mp == null)
      return false;

    mp.start();
    return true;
  }

  private static MediaPlayer voiceMediaPlayer = new MediaPlayer();
  private static ZipFile voiceZipFile = null;

  public static boolean playVoice(Context context, String name) {
    if (voiceZipFile == null)
      return false;

    // Extract zipped ogg file to temporary file
    File tmpFile;
    try {
      String entryName = name + ".ogg";

      ZipEntry entry = voiceZipFile.getEntry(entryName);
      if (entry == null)
        return false;

      InputStream in = voiceZipFile.getInputStream(entry);

      tmpFile = new File(context.getCacheDir(), entryName);
      FileOutputStream out = new FileOutputStream(tmpFile);

      int count;
      byte[] buffer = new byte[1024];
      while ((count = in.read(buffer)) != -1) 
      {
        out.write(buffer, 0, count);
      }

      in.close();
      out.close();
    } catch (IOException ex) {
      return false;
    }

    // Set media player to play temporary file
    voiceMediaPlayer.reset();
    try {
      voiceMediaPlayer.setDataSource(tmpFile.getAbsolutePath());
      voiceMediaPlayer.prepare();
    } catch (IOException ex) {
      return false;
    }

    voiceMediaPlayer.start();
    return true;
  }

  public static boolean setVoiceFile(Context context, String path) {
    try {
      voiceZipFile = new ZipFile(path);
    } catch (IOException ex) {
      voiceZipFile = null;
      return false;
    }

    return true;
  }
}
