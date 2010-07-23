/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

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

#include "InfoBoxes/Content/Altitude.hpp"

#include "InfoBoxes/InfoBoxWindow.hpp"
#include "Units.hpp"
#include "Interface.hpp"

#include "DeviceBlackboard.hpp"
#include "Simulator.hpp"

#include <tchar.h>

void
InfoBoxContentAltitudeGPS::Update(InfoBoxWindow &infobox)
{
  TCHAR sTmp[32];

  // Set Title
  infobox.SetTitle(_T("H GPS"));

  // Set Value
  Units::FormatUserAltitude(XCSoarInterface::Basic().GPSAltitude, sTmp,
                            sizeof(sTmp) / sizeof(sTmp[0]), false);
  infobox.SetValue(sTmp);

  // Set Comment
  Units::FormatAlternateUserAltitude(XCSoarInterface::Basic().GPSAltitude, sTmp,
                                     sizeof(sTmp) / sizeof(sTmp[0]));
  infobox.SetComment(sTmp);

  // Set Unit
  infobox.SetValueUnit(Units::AltitudeUnit);
}

bool
InfoBoxContentAltitudeGPS::HandleKey(const InfoBoxKeyCodes keycode)
{
  if (!is_simulator())
    return false;

  fixed fixed_step = (fixed)Units::ToSysUnit(100, Units::AltitudeUnit);
  const Angle a5 = Angle::degrees(fixed(5));

  switch (keycode) {
  case ibkUp:
    device_blackboard.SetAltitude(
        XCSoarInterface::Basic().GPSAltitude + fixed_step);
    return true;

  case ibkDown:
    device_blackboard.SetAltitude(
        max(fixed_zero, XCSoarInterface::Basic().GPSAltitude - fixed_step));
    return true;

  case ibkLeft:
    device_blackboard.SetTrackBearing(
        XCSoarInterface::Basic().TrackBearing - a5);
    return true;

  case ibkRight:
    device_blackboard.SetTrackBearing(
        XCSoarInterface::Basic().TrackBearing + a5);
    return true;
  }

  return false;
}

void
InfoBoxContentAltitudeAGL::Update(InfoBoxWindow &infobox)
{
  TCHAR sTmp[32];

  // Set Title
  infobox.SetTitle(_T("H AGL"));

  if (!XCSoarInterface::Calculated().TerrainValid) {
    infobox.SetInvalid();
    return;
  }

  // Set Value
  Units::FormatUserAltitude(XCSoarInterface::Basic().AltitudeAGL, sTmp,
                            sizeof(sTmp) / sizeof(sTmp[0]), false);
  infobox.SetValue(sTmp);

  // Set Comment
  Units::FormatAlternateUserAltitude(XCSoarInterface::Basic().AltitudeAGL, sTmp,
                                     sizeof(sTmp) / sizeof(sTmp[0]));
  infobox.SetComment(sTmp);

  // Set Unit
  infobox.SetValueUnit(Units::AltitudeUnit);

  if (XCSoarInterface::Basic().AltitudeAGL <
      XCSoarInterface::SettingsComputer().safety_height_terrain)
    // red
    infobox.SetColor(1);
  else
    infobox.SetColor(0);
}

void
InfoBoxContentAltitudeBaro::Update(InfoBoxWindow &infobox)
{
  TCHAR sTmp[32];

  // Set Title
  infobox.SetTitle(_T("H Baro"));

  if (!XCSoarInterface::Basic().BaroAltitudeAvailable) {
    infobox.SetInvalid();
    return;
  }

  // Set Value
  Units::FormatUserAltitude(XCSoarInterface::Basic().BaroAltitude, sTmp,
                            sizeof(sTmp) / sizeof(sTmp[0]), false);
  infobox.SetValue(sTmp);

  // Set Comment
  Units::FormatAlternateUserAltitude(XCSoarInterface::Basic().BaroAltitude, sTmp,
                                     sizeof(sTmp) / sizeof(sTmp[0]));
  infobox.SetComment(sTmp);

  // Set Unit
  infobox.SetValueUnit(Units::AltitudeUnit);
}

void
InfoBoxContentAltitudeQFE::Update(InfoBoxWindow &infobox)
{
  TCHAR sTmp[32];

  // Set Title
  infobox.SetTitle(_T("QFE GPS"));

  /// @todo fix QFE infobox
#ifdef OLD_TASK
  fixed Value = Units::ToUserAltitude(XCSoarInterface::Basic().GPSAltitude - QFEAltitudeOffset);
#else
  fixed Value = Units::ToUserAltitude(XCSoarInterface::Basic().GPSAltitude);
#endif

  // Set Value
  Units::FormatUserAltitude(Value, sTmp,
                            sizeof(sTmp) / sizeof(sTmp[0]), false);
  infobox.SetValue(sTmp);

  // Set Comment
  Units::FormatAlternateUserAltitude(Value, sTmp,
                                     sizeof(sTmp) / sizeof(sTmp[0]));
  infobox.SetComment(sTmp);

  // Set Unit
  infobox.SetValueUnit(Units::AltitudeUnit);
}

void
InfoBoxContentTerrainHeight::Update(InfoBoxWindow &infobox)
{
  TCHAR sTmp[32];

  // Set Title
  infobox.SetTitle(_T("H GND"));

  if (!XCSoarInterface::Calculated().TerrainValid) {
    infobox.SetInvalid();
    return;
  }

  // Set Value
  Units::FormatUserAltitude(XCSoarInterface::Calculated().TerrainAlt, sTmp,
                            sizeof(sTmp) / sizeof(sTmp[0]), false);
  infobox.SetValue(sTmp);

  // Set Comment
  Units::FormatAlternateUserAltitude(XCSoarInterface::Calculated().TerrainAlt, sTmp,
                                     sizeof(sTmp) / sizeof(sTmp[0]));
  infobox.SetComment(sTmp);

  // Set Unit
  infobox.SetValueUnit(Units::AltitudeUnit);
}
