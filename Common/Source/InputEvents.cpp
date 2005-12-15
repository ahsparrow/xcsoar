/* 

InputEvents

  This class is used to control all user and external InputEvents.
  This includes some Nmea strings, virtual events (Glide Computer
  Evnets) and Keyboard.

  What it does not cover is Glide Computer normal processing - this
  includes GPS and Vario processing.

  What it does include is what to do when an automatic event (switch
  to Climb mode) and user events are entered.

  It also covers the configuration side of on screen labels.

  For further information on config file formats see

	source/Common/Data/Input/ALL
	doc/html/advanced/input/ALL		http://xcsoar.sourceforge.net/advanced/input/

Copyright_License {

  XCSoar Glide Computer - http://xcsoar.sourceforge.net/
  Copyright (C) 2000 - 2005  

  	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@bigfoot.com>

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


#include "stdafx.h"
#include "XCSoar.h"
#include "InputEvents.h"
#include "Utils.h"
#include "VarioSound.h"
#include "Terrain.h"
#include "compatibility.h"
#include <commctrl.h>
#include <aygshell.h>
#include "InfoBoxLayout.h"
#include "Airspace.h"
#include "Process.h"
#include "Port.h"
#include "Message.h"
#include "Units.h"

// Sensible maximums 
#define MAX_MODE 100
#define MAX_MODE_STRING 25
#define MAX_KEY 255
#define MAX_EVENTS 2048
#define MAX_LABEL NUMBUTTONLABELS

/*
	TODO - All of this input_Errors code needs to be removed and replaced with standard logger.
	The logger can then display messages through Message:: if ncessary and log to files etc
	This code, and baddly written #ifdef should be moved to Macros in the Log class.
*/


#ifdef _INPUTDEBUG_
	// Log first NN input event errors for display in simulator mode
	#define MAX_INPUT_ERRORS 5
	TCHAR input_errors[MAX_INPUT_ERRORS][3000];
	int input_errors_count = 0; 
	// JMW this is just far too annoying right now,
	// since "title" "note" and commencts are not parsed, they
	// come up as errors.
#endif

// Current modes - map mode to integer (primitive hash)
TCHAR mode_current[MAX_MODE_STRING] = TEXT("default");		// Current mode
TCHAR mode_map[MAX_MODE][MAX_MODE_STRING];					// Map mode to location
int mode_map_count = 0;

// Key map to Event - Keys (per mode) mapped to events
int Key2Event[MAX_MODE][MAX_KEY];		// Points to Events location

// Glide Computer Events
int GC2Event[MAX_MODE][GCE_COUNT];

// NMEA Triggered Events
int N2Event[MAX_MODE][NE_COUNT];

// Events - What do you want to DO
typedef struct {
  pt2Event event;		// Which function to call (can be any, but should be here)
  TCHAR *misc;			// Parameters
  int next;				// Next in event list - eg: Macros
} EventSTRUCT;
EventSTRUCT Events[MAX_EVENTS];	
int Events_count;				// How many have we defined

// Labels - defined per mode
typedef struct {
  TCHAR *label;
  int location;
  int event;
} ModeLabelSTRUCT;
ModeLabelSTRUCT ModeLabel[MAX_MODE][MAX_LABEL];
int ModeLabel_count[MAX_MODE];				// Where are we up to in this mode...

// -----------------------------------------------------------------------
// Initialisation and Defaults
// -----------------------------------------------------------------------

bool InitONCE = false;
extern TCHAR szRegistryInputFile[];

// Mapping text names of events to the real thing
typedef struct {
  TCHAR *text;
  pt2Event event;
} Text2EventSTRUCT;
Text2EventSTRUCT Text2Event[256];
int Text2Event_count;

// Mapping text names of events to the real thing
TCHAR *Text2GCE[GCE_COUNT];

// Mapping text names of events to the real thing
TCHAR *Text2NE[NE_COUNT];

// DLL Cache
typedef void (CALLBACK *DLLFUNC_INPUTEVENT)(TCHAR*);
typedef void (CALLBACK *DLLFUNC_SETHINST)(HMODULE);

#define MAX_DLL_CACHE 256
typedef struct {
  TCHAR *text;
  HINSTANCE hinstance;
} DLLCACHESTRUCT;
DLLCACHESTRUCT DLLCache[MAX_DLL_CACHE];
int DLLCache_Count = 0;

