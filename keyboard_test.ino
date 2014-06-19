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
#define CODE_CAPS 0x39
#define CODE_SEMICOLON 0x33

class MouseRptParser : public HIDReportParser {
public:
  virtual void Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

void MouseRptParser::Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf)
{
  MOUSEINFO *pmi = (MOUSEINFO*)buf;

  Mouse.move(pmi->dX, pmi->dY);
  Mouse.set_buttons(pmi->bmLeftButton, pmi->bmMiddleButton, pmi->bmRightButton);
}

class KbdRptParser : public HIDReportParser {
public:
  KbdRptParser() {
    semicolon_down = false;
    semicolon_down_last = false;
    capslock_down = false;
    capslock_down_last = false;
    anything_pressed = false;
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
  bool anything_pressed;
  uint8_t mods_last;
};

void KbdRptParser::Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf)
{
  // On error - return
  if (buf[2] == 1)
    return;

  // KBDINFO *pki = (KBDINFO*)buf;

  // Keyboard.set_key1(buf[2]);
  // Keyboard.set_key2(buf[3]);
  // Keyboard.set_key3(buf[4]);
  // Keyboard.set_key4(buf[5]);
  // Keyboard.set_key5(buf[6]);
  // Keyboard.set_key6(buf[7]);

  semicolon_down = false;
  capslock_down = false;

  uint8_t mods = buf[0];

  mods = RemapModifiers(mods);

  bool any_key_down = false;

  for (uint8_t i = 2; i < 8; i++) {
    uint8_t key = buf[i];
    if (semicolon_down_last) {
      switch(key) {
        case CODE_J: key = KEY_LEFT; break;
        case CODE_K: key = KEY_DOWN; break;
        case CODE_L: key = KEY_RIGHT; break;
        case CODE_I: key = KEY_UP; break;
        case CODE_U: key = KEY_HOME; break;
        case CODE_O: key = KEY_END; break;
        case CODE_H: key = KEY_PAGE_UP; break;
        case CODE_N: key = KEY_PAGE_DOWN; break;
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
      // Serial.printf("%x\n", key);
    }

    SetKey(i, key);
  }

  if (!mods && mods_last && !anything_pressed) {
    if (mods_last & MODIFIERKEY_LEFT_SHIFT) {
      Keyboard.set_key1(KEY_BACKSPACE);
      Keyboard.send_now();
    }

    if (mods_last & MODIFIERKEY_RIGHT_SHIFT) {
      Keyboard.set_key1(KEY_DELETE);
      Keyboard.send_now();
    }

    // clear the delete
    Keyboard.set_key1(0);
  }

  if (!semicolon_down && semicolon_down_last && !anything_pressed) {
    // send and then immediately clear the semicolon
    Keyboard.set_key1(KEY_SEMICOLON);
    Keyboard.send_now();
    Keyboard.set_key1(0);
  }

  if (!capslock_down && capslock_down_last && !anything_pressed) {
    // send and then immediately clear the escape
    Keyboard.set_key1(KEY_ESC);
    Keyboard.send_now();
    Keyboard.set_key1(0);
  }

  if (!any_key_down && !mods && !semicolon_down && !capslock_down) {
    anything_pressed = false;
  }

  mods_last = mods;
  semicolon_down_last = semicolon_down;
  capslock_down_last = capslock_down;

  if (anything_pressed) {
    Keyboard.set_modifier(mods);
  } else {
    Keyboard.set_modifier(0);
  }

  Keyboard.send_now();
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
  uint8_t out_mods;

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

//uint32_t next_time;

KbdRptParser KbdPrs;
MouseRptParser MousePrs;

void setup()
{
  Serial.begin( 115200 );
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
  Serial.println("Start");

  if (Usb.Init() == -1)
    Serial.println("OSC did not start.");

  delay( 200 );

  //next_time = millis() + 5000;

  HidComposite.SetReportParser(0, (HIDReportParser*)&KbdPrs);
  HidComposite.SetReportParser(1,(HIDReportParser*)&MousePrs);
  HidKeyboard.SetReportParser(0, (HIDReportParser*)&KbdPrs);
  HidMouse.SetReportParser(0,(HIDReportParser*)&MousePrs);
}

void loop()
{
    Usb.Task();
}
