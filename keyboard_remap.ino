// To use this sketch, you must set the board to "Teensy 2.0", the CPU speed to 8 MHz, and the USB
// type to "Serial + Keyboard + Mouse + Joystick". You also need the Teensyduino and USB Host Shield
// 2.0 libraries installed.

#include <hidboot.h>
#include <usbhub.h>

#define CODE_J 0x0D
#define CODE_K 0x0E
#define CODE_L 0x0F
#define CODE_I 0x0C
#define CODE_U 0x18
#define CODE_O 0x12
#define CODE_H 0x0B
#define CODE_N 0x11
#define CODE_Y 0x1c
#define CODE_CAPS 0x39
#define CODE_SEMICOLON 0x33
#define CODE_UP 0x52
#define CODE_DOWN 0x51
#define CODE_SPACE 0x2C

#define MOUSE_DEAD 2
#define MOUSE_SUB 1

bool anything_pressed;

class MouseRptParser : public HIDReportParser {
public:
  virtual void Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

void MouseRptParser::Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf)
{
  MOUSEINFO *pmi = (MOUSEINFO*)buf;

  Mouse.set_buttons(pmi->bmLeftButton, pmi->bmMiddleButton, pmi->bmRightButton);

  if (pmi->bmLeftButton || pmi->bmMiddleButton || pmi->bmRightButton) {
    anything_pressed = true;
  }

  signed char dX = pmi->dX;
  signed char dY = pmi->dY;

  if (dX*dX + dY*dY > MOUSE_DEAD) {
    if (dX > MOUSE_SUB) {
      dX -= MOUSE_SUB;
    } else if (dX < -MOUSE_SUB) {
      dX += MOUSE_SUB;
    } else {
      dX = 0;
    }

    if (dY > MOUSE_SUB) {
      dY -= MOUSE_SUB;
    } else if (dY < -MOUSE_SUB) {
      dY += MOUSE_SUB;
    } else {
      dY = 0;
    }
  } else {
    dX = 0;
    dY = 0;
  }

  Mouse.move(dX, dY);
}

class KbdRptParser : public HIDReportParser {
public:
  KbdRptParser() {
    semicolon_down = false;
    semicolon_down_last = false;
    capslock_down = false;
    capslock_down_last = false;
    semicolon_used = false;
    mods_last = 0;
  };

  virtual void Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);

protected:
  uint8_t RemapModifiers(uint8_t modifiers);
  void SetKey(uint8_t index, uint8_t key);

private:
  bool semicolon_down;
  bool semicolon_down_last;
  bool capslock_down;
  bool capslock_down_last;
  bool semicolon_used;
  uint8_t mods_last;
};

void KbdRptParser::Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf)
{
  // On error - return
  if (buf[2] == 1)
    return;

  semicolon_down = false;
  capslock_down = false;

  uint8_t mods = buf[0];

  mods = RemapModifiers(mods);

  bool any_key_down = false;

  uint8_t free_index = 0;

  for (uint8_t i = 2; i < 8; i++) {
    uint8_t key = buf[i];
    if (semicolon_down_last) {
      switch(key) {
        case CODE_J: key = KEY_LEFT; semicolon_used = true; break;
        case CODE_K: key = KEY_DOWN; semicolon_used = true; break;
        case CODE_L: key = KEY_RIGHT; semicolon_used = true; break;
        case CODE_I: key = KEY_UP; semicolon_used = true; break;
        case CODE_U: key = KEY_HOME; semicolon_used = true; break;
        case CODE_O: key = KEY_END; semicolon_used = true; break;
        case CODE_H: key = KEY_PAGE_UP; semicolon_used = true; break;
        case CODE_N: key = KEY_PAGE_DOWN; semicolon_used = true; break;
        case CODE_Y: key = KEY_BACKSPACE; semicolon_used = true; break;
        case CODE_SPACE: key = KEY_DELETE; semicolon_used = true; break;
      }
    }

    if (key == CODE_CAPS) {
      key = 0; // don't send caps lock
      mods |= MODIFIERKEY_LEFT_CTRL;
      capslock_down = true;
    } else if (key == CODE_SEMICOLON) {
      key = 0; // don't send the semicolon
      semicolon_down = true;
    } else if (key != 0) {
      anything_pressed = true;
      any_key_down = true;
#ifdef DEBUG
      Serial.printf("%x\n", key);
#endif
    }

    if (key == 0) {
      free_index = i;
    }

    SetKey(i, key);
  }

  bool clear_momentary_key = false;

  if (!mods && mods_last && !anything_pressed) {
    // if (mods_last & MODIFIERKEY_LEFT_SHIFT) {
    //   SetKey(free_index, KEY_BACKSPACE);
    //   clear_momentary_key = true;
    //   anything_pressed = true;
    // }

    if (mods_last & MODIFIERKEY_RIGHT_SHIFT) {
      SetKey(free_index, KEY_DELETE);
      clear_momentary_key = true;
      anything_pressed = true;
    }
  }

  if (!semicolon_down && semicolon_down_last && !semicolon_used) {
    SetKey(free_index, KEY_SEMICOLON);
    clear_momentary_key = true;
    anything_pressed = true;
  }

  if (!semicolon_down) {
    semicolon_used = false;
  }

  if (!capslock_down && capslock_down_last && !anything_pressed) {
    SetKey(free_index, KEY_ESC);
    clear_momentary_key = true;
    anything_pressed = true;
  }

  if (!any_key_down && !mods && !semicolon_down && !capslock_down) {
    anything_pressed = false;
  }

  mods_last = mods;
  semicolon_down_last = semicolon_down;
  capslock_down_last = capslock_down;

  Keyboard.set_modifier(mods);

  Keyboard.send_now();

  if (clear_momentary_key) {
    SetKey(free_index, 0);
    Keyboard.send_now();
  }
}