// Read the data files
void InputEvents::readFile() {
  // Get defaults 
  if (!InitONCE) {
#include "InputEvents_defaults.cpp"
#include "InputEvents_Text2Event.cpp"
    InitONCE = true;
  }

  // Read in user defined configuration file
	
  TCHAR szFile1[MAX_PATH] = TEXT("\0");
  FILE *fp;

  // Open file from registry
  GetRegistryString(szRegistryInputFile, szFile1, MAX_PATH);
  SetRegistryString(szRegistryInputFile, TEXT("\0"));
	
  if (szFile1)
    fp  = _tfopen(szFile1, TEXT("rt"));

  if (fp == NULL) 
    return;

  // TODO - Safer sizes, strings etc - use C++ (can scanf restrict length?)
  TCHAR buffer[2049];	// Buffer for all
  TCHAR key[2049];	// key from scanf
  TCHAR value[2049];	// value from scanf
  TCHAR *new_label = NULL;		
  int found;

  // Init first entry
  bool some_data = false;		// Did we fin some in the last loop...
  TCHAR d_mode[1024] = TEXT("");			// Multiple modes (so large string)
  TCHAR d_type[256] = TEXT("");
  TCHAR d_data[256] = TEXT("");
  int event_id = 0;
  TCHAR d_label[256] = TEXT("");
  int d_location = 0;
	TCHAR d_event[256] = TEXT("");
	TCHAR d_misc[256] = TEXT("");

	int line = 0;

                                                                                                                                              /* Read from the file */
  while (
	 _fgetts(buffer, 2048, fp)
	 // TODO What about \r - as in \r\n ?
	 // TODO Note that ^# does not allow # in key - might be required (probably not)
	 //		Better way is to separate the check for # and the scanf 
	 && ((found = _stscanf(buffer, TEXT("%[^#=]=%[^\n]\n"), key, value)) != EOF)
  ) {
	  line++;

    // Check valid line? If not valid, assume next record (primative, but works ok!)
	if ((buffer[0] == '\n') || (buffer[0] == NULL)) {
		// General checks before continue...
		if (
			some_data
			&& (d_mode != NULL)						// We have a mode
			&& (_tcscmp(d_mode, TEXT("")) != 0)		//
		) {

			TCHAR *token;

			// For each mode
			token = _tcstok(d_mode, TEXT(" "));

			// General errors - these should be true
			ASSERT(d_location >= 0);
  			ASSERT(d_location < 1024);	// Scott arbitrary limit
			ASSERT(event_id >= 0);
			ASSERT(d_mode != NULL);
			ASSERT(d_type != NULL);
			ASSERT(d_label != NULL);

			// These could indicate bad data - thus not an ASSERT (debug only)
			// ASSERT(_tcslen(d_mode) < 1024);
			// ASSERT(_tcslen(d_type) < 1024);
			// ASSERT(_tcslen(d_label) < 1024);

			while( token != NULL ) {

				// All modes are valid at this point
				int mode_id = mode2int(token, true);
				ASSERT(mode_id >= 0);

				// Make label event
				// TODO Consider Reuse existing entries...
				if (d_location > 0) {
					// Only copy this once per object - save string space
					if (!new_label) {
						new_label = StringMallocParse(d_label);
					}
					InputEvents::makeLabel(mode_id, new_label, d_location, event_id);
				} 

				// Make key (Keyboard input)
				if (_tcscmp(d_type, TEXT("key")) == 0)	{	// key - Hardware key or keyboard
					int key = findKey(d_data);				// Get the int key (eg: APP1 vs 'a')
					if (key > 0)
						Key2Event[mode_id][key] = event_id;
   					#ifdef _INPUTDEBUG_
					else if (input_errors_count < MAX_INPUT_ERRORS)
					  _stprintf(input_errors[input_errors_count++], TEXT("Invalid key data: %s at %i"), d_data, line);
					#endif


				// Make gce (Glide Computer Event)
				} else if (_tcscmp(d_type, TEXT("gce")) == 0) {		// GCE - Glide Computer Event
					int key = findGCE(d_data);				// Get the int key (eg: APP1 vs 'a')
					if (key >= 0)
						GC2Event[mode_id][key] = event_id;
   					#ifdef _INPUTDEBUG_
					else if (input_errors_count < MAX_INPUT_ERRORS)
					  _stprintf(input_errors[input_errors_count++], TEXT("Invalid GCE data: %s at %i"), d_data, line);
					#endif

				// Make ne (NMEA Event)
				} else if (_tcscmp(d_type, TEXT("ne")) == 0) { 		// NE - NMEA Event
					int key = findNE(d_data);			// Get the int key (eg: APP1 vs 'a')
					if (key >= 0)
						N2Event[mode_id][key] = event_id;
   					#ifdef _INPUTDEBUG_
					else if (input_errors_count < MAX_INPUT_ERRORS)
					  _stprintf(input_errors[input_errors_count++], TEXT("Invalid GCE data: %s at %i"), d_data, line);
					#endif

				} else if (_tcscmp(d_type, TEXT("label")) == 0)	{	// label only - no key associated (label can still be touch screen)
					// Nothing to do here...

   				#ifdef _INPUTDEBUG_
				} else if (input_errors_count < MAX_INPUT_ERRORS) {
				  _stprintf(input_errors[input_errors_count++], TEXT("Invalid type: %s at %i"), d_type, line);
				#endif

				}

				token = _tcstok( NULL, TEXT(" "));
			}

		}
		
		// Clear all data.
		some_data = false;
		_tcscpy(d_mode, TEXT(""));
		_tcscpy(d_type, TEXT(""));
		_tcscpy(d_data, TEXT(""));
		event_id = 0;
		_tcscpy(d_label, TEXT(""));
		d_location = 0;
		new_label = NULL;

	} else if ((found != 2) || !key || !value) {
		// Do nothing - we probably just have a comment line
		// JG removed "void;" - causes warning (void is declaration and needs variable)
		// NOTE: Do NOT display buffer to user as it may contain an invalid stirng !

    } else {
      if (_tcscmp(key, TEXT("mode")) == 0) {
		  if (_tcslen(value) < 1024) {
				some_data = true;	// Success, we have a real entry
				_tcscpy(d_mode, value);				
		  }
      } else if (_tcscmp(key, TEXT("type")) == 0) {
		  if (_tcslen(value) < 256)
			_tcscpy(d_type, value);				
      } else if (_tcscmp(key, TEXT("data")) == 0) {
		  if (_tcslen(value) < 256)
			_tcscpy(d_data, value);				
      } else if (_tcscmp(key, TEXT("event")) == 0) {
		  if (_tcslen(value) < 256) {
				_tcscpy(d_event, TEXT(""));
				_tcscpy(d_misc, TEXT(""));
				int ef;
        #if defined(__BORLANDC__	)
        memset(d_event, 0, sizeof(d_event));
        memset(d_misc, 0, sizeof(d_event));
        if (_tcschr(value, ' ') == NULL){
          _tcscpy(d_event, value);
        } else {
        #endif
				ef = _stscanf(value, TEXT("%[^ ] %[A-Za-z0-9 \\/().,]"), d_event, d_misc);
        #if defined(__BORLANDC__	)
        }
        #endif

				// TODO Can't use token here - breaks
				// other token - damn C - how about
				// C++ String class ?

				// TCHAR *eventtoken;
				// eventtoken = _tcstok(value, TEXT(" "));
				// d_event = token;
				// eventtoken = _tcstok(value, TEXT(" "));

				if ((ef == 1) || (ef == 2)) {

					// TODO - Consider reusing
					// existing identical events
					// (not worth it right now)

					pt2Event event = findEvent(d_event);
					if (event) {
						event_id = makeEvent(event, StringMallocParse(d_misc), event_id);
   					#ifdef _INPUTDEBUG_
					} else  if (input_errors_count < MAX_INPUT_ERRORS) {
					  _stprintf(input_errors[input_errors_count++], TEXT("Invalid event type: %s at %i"), d_event, line);
					#endif
					}
   				#ifdef _INPUTDEBUG_
				} else  if (input_errors_count < MAX_INPUT_ERRORS) {
				  _stprintf(input_errors[input_errors_count++], TEXT("Invalid event type at %i"), line);
				#endif
				}
		  }
	  } else if (_tcscmp(key, TEXT("label")) == 0) {
		_tcscpy(d_label, value);				
	  } else if (_tcscmp(key, TEXT("location")) == 0) {
		_stscanf(value, TEXT("%d"), &d_location);

	  #ifdef _INPUTDEBUG_
	  } else if (input_errors_count < MAX_INPUT_ERRORS) {
		  _stprintf(input_errors[input_errors_count++], TEXT("Invalid key/value pair %s=%s at %i"), key, value, line);
	  #endif
	  }
    }

  } // end while

  // file was ok, so save it to registry
  SetRegistryString(szRegistryInputFile, szFile1);

  fclose(fp);

}

