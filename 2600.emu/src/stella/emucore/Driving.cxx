//============================================================================
//
//   SSSS    tt          lll  lll       
//  SS  SS   tt           ll   ll        
//  SS     tttttt  eeee   ll   ll   aaaa 
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2012 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: Driving.cxx 2405 2012-03-04 19:20:29Z stephena $
//============================================================================

#include "Event.hxx"
#include "System.hxx"

#include "Driving.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Driving::Driving(Jack jack, const Event& event, const System& system)
  : Controller(jack, event, system, Controller::Driving),
    myCounter(0),
    myControlID(-1),
    myControlIDX(-1),
    myControlIDY(-1)
{
  if(myJack == Left)
  {
    myCCWEvent   = Event::JoystickZeroLeft;
    myCWEvent    = Event::JoystickZeroRight;
    myFireEvent  = Event::JoystickZeroFire;
    myXAxisValue = Event::SALeftAxis0Value;
    myYAxisValue = Event::SALeftAxis1Value;
  }
  else
  {
    myCCWEvent   = Event::JoystickOneLeft;
    myCWEvent    = Event::JoystickOneRight;
    myFireEvent  = Event::JoystickOneFire;
    myXAxisValue = Event::SARightAxis0Value;
    myYAxisValue = Event::SARightAxis1Value;
  }

  // Digital pins 3 and 4 are not connected
  myDigitalPinState[Three] = myDigitalPinState[Four] = true;

  // Analog pins are not connected
  myAnalogPinValue[Five] = myAnalogPinValue[Nine] = maximumResistance;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Driving::~Driving()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Driving::update()
{
  // Make sure direct gray codes from Stelladaptor stay in sync with
  // simulated gray codes generated by PC keyboard or PC joystick
  myCounter = (myGrayIndex << 2) | (myCounter & 3);
  
  // Digital events (from keyboard or joystick hats & buttons)
  myDigitalPinState[Six] = (myEvent.get(myFireEvent) == 0);
  int d_axis = myEvent.get(myXAxisValue);
  if(myEvent.get(myCCWEvent) != 0 || d_axis < -16384)     myCounter--;
  else if(myEvent.get(myCWEvent) != 0 || d_axis > 16384)  myCounter++;

  // Mouse motion and button events
  if(myControlID > -1)
  {
    int m_axis = myEvent.get(Event::MouseAxisXValue);
    if(m_axis < -2)     myCounter--;
    else if(m_axis > 2) myCounter++;
    if(myEvent.get(Event::MouseButtonLeftValue) ||
       myEvent.get(Event::MouseButtonRightValue))
      myDigitalPinState[Six] = false;
  }
  else
  {
    // Test for 'untied' mouse axis mode, where each axis is potentially
    // mapped to a separate driving controller
    if(myControlIDX > -1)
    {
      int m_axis = myEvent.get(Event::MouseAxisXValue);
      if(m_axis < -2)     myCounter--;
      else if(m_axis > 2) myCounter++;
      if(myEvent.get(Event::MouseButtonLeftValue))
        myDigitalPinState[Six] = false;
    }
    if(myControlIDY > -1)
    {
      int m_axis = myEvent.get(Event::MouseAxisYValue);
      if(m_axis < -2)     myCounter--;
      else if(m_axis > 2) myCounter++;
      if(myEvent.get(Event::MouseButtonRightValue))
        myDigitalPinState[Six] = false;
    }
  }

  // Only consider the lower-most bits (corresponding to pins 1 & 2)
  myCounter &= 0x0f;
  myGrayIndex = myCounter >> 2;
 
  // Stelladaptor is the only controller that should set this
  int yaxis = myEvent.get(myYAxisValue);

  // Only overwrite gray code when Stelladaptor input has changed
  // (that means real changes, not just analog signal jitter)
  if((yaxis < (myLastYaxis - 1024)) || (yaxis > (myLastYaxis + 1024)))
  {
    myLastYaxis = yaxis;
    if(yaxis <= -16384-4096)
      myGrayIndex = 3; // up
    else if(yaxis > 16384+4096)
      myGrayIndex = 1; // down
    else if(yaxis >= 16384-4096)
      myGrayIndex = 2; // up + down
    else if(yaxis < 16384-4096)
      myGrayIndex = 0; // no movement
  }

  // Gray codes for rotation
  static const uInt8 graytable[] = { 0x03, 0x01, 0x00, 0x02 };

  // Determine which bits are set
  uInt8 gray = graytable[myGrayIndex];
  myDigitalPinState[One] = (gray & 0x1) != 0;
  myDigitalPinState[Two] = (gray & 0x2) != 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Driving::setMouseControl(
    MouseControl::Axis xaxis, MouseControl::Axis yaxis, int ctrlID)
{
  // In 'automatic' mode, only the X-axis is used
  if(xaxis == MouseControl::Automatic || yaxis == MouseControl::Automatic)
  {
    myControlID = ((myJack == Left && ctrlID == 0) ||
                   (myJack == Right && ctrlID == 1)
                  ) ? ctrlID : -1;
    myControlIDX = myControlIDY = -1;
  }
  else
  {
    // The following is somewhat complex, but we need to pre-process as much
    // as possible, so that ::update() can run quickly
    myControlID = -1;
    if(myJack == Left)
    {
      myControlIDX = xaxis == MouseControl::Driving0 ? 0 : -1;
      myControlIDY = yaxis == MouseControl::Driving0 ? 0 : -1;
    }
    else  // myJack == Right
    {
      myControlIDX = xaxis == MouseControl::Driving1 ? 1 : -1;
      myControlIDY = yaxis == MouseControl::Driving1 ? 1 : -1;
    }
  }
}