void KbdRptParser::SetKey(uint8_t index, uint8_t key)
{
  switch(index) {
    case 2: Keyboard.set_key1(key); break;
    case 3: Keyboard.set_key2(key); break;
    case 4: Keyboard.set_key3(key); break;
    case 5: Keyboard.set_key4(key); break;
    case 6: Keyboard.set_key5(key); break;
    case 7: Keyboard.set_key6(key); break;
  }
}

uint8_t KbdRptParser::RemapModifiers(uint8_t in_mods)
{
  uint8_t out_mods = 0;

  if (in_mods & MODIFIERKEY_LEFT_CTRL) {
    out_mods |= MODIFIERKEY_LEFT_ALT;
  }

  if (in_mods & MODIFIERKEY_RIGHT_CTRL) {
    out_mods |= MODIFIERKEY_RIGHT_ALT;
  }

  if (in_mods & MODIFIERKEY_LEFT_ALT) {
    out_mods |= MODIFIERKEY_LEFT_CTRL;
  }

  if (in_mods & MODIFIERKEY_RIGHT_ALT) {
    out_mods |= MODIFIERKEY_RIGHT_CTRL;
  }

  if (in_mods & MODIFIERKEY_LEFT_SHIFT) {
    out_mods |= MODIFIERKEY_LEFT_SHIFT;
  }

  if (in_mods & MODIFIERKEY_RIGHT_SHIFT) {
    out_mods |= MODIFIERKEY_RIGHT_SHIFT;
  }

  if (in_mods & MODIFIERKEY_LEFT_GUI) {
    out_mods |= MODIFIERKEY_LEFT_GUI;
  }

  if (in_mods & MODIFIERKEY_RIGHT_GUI) {
    out_mods |= MODIFIERKEY_RIGHT_GUI;
  }

  return out_mods;
}

USB Usb;
USBHub Hub(&Usb);

HIDBoot<HID_PROTOCOL_KEYBOARD | HID_PROTOCOL_MOUSE> HidComposite(&Usb);
HIDBoot<HID_PROTOCOL_KEYBOARD> HidKeyboard(&Usb);
HIDBoot<HID_PROTOCOL_MOUSE> HidMouse(&Usb);

KbdRptParser KbdPrs;
MouseRptParser MousePrs;

void setup()
{
#ifdef DEBUG
  Serial.begin( 115200 );
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
  Serial.println("Start");
#endif

#ifdef DEBUG
  if (Usb.Init() == -1)
    Serial.println("OSC did not start.");
#else
  Usb.Init();
#endif

  delay(200);

  anything_pressed = false;

  HidComposite.SetReportParser(0, (HIDReportParser*)&KbdPrs);
  HidComposite.SetReportParser(1, (HIDReportParser*)&MousePrs);
  HidKeyboard.SetReportParser(0, (HIDReportParser*)&KbdPrs);
  HidMouse.SetReportParser(0, (HIDReportParser*)&MousePrs);
}

void loop()
{
  Usb.Task();
}