#ifdef _INPUTDEBUG_
void InputEvents::showErrors() {
	TCHAR buffer[2048];
	int i;
	for (i = 0; i < input_errors_count; i++) {
		_stprintf(buffer, TEXT("%i of %i\r\n%s"), i + 1, input_errors_count, input_errors[i]);
		DoStatusMessage(TEXT("XCI Error"), buffer);
	}
	input_errors_count = 0;
}
#endif

int InputEvents::findKey(TCHAR *data) {
  if (_tcscmp(data, TEXT("APP1")) == 0)
    return VK_APP1;
  else if (_tcscmp(data, TEXT("APP2")) == 0)
    return VK_APP2;
  else if (_tcscmp(data, TEXT("APP3")) == 0)
    return VK_APP3;
  else if (_tcscmp(data, TEXT("APP4")) == 0)
    return VK_APP4;
  else if (_tcscmp(data, TEXT("APP5")) == 0)
    return VK_APP5;
  else if (_tcscmp(data, TEXT("APP6")) == 0)
    return VK_APP6;
  else if (_tcscmp(data, TEXT("F1")) == 0)
    return VK_F1;
  else if (_tcscmp(data, TEXT("F2")) == 0)
    return VK_F2;
  else if (_tcscmp(data, TEXT("F3")) == 0)
    return VK_F3;
  else if (_tcscmp(data, TEXT("F4")) == 0)
    return VK_F4;
  else if (_tcscmp(data, TEXT("F5")) == 0)
    return VK_F5;
  else if (_tcscmp(data, TEXT("F6")) == 0)
    return VK_F6;
  else if (_tcscmp(data, TEXT("F7")) == 0)
    return VK_F7;
  else if (_tcscmp(data, TEXT("F8")) == 0)
    return VK_F8;
  else if (_tcscmp(data, TEXT("F9")) == 0)
    return VK_F9;
  else if (_tcscmp(data, TEXT("F10")) == 0)
    return VK_F10;
  else if (_tcscmp(data, TEXT("LEFT")) == 0)
    return VK_LEFT;
  else if (_tcscmp(data, TEXT("RIGHT")) == 0)
    return VK_RIGHT;
  else if (_tcscmp(data, TEXT("UP")) == 0)
    return VK_UP;
  else if (_tcscmp(data, TEXT("DOWN")) == 0)
    return VK_DOWN;
  else if (_tcscmp(data, TEXT("RETURN")) == 0)
    return VK_RETURN;
  else if (_tcslen(data) == 1)
    return towupper(data[0]);
  else
    return 0;
}

pt2Event InputEvents::findEvent(TCHAR *data) {
  int i;
  for (i = 0; i < Text2Event_count; i++) {
    if (_tcscmp(data, Text2Event[i].text) == 0)
      return Text2Event[i].event;
  }
  return NULL;
}

int InputEvents::findGCE(TCHAR *data) {
  int i;
  for (i = 0; i < GCE_COUNT; i++) {
    if (_tcscmp(data, Text2GCE[i]) == 0)
      return i;
  }
  return -1;
}

int InputEvents::findNE(TCHAR *data) {
  int i;
  for (i = 0; i < NE_COUNT; i++) {
    if (_tcscmp(data, Text2NE[i]) == 0)
      return i;
  }
  return -1;
}

// Create EVENT Entry
// NOTE: String must already be copied (allows us to use literals
// without taking up more data - but when loading from file must copy string
int InputEvents::makeEvent(void (*event)(TCHAR *), TCHAR *misc, int next) {
  if (Events_count >= MAX_EVENTS)
    return 0;
  Events_count++;	// NOTE - Starts at 1 - 0 is a noop
  Events[Events_count].event = event;
  Events[Events_count].misc = misc;
  Events[Events_count].next = next;
  return Events_count;
}


// Make a new label (add to the end each time)
// NOTE: String must already be copied (allows us to use literals
// without taking up more data - but when loading from file must copy string
void InputEvents::makeLabel(int mode_id, TCHAR* label, int location, int event_id) {
  if ((mode_id >= 0) && (mode_id < MAX_MODE) && (ModeLabel_count[mode_id] < MAX_LABEL)) {
    ModeLabel[mode_id][ModeLabel_count[mode_id]].label = label;
    ModeLabel[mode_id][ModeLabel_count[mode_id]].location = location;
    ModeLabel[mode_id][ModeLabel_count[mode_id]].event = event_id;
    ModeLabel_count[mode_id]++;
  }
}

// Return 0 for anything else - should probably return -1 !
int InputEvents::mode2int(TCHAR *mode, bool create) {
  int i = 0;
  
  // Better checks !
  if ((mode == NULL))
    return -1;
  
  for (i = 0; i < mode_map_count; i++) {
    if (_tcscmp(mode, mode_map[i]) == 0)
      return i;
  }
  
  if (create) {
    // Keep a copy
    _tcsncpy(mode_map[mode_map_count], mode, 25);
    mode_map_count++;
    return mode_map_count - 1;
  }
  
  return -1;
}


void InputEvents::setMode(TCHAR *mode) {
  static int lastmode = -1;
  int thismode;

  _tcsncpy(mode_current, mode, MAX_MODE_STRING);

  // Mode must already exist to use it here...
  thismode = mode2int(mode,false);
  if (thismode < 0)	// Technically an error in config (eg event=Mode DoesNotExist)
	  return;		// TODO Add debugging here 

  if (thismode == lastmode) return;
  lastmode = thismode;

  // TODO Enable this in debug modes
  // for debugging at least, set mode indicator on screen
  /* 
	  if (thismode==0) {
		ButtonLabel::SetLabelText(0,NULL);
	  } else {
		ButtonLabel::SetLabelText(0,mode);
	  }
  */
  ButtonLabel::SetLabelText(0,NULL);

  // Set button labels
  int i;
  for (i = 0; i < ModeLabel_count[thismode]; i++) {
    // JMW removed requirement that label has to be non-null
    if (// (ModeLabel[thismode][i].label != NULL) && 
	(ModeLabel[thismode][i].location > 0)) 
      ButtonLabel::SetLabelText(
				ModeLabel[thismode][i].location,
				ModeLabel[thismode][i].label
				);
  }

}


TCHAR* InputEvents::getMode() {
  return mode_current;
}

int InputEvents::getModeID() {
  return InputEvents::mode2int(InputEvents::getMode(), false);
}

// -----------------------------------------------------------------------
// Processing functions - which one to do
// -----------------------------------------------------------------------

// Input is a via the user touching the label on a touch screen / mouse
bool InputEvents::processButton(int bindex) {
  int thismode = getModeID();
  ASSERT(thismode >= 0);	// Must always be so because mode_current should always be valid
  int i;
  // Note - reverse order - last one wins
  for (i = ModeLabel_count[thismode]; i >= 0; i--) {
    if ((ModeLabel[thismode][i].location == bindex) 
	// && (ModeLabel[thismode][i].label != NULL)
	// JMW removed requirement of having a label!
	) {

      // JMW need a debounce method here..
      if (!Debounce()) return true;

      processGo(ModeLabel[thismode][i].event);
      return true;
    }
  }

  return false;
}

/*
  InputEvent::processKey(KeyID);
  Process keys normally brought in by hardware or keyboard presses
  Future will also allow for long and double click presses...
  Return = We had a valid key (even if nothing happens because of Bounce)
*/
bool InputEvents::processKey(int dWord) {
  int event_id;

  // Valid input ?
  if ((dWord < 0) || (dWord > MAX_KEY))
    return false;

  // get current mode
  int mode = InputEvents::getModeID();
  ASSERT(mode >= 0);
  
  // Which key - can be defined locally or at default (fall back to default)
  event_id = Key2Event[mode][dWord];
  if (event_id == 0) {
    // go with default key..
    event_id = Key2Event[0][dWord];
  }

  if (event_id > 0) {
    if (!Debounce()) return true;
    InputEvents::processGo(event_id);
    return true;
  }
  
  return false;
}

/*
  InputEvent::processNmea(TCHAR* data)
  Take hard coded inputs from NMEA processor.
  Return = TRUE if we have a valid key match
*/
bool InputEvents::processNmea(int ne_id) {
  int event_id = 0;

  // Valid input ?
  if ((ne_id < 0) || (ne_id >= NE_COUNT))
    return false;

  // get current mode
  int mode = InputEvents::getModeID();
  ASSERT(mode >= 0);
  
  // Which key - can be defined locally or at default (fall back to default)
  event_id = N2Event[mode][ne_id];
  if (event_id == 0) {
    // go with default key..
    event_id = N2Event[0][ne_id];
  }

  if (event_id > 0) {
    InputEvents::processGo(event_id);
    return true;
  }
  
  return false;
}

/*
  InputEvents::processGlideComputer
  Take virtual inputs from a Glide Computer to do special events
*/
bool InputEvents::processGlideComputer(int gce_id) {
int event_id = 0;

  // Valid input ?
  if ((gce_id < 0) || (gce_id >= GCE_COUNT))
    return false;

  // get current mode
  int mode = InputEvents::getModeID();
  ASSERT(mode >= 0);
  
  // Which key - can be defined locally or at default (fall back to default)
  event_id = GC2Event[mode][gce_id];
  if (event_id == 0) {
    // go with default key..
    event_id = GC2Event[0][gce_id];
  }

  if (event_id > 0) {
    InputEvents::processGo(event_id);
    return true;
  }
  
  return false;
}

extern int MenuTimeOut;

// EXECUTE an Event - lookup event handler and call back - no return
void InputEvents::processGo(int eventid) {
  // evnentid 0 is special for "noop" - otherwise check event
  // exists (pointer to function)
	if (eventid) {
	  if (Events[eventid].event) {
	    Events[eventid].event(Events[eventid].misc);
	    MenuTimeOut = 0;
	  }
	  if (Events[eventid].next > 0)
	    InputEvents::processGo(Events[eventid].next);
	}
	return;
}

// -----------------------------------------------------------------------
// Execution - list of things you can do
// -----------------------------------------------------------------------

// TODO Keep marker text for log file etc.
void InputEvents::eventMarkLocation(TCHAR *misc) {
  LockFlightData();
  MarkLocation(GPS_INFO.Longitude, GPS_INFO.Latitude);
  UnlockFlightData();
}


void InputEvents::eventSounds(TCHAR *misc) {
  bool OldEnableSoundVario = EnableSoundVario;
  
  if (_tcscmp(misc, TEXT("toggle")) == 0)
    EnableSoundVario = !EnableSoundVario;
  else if (_tcscmp(misc, TEXT("on")) == 0)
    EnableSoundVario = true;
  else if (_tcscmp(misc, TEXT("off")) == 0)
    EnableSoundVario = false;
  else if (_tcscmp(misc, TEXT("show")) == 0) {
    if (EnableSoundVario)
      DoStatusMessage(TEXT("Vario Sounds ON"));
    else
      DoStatusMessage(TEXT("Vario Sounds OFF"));  
  }
  
  if (EnableSoundVario != OldEnableSoundVario) {
    VarioSound_EnableSound(EnableSoundVario);
  }
}

void InputEvents::eventSnailTrail(TCHAR *misc) {
  int OldTrailActive;
  OldTrailActive = TrailActive;

  if (_tcscmp(misc, TEXT("toggle")) == 0) {
    TrailActive ++;
    if (TrailActive>2) {
      TrailActive=0;
    }
  } 
  else if (_tcscmp(misc, TEXT("off")) == 0)
    TrailActive = 0;
  else if (_tcscmp(misc, TEXT("long")) == 0)
    TrailActive = 1;
  else if (_tcscmp(misc, TEXT("short")) == 0)
    TrailActive = 2;

  else if (_tcscmp(misc, TEXT("show")) == 0) {
    if (TrailActive==0)
      DoStatusMessage(TEXT("SnailTrail OFF"));
    if (TrailActive==1) 
      DoStatusMessage(TEXT("SnailTrail ON Long"));
    if (TrailActive==2) 
      DoStatusMessage(TEXT("SnailTrail ON Short"));
  }  
}

void InputEvents::eventScreenModes(TCHAR *misc) {
  // toggle switches like this:
  //  -- normal infobox
  //  -- auxiliary infobox
  //  -- full screen
  //  -- normal infobox

  if (_tcscmp(misc, TEXT("normal")) == 0) {
    MapWindow::RequestOffFullScreen();
    EnableAuxiliaryInfo = false;
  } else if (_tcscmp(misc, TEXT("auxilary")) == 0) {
    MapWindow::RequestOffFullScreen();
    EnableAuxiliaryInfo = true;
  } else if (_tcscmp(misc, TEXT("toggleauxiliary")) == 0) {
    MapWindow::RequestOffFullScreen();
    EnableAuxiliaryInfo = !EnableAuxiliaryInfo;
  } else if (_tcscmp(misc, TEXT("full")) == 0) {
    MapWindow::RequestOnFullScreen();
  } else if (_tcscmp(misc, TEXT("togglefull")) == 0) {
    if (MapWindow::IsMapFullScreen()) {
      MapWindow::RequestOffFullScreen();
    } else {
      MapWindow::RequestOnFullScreen();
    }
  } else if (_tcscmp(misc, TEXT("show")) == 0) {
		if (MapWindow::IsMapFullScreen()) 
			DoStatusMessage(TEXT("Screen Mode Full"));
		else if (EnableAuxiliaryInfo)
			DoStatusMessage(TEXT("Screen Mode Auxiliary"));
		else 
			DoStatusMessage(TEXT("Screen Mode Normal"));
  } else {

	  if (EnableAuxiliaryInfo) {
		MapWindow::RequestToggleFullScreen();
		EnableAuxiliaryInfo = false;
	  } else {
		if (MapWindow::IsMapFullScreen()) {
		  MapWindow::RequestToggleFullScreen();		    
		} else {
		  EnableAuxiliaryInfo = true;
		}
	  }
  }
}



// eventAutoZoom - Turn on|off|toggle AutoZoom
// misc:
//	auto on - Turn on if not already
//	auto off - Turn off if not already
//	auto toggle - Toggle current full screen status
//	auto show
//	+	- Zoom in a bit
//	++	- Zoom in more
//	-	- Zoom out a bit
//	--	- Zoom out more
//	n.n	- Zoom to particular number
//	show - Show current zoom number
void InputEvents::eventZoom(TCHAR* misc) {
  // JMW pass through to handler in MapWindow
  // here:
  // -1 means toggle
  // 0 means off
  // 1 means on
  float zoom;

  if (_tcscmp(misc, TEXT("auto toggle")) == 0)
    MapWindow::Event_AutoZoom(-1);
  else if (_tcscmp(misc, TEXT("auto on")) == 0)
    MapWindow::Event_AutoZoom(1);
  else if (_tcscmp(misc, TEXT("auto off")) == 0)
    MapWindow::Event_AutoZoom(0);
  else if (_tcscmp(misc, TEXT("auto show")) == 0) {
	  if (MapWindow::isAutoZoom())
		DoStatusMessage(TEXT("AutoZoom ON"));
	  else
		DoStatusMessage(TEXT("AutoZoom OFF"));
  }
  else if (_tcscmp(misc, TEXT("out")) == 0)
    MapWindow::Event_ScaleZoom(-1);
  else if (_tcscmp(misc, TEXT("in")) == 0)
    MapWindow::Event_ScaleZoom(1);
  else if (_tcscmp(misc, TEXT("-")) == 0)
    MapWindow::Event_ScaleZoom(-1);
  else if (_tcscmp(misc, TEXT("+")) == 0)
    MapWindow::Event_ScaleZoom(1);
  else if (_tcscmp(misc, TEXT("--")) == 0)
    MapWindow::Event_ScaleZoom(-2);
  else if (_tcscmp(misc, TEXT("++")) == 0) 
    MapWindow::Event_ScaleZoom(2);
  else if (_stscanf(misc, TEXT("%f"), &zoom) == 1)
		MapWindow::Event_SetZoom((double)zoom);
}

// Pan
//	on	Turn pan on
//	off	Turn pan off
//      supertoggle Toggles pan and fullscreen
//	up	Pan up
//	down	Pan down
//	left	Pan left
//	right	Pan right
//	TODO n,n	Go that direction - +/- 
//	TODO ???	Go to particular point
//	TODO ???	Go to waypoint (eg: next, named)
void InputEvents::eventPan(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("toggle")) == 0)
    MapWindow::Event_Pan(-1);
  else if (_tcscmp(misc, TEXT("supertoggle")) == 0)
    MapWindow::Event_Pan(-2);
  else if (_tcscmp(misc, TEXT("on")) == 0) 
    MapWindow::Event_Pan(1);
  else if (_tcscmp(misc, TEXT("off")) == 0) 
    MapWindow::Event_Pan(0);
  else if (_tcscmp(misc, TEXT("up")) == 0)
    MapWindow::Event_PanCursor(0,1);
  else if (_tcscmp(misc, TEXT("down")) == 0)
    MapWindow::Event_PanCursor(0,-1);
  else if (_tcscmp(misc, TEXT("left")) == 0)
    MapWindow::Event_PanCursor(1,0);
  else if (_tcscmp(misc, TEXT("right")) == 0)
    MapWindow::Event_PanCursor(-1,0);
  else if (_tcscmp(misc, TEXT("show")) == 0) {
    if (MapWindow::isPan())
      DoStatusMessage(TEXT("Pan mode ON"));
    else
      DoStatusMessage(TEXT("Pan mode OFF"));
  }

}

// Do JUST Terrain/Toplogy (toggle any, on/off any, show)
void InputEvents::eventTerrainTopology(TCHAR *misc) {

  if (_tcscmp(misc, TEXT("terrain toggle")) == 0) 
	  MapWindow::Event_TerrainTopology(-2);

  else if (_tcscmp(misc, TEXT("toplogy toggle")) == 0) 
	  MapWindow::Event_TerrainTopology(-3);

  else if (_tcscmp(misc, TEXT("terrain on")) == 0) 
	  MapWindow::Event_TerrainTopology(1);

  else if (_tcscmp(misc, TEXT("terrain off")) == 0) 
	  MapWindow::Event_TerrainTopology(1);

  else if (_tcscmp(misc, TEXT("topology on")) == 0) 
	  MapWindow::Event_TerrainTopology(1);

  else if (_tcscmp(misc, TEXT("topology off")) == 0) 
	  MapWindow::Event_TerrainTopology(1);

  else if (_tcscmp(misc, TEXT("show")) == 0) 
	  MapWindow::Event_TerrainTopology(0);

  else if (_tcscmp(misc, TEXT("toggle")) == 0) 
	  MapWindow::Event_TerrainTopology(-1);

}

 // Do clear warnings IF NONE Toggle Terrain/Topology
void InputEvents::eventClearWarningsOrTerrainTopology(TCHAR *misc) {
  if (ClearAirspaceWarnings(true,false)) {
    // airspace was active, enter was used to acknowledge
    return;
  }
  // Else toggle TerrainTopology - and show the results
  MapWindow::Event_TerrainTopology(-1);
  MapWindow::Event_TerrainTopology(0);
}

// Do Clear Airspace warnings
void InputEvents::eventClearAirspaceWarnings(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("day")) == 0) 
	// JMW clear airspace warnings for entire day (for selected airspace)
	ClearAirspaceWarnings(true,true);
  else 
	// default, clear airspace for short acknowledgmenet time
	ClearAirspaceWarnings(true,false);
}

// Do Clear Event Warnings
void InputEvents::eventClearStatusMessages(TCHAR *misc) {
  ClearStatusMessages();
  // TODO: allow selection of specific messages (here we are acknowledging all)
  Message::Acknowledge(0);
}

void InputEvents::eventSelectInfoBox(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("next")) == 0) {
    Event_SelectInfoBox(1);
  }
  if (_tcscmp(misc, TEXT("previous")) == 0) {
    Event_SelectInfoBox(-1);
  }
}


void InputEvents::eventChangeInfoBoxType(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("next")) == 0) {
    Event_ChangeInfoBoxType(1);
  }
  if (_tcscmp(misc, TEXT("previous")) == 0) {
    Event_ChangeInfoBoxType(-1);
  }
}


void InputEvents::eventDoInfoKey(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("up")) == 0) {
    DoInfoKey(1);
  }
  if (_tcscmp(misc, TEXT("down")) == 0) {
    DoInfoKey(-1);
  }
  if (_tcscmp(misc, TEXT("left")) == 0) {
    DoInfoKey(-2);
  }
  if (_tcscmp(misc, TEXT("right")) == 0) {
    DoInfoKey(2);
  }
  if (_tcscmp(misc, TEXT("return")) == 0) {
    DoInfoKey(0);
  }
  
}


void InputEvents::eventMode(TCHAR *misc) {
  InputEvents::setMode(misc);
  
  // trigger redraw of screen to reduce blank area under windows
  MapWindow::RequestFastRefresh = true;
}

void InputEvents::eventMainMenu(TCHAR *misc) {
  // todo: popup main menu
}

void InputEvents::eventStatus(TCHAR *misc) {
  ShowStatus();
}

void InputEvents::eventAnalysis(TCHAR *misc) {
  PopupAnalysis();
}

void InputEvents::eventWaypointDetails(TCHAR *misc) {

  if (_tcscmp(misc, TEXT("current")) == 0) {
    if (SelectedWaypoint<0){
      DoStatusMessage(TEXT("No Active Waypoint!"));
      return;
    }
    LockFlightData();
    PopupWaypointDetails();
    UnlockFlightData();
  } else
  if (_tcscmp(misc, TEXT("select")) == 0) {
    extern int dlgWayPointSelect(void);
    int res;

    if ((res = dlgWayPointSelect()) != -1){
      SelectedWaypoint = res;
      PopupWaypointDetails();
    };

  }
}


void InputEvents::eventStatusMessage(TCHAR *misc) {
	DoStatusMessage(misc);
}

void InputEvents::eventPlaySound(TCHAR *misc) {
	PlayResource(misc);
}


// up, down, auto on, auto off, auto toggle, auto show
void InputEvents::eventMacCready(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("up")) == 0) {
    MacCreadyProcessing(1);
  } else if (_tcscmp(misc, TEXT("down")) == 0) {
    MacCreadyProcessing(-1);
  } else if (_tcscmp(misc, TEXT("auto toggle")) == 0) {
    MacCreadyProcessing(0);
  } else if (_tcscmp(misc, TEXT("auto on")) == 0) {
    MacCreadyProcessing(+2);
  } else if (_tcscmp(misc, TEXT("auto off")) == 0) {
    MacCreadyProcessing(-2);
  } else if (_tcscmp(misc, TEXT("auto show")) == 0) {
    if (CALCULATED_INFO.AutoMacCready) {
      DoStatusMessage(TEXT("Auto MacCready ON"));
    } else {
      DoStatusMessage(TEXT("Auto MacCready OFF"));
    }
  } else if (_tcscmp(misc, TEXT("show")) == 0) {
	TCHAR Temp[100];
	_stprintf(Temp,TEXT("%f"),MACCREADY*LIFTMODIFY);
	DoStatusMessage(TEXT("MacCready Value"), Temp);
  }
}


// TODO Increase wind by larger amounts ? Set wind to specific amount ?
//	(may sound silly - but future may get SMS event that then sets wind)
void InputEvents::eventWind(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("up")) == 0) {
    WindSpeedProcessing(1);
  }
  if (_tcscmp(misc, TEXT("down")) == 0) {
    WindSpeedProcessing(-1);
  }
  if (_tcscmp(misc, TEXT("left")) == 0) {
    WindSpeedProcessing(-2);
  }
  if (_tcscmp(misc, TEXT("right")) == 0) {
    WindSpeedProcessing(2);
  }
  if (_tcscmp(misc, TEXT("save")) == 0) {
    WindSpeedProcessing(0);
  }
}


int jmw_demo=0;

void InputEvents::eventSendNMEA(TCHAR *misc) {
  if (misc) {
    VarioWriteNMEA(misc);
  }
}

void InputEvents::eventAdjustVarioFilter(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("slow")) == 0) {
    VarioWriteNMEA(TEXT("PDVSC,S,VarioTimeConstant,3"));
    return;
  }
  if (_tcscmp(misc, TEXT("medium")) == 0) {
    VarioWriteNMEA(TEXT("PDVSC,S,VarioTimeConstant,2"));
    return;
  }
  if (_tcscmp(misc, TEXT("fast")) == 0) {
    VarioWriteNMEA(TEXT("PDVSC,S,VarioTimeConstant,1"));
    return;
  }
  if (_tcscmp(misc, TEXT("statistics"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,Diagnostics,1"));
    jmw_demo=0;
    return;
  }
  if (_tcscmp(misc, TEXT("diagnostics"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,Diagnostics,2"));
    jmw_demo=0;
    return;
  }
  if (_tcscmp(misc, TEXT("psraw"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,Diagnostics,3"));
    return;
  }
  if (_tcscmp(misc, TEXT("switch"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,Diagnostics,4"));
    return;
  }
  if (_tcscmp(misc, TEXT("democlimb"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,DemoMode,2"));
    jmw_demo=2;
    return;
  }
  if (_tcscmp(misc, TEXT("demostf"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,DemoMode,1"));
    jmw_demo=1;
    return;
  }
  if (_tcscmp(misc, TEXT("zero"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,ZeroASI,1"));
    // zero, no mixing
    return;
  }
  if (_tcscmp(misc, TEXT("save"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,StoreToEeprom,2"));
    return;
  }

  // accel calibration
  if (_tcscmp(misc, TEXT("X1"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,CalibrateAccel,1"));
    return;
  }
  if (_tcscmp(misc, TEXT("X2"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,CalibrateAccel,2"));
    return;
  }
  if (_tcscmp(misc, TEXT("X3"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,CalibrateAccel,3"));
    return;
  }
  if (_tcscmp(misc, TEXT("X4"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,CalibrateAccel,4"));
    return;
  }
  if (_tcscmp(misc, TEXT("X5"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,CalibrateAccel,5"));
    return;
  }

 if (_tcscmp(misc, TEXT("jjj"))==0) {
    VarioWriteNMEA(TEXT("PDVSC,S,ToneCruiseLiftDetectionType,2"));
    return;
 }

}


void InputEvents::eventAudioDeadband(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("+"))) {
    SoundDeadband++;
  }
  if (_tcscmp(misc, TEXT("-"))) {
    SoundDeadband--;
  }
  SoundDeadband = min(40,max(SoundDeadband,0));
  VarioSound_SetVdead(SoundDeadband);
  SaveSoundSettings(); // save to registry

  // JMW TODO send to vario if available

}


void InputEvents::eventAdjustWaypoint(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("next")) == 0) {
    NextUpDown(1); // next
  } else if (_tcscmp(misc, TEXT("nextwrap")) == 0) {
    NextUpDown(2); // next - with wrap
  } else if (_tcscmp(misc, TEXT("previous")) == 0) {
    NextUpDown(-1); // previous
  } else if (_tcscmp(misc, TEXT("previouswrap")) == 0) {
    NextUpDown(-2); // previous with wrap
  }
}

// toggle, abort, resume
void InputEvents::eventAbortTask(TCHAR *misc) {
    LockFlightData();
	if (_tcscmp(misc, TEXT("abort")) == 0)
	    ResumeAbortTask(1);
	else if (_tcscmp(misc, TEXT("resume")) == 0)
	    ResumeAbortTask(-1);
	else if (_tcscmp(misc, TEXT("show")) == 0) {
		if (TaskAborted)
		    DoStatusMessage(TEXT("Task Aborted"));
		else 
		    DoStatusMessage(TEXT("Task Resume"));			
	} else 
	    ResumeAbortTask();  // ToDo arg?
    UnlockFlightData();
}

#include "device.h"
#include "McReady.h"

void InputEvents::eventBugs(TCHAR *misc) {
  double oldBugs = BUGS;
  LockFlightData();

  if (_tcscmp(misc, TEXT("up")) == 0) {
    BUGS = iround(BUGS*100+10) / 100.0;
  } 
  if (_tcscmp(misc, TEXT("down")) == 0) {
    BUGS = iround(BUGS*100-10) / 100.0;
  }
  if (_tcscmp(misc, TEXT("max")) == 0) {
    BUGS= 1.0;
  }
  if (_tcscmp(misc, TEXT("min")) == 0) {
    BUGS= 0.0;
  }
  if (_tcscmp(misc, TEXT("show")) == 0) {
    TCHAR Temp[100];
    _stprintf(Temp,TEXT("%d"), iround(BUGS*100));
    DoStatusMessage(TEXT("Bugs Performance"), Temp);    
  } 
  if (BUGS != oldBugs) {
    BUGS= min(1.0,max(0.5,BUGS));
    
    devPutBugs(devA(), BUGS);
    devPutBugs(devB(), BUGS);
    GlidePolar::SetBallast();
  }
  UnlockFlightData();
}


void InputEvents::eventBallast(TCHAR *misc) {
  double oldBallast= BALLAST;
  LockFlightData();
  if (_tcscmp(misc, TEXT("up")) == 0) {
    BALLAST = iround(BALLAST*100.0+10) / 100.0;
  } 
  if (_tcscmp(misc, TEXT("down")) == 0) {
    BALLAST = iround(BALLAST*100.0-10) / 100.0;
  } 
  if (_tcscmp(misc, TEXT("max")) == 0) {
    BALLAST= 1.0;
  } 
  if (_tcscmp(misc, TEXT("min")) == 0) {
    BALLAST= 0.0;
  } 
  if (_tcscmp(misc, TEXT("show")) == 0) {
    TCHAR Temp[100];
    _stprintf(Temp,TEXT("%d"),iround(BALLAST*100));
    DoStatusMessage(TEXT("Ballast %"), Temp);
  } 
  if (BALLAST != oldBallast) {
    BALLAST=min(1.0,max(0.0,BALLAST));
    devPutBallast(devA(), BALLAST);
    devPutBallast(devB(), BALLAST);
    GlidePolar::SetBallast();
  }
  UnlockFlightData();
}

#include "Task.h"
#include "Logger.h"

void InputEvents::eventLogger(TCHAR *misc) {
  // TODO - start logger without asking
  // start stop toggle addnote
  if (_tcscmp(misc, TEXT("start ask")) == 0) {
    guiStartLogger();
    return;
  } else if (_tcscmp(misc, TEXT("start")) == 0) {
    guiStartLogger(true);
    return;
  } else if (_tcscmp(misc, TEXT("stop ask")) == 0) {
    guiStopLogger();
    return;
  } else if (_tcscmp(misc, TEXT("stop")) == 0) {
    guiStopLogger(true);
    return;
  } else if (_tcscmp(misc, TEXT("toggle ask")) == 0) {
    guiToggleLogger();
    return;
  } else if (_tcscmp(misc, TEXT("toggle")) == 0) {
    guiToggleLogger(true);
    return;
  } else if (_tcscmp(misc, TEXT("show")) == 0) {
    if (LoggerActive) {
      DoStatusMessage(TEXT("Logger ON"));
    } else {
      DoStatusMessage(TEXT("Logger OFF"));
    }
  } else if (_tcsncmp(misc, TEXT("note"), 4)==0) {
    // add note to logger file if available..
    LoggerNote(misc+4);
  }
}


void InputEvents::eventRepeatStatusMessage(TCHAR *misc) {
  // new interface
  // TODO: display only by type specified in misc field
  Message::Repeat(0);
}


void InputEvents::eventNearestAirspaceDetails(TCHAR *misc) {
  double nearestdistance, nearestbearing;
  int foundcircle, foundarea, i;
  bool inside = false;

  TCHAR szMessageBuffer[1024];
  TCHAR szTitleBuffer[1024];
  TCHAR text[1024];

  FindNearestAirspace(GPS_INFO.Longitude, GPS_INFO.Latitude,
		      &nearestdistance, &nearestbearing,
		      &foundcircle, &foundarea);

  if ((foundcircle == -1)&&(foundarea == -1)) {
    // nothing to display!
    return;
  }

  if (foundcircle != -1) {
    i = foundcircle;

    FormatWarningString(AirspaceCircle[i].Type , AirspaceCircle[i].Name , 
			AirspaceCircle[i].Base, AirspaceCircle[i].Top, 
			szMessageBuffer, szTitleBuffer );

  } else if (foundarea != -1) {

    i = foundarea;

    FormatWarningString(AirspaceArea[i].Type , AirspaceArea[i].Name , 
			AirspaceArea[i].Base, AirspaceArea[i].Top, 
			szMessageBuffer, szTitleBuffer );
 }

  if (nearestdistance<0) {
    inside = true;
    nearestdistance = -nearestdistance;
  }

  if (inside) {
    _stprintf(text,TEXT("Inside airspace: %s\r\n%s\r\nExit: %.1f %s\r\nBearing %d"), 
	     szTitleBuffer, 
	     szMessageBuffer,
	     nearestdistance*DISTANCEMODIFY,
	     Units::GetDistanceName(),
	     (int)nearestbearing);
  } else {
    _stprintf(text,TEXT("Nearest airspace: %s\r\n%s\r\nDistance: %.1f %s\r\nBearing %d"), 
	     szTitleBuffer, 
	     szMessageBuffer,
	     nearestdistance*DISTANCEMODIFY,
	     Units::GetDistanceName(),
	     (int)nearestbearing);
  }
    
  // clear previous warning if any
  Message::Acknowledge(MSG_AIRSPACE);

  // TODO No control via status data (ala DoStatusMEssage) - can we change this?
  Message::AddMessage(5000, MSG_AIRSPACE, text);
  
}


void InputEvents::eventNearestWaypointDetails(TCHAR *misc) {
  if (_tcscmp(misc, TEXT("aircraft")) == 0) {
    MapWindow::Event_NearestWaypointDetails(GPS_INFO.Longitude,
					    GPS_INFO.Latitude,
					    1.0e5); // big range..
  }
  // TODO, also allow getting waypoint details at center of screen, or at cursor..

}


void InputEvents::eventNull(TCHAR *misc) {
  // do nothing
}


void InputEvents::eventTaskLoad(TCHAR *misc) {
  LoadNewTask(misc);
}

void InputEvents::eventTaskSave(TCHAR *misc) {
  SaveTask(misc);
}

void dlgBasicSettingsShowModal(void);
void dlgWindSettingsShowModal(void);

void InputEvents::eventSetup(TCHAR *misc) {

  if (_tcscmp(misc,TEXT("Basic"))==0){
    dlgBasicSettingsShowModal();
  } else
  if (_tcscmp(misc,TEXT("Wind"))==0){
    dlgWindSettingsShowModal();
  } else
  if (_tcscmp(misc,TEXT("System"))==0){
    // dlgSystemSettingsShowModal();
  }
}



void InputEvents::eventDLLExecute(TCHAR *misc) {
	// LoadLibrary(TEXT("test.dll"));
	
	TCHAR data[MAX_PATH];
	TCHAR* dll_name;
	TCHAR* func_name;
	TCHAR* other;
	TCHAR* pdest;
	
	_tcscpy(data, misc);

	// dll_name (up to first space)
	pdest = _tcsstr(data, TEXT(" "));
	if (pdest == NULL) {
		#ifdef _INPUTDEBUG_
		_stprintf(input_errors[input_errors_count++], TEXT("Invalid DLLExecute string - no DLL"));
		InputEvents::showErrors();
		#endif
		return;
	}
	*pdest = NULL;
	dll_name = data;

	// func_name (after first space)
	func_name = pdest + 1;

	// other (after next space to end of string)
	pdest = _tcsstr(func_name, TEXT(" "));
	if (pdest != NULL) {
		*pdest = NULL;
		other = pdest + 1;
	} else {
		other = NULL;
	}

	HINSTANCE hinstLib;	// Library pointer
	DLLFUNC_INPUTEVENT lpfnDLLProc;	// Function pointer

	// Load library, find function, execute, unload library
	hinstLib = _loadDLL(dll_name);
	if (hinstLib != NULL) {
		lpfnDLLProc = (DLLFUNC_INPUTEVENT)GetProcAddress(hinstLib, func_name);
		if (lpfnDLLProc != NULL) {
			(*lpfnDLLProc)(other);
	    #ifdef _INPUTDEBUG_
		} else {
			DWORD le;
			le = GetLastError();
			_stprintf(input_errors[input_errors_count++], TEXT("Problem loading function (%s) in DLL (%s) = %d"), func_name, dll_name, le);
			InputEvents::showErrors();
		#endif
		}
	}
}

// Load a DLL (only once, keep a cache of the handle)
//	TODO FreeLibrary - it would be nice to call FreeLibrary before exit on each of these
HINSTANCE _loadDLL(TCHAR *name) {
	int i;
	for (i = 0; i < DLLCache_Count; i++) {
		if (_tcscmp(name, DLLCache[i].text) == 0)
			return DLLCache[i].hinstance;
	}
	if (DLLCache_Count < MAX_DLL_CACHE) {
		DLLCache[DLLCache_Count].hinstance = LoadLibrary(name);
		if (DLLCache[DLLCache_Count].hinstance) {
			DLLCache[DLLCache_Count].text = StringMallocParse(name);
			DLLCache_Count++;

			// First time setup... (should check version numbers etc...)
			DLLFUNC_SETHINST lpfnDLLProc;
			lpfnDLLProc = (DLLFUNC_SETHINST)GetProcAddress(DLLCache[DLLCache_Count - 1].hinstance, TEXT("XCSAPI_SetHInst"));
			if (lpfnDLLProc)
				lpfnDLLProc(GetModuleHandle(NULL));

			return DLLCache[DLLCache_Count - 1].hinstance;
		#ifdef _INPUTDEBUG_
		} else {
			_stprintf(input_errors[input_errors_count++], TEXT("Invalid DLLExecute - not loaded - %s"), name);
			InputEvents::showErrors();
		#endif
		}
	}

	return NULL;
}

// JMW TODO: have all inputevents return bool, indicating whether
// the button should after processing be hilit or not.
// this allows the buttons to indicate whether things are enabled/disabled
// SDP TODO: maybe instead do conditional processing ?

// JMW TODO: make sure when we change things here we also set registry values...
// or maybe have special tag "save" which indicates it should be saved (notice that
// the wind adjustment uses this already, see in Process.cpp)

/* Recently done

   eventTaskLoad		- Load tasks from a file (misc = filename)
   eventTaskSave		- Save tasks to a file (misc = filename)

*/

/* TODO

   eventMainMenu
   eventMenu
   eventBugs - up down max min show // JMW new done
   eventBallast up down max min show // JMW new done
   eventLogger - start/stop/toggle/addnote // JMW new done
   eventClearAirspaceWarnings - // JMW new done
   eventClearStatusMessages - // JMW new done


   eventPanWaypoint		                - Set pan to a waypoint
						- Waypoint could be "next", "first", "last", "previous", or named
						- Note: wrong name - probably just part of eventPan
   eventPressure		- Increase, Decrease, show, Set pressure value
   eventDeclare			- (JMW separate from internal logger)
   eventAirspaceDisplay	- all, below nnn, below me, auto nnn
   eventAirspaceWarnings- on, off, time nn, ack nn
   eventTerrain			- see MapWindow::Event_Terrain
   eventCompass			- on, off, cruise on, crusie off, climb on, climb off
   eventVario			- on, off // JMW what does this do?
   eventOrientation		- north, track,  ???
   eventTerrainRange	        - on, off (might be part of eventTerrain)
   eventSounds			- Include Task and Modes sounds along with Vario
                                    - Include master nn, deadband nn, netto trigger mph/kts/...

*/
