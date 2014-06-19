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
    semicolon_was_pressed = false;
    semicolon_used = false;
    anything_pressed = false;
    capslock_was_pressed = false;
    old_mods = 0;
  };

  virtual void Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);

protected:
  void HandleModifiers(uint8_t modifiers);
  void SetKey(uint8_t index, uint8_t key);

private:
  bool semicolon_down;
  bool semicolon_was_pressed;
  bool semicolon_used;
  bool anything_pressed;
  bool capslock_was_pressed;
  uint8_t old_mods;
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
  uint8_t free_index = 0;
  bool any_key_down = false;

  uint8_t mods = buf[0];

  for (uint8_t i = 2; i < 8; i++) {
    uint8_t key = buf[i];
    if (semicolon_was_pressed) {
      switch(key) {
        case CODE_J: key = KEY_LEFT; semicolon_used = true; break;
        case CODE_K: key = KEY_DOWN; semicolon_used = true; break;
        case CODE_L: key = KEY_RIGHT; semicolon_used = true; break;
        case CODE_I: key = KEY_UP; semicolon_used = true; break;
        case CODE_U: key = KEY_HOME; semicolon_used = true; break;
        case CODE_O: key = KEY_END; semicolon_used = true; break;
        case CODE_H: key = KEY_PAGE_UP; semicolon_used = true; break;
        case CODE_N: key = KEY_PAGE_DOWN; semicolon_used = true; break;
      }
    }

    if (key == 0) {
      free_index = i;
    } else {
      if (key == CODE_CAPS) {
        mods |= MODIFIERKEY_LEFT_ALT;
        capslock_was_pressed = true;
      }

      any_key_down = true;
      Serial.printf("%x\n", key);
    }

    if (key == 0x33) {
      free_index = i;
      SetKey(i, 0); // don't send the semicolon

      semicolon_down = true;
      semicolon_was_pressed = true;
    } else {
      SetKey(i, key);
    }
  }

  if (any_key_down) {
    anything_pressed = true;
  }

  if (!semicolon_down) {
    if (semicolon_was_pressed && !semicolon_used && free_index != 0) {
      // send and then immediately clear the semicolon
      SetKey(free_index, KEY_SEMICOLON);
      Keyboard.send_now();
      SetKey(free_index, 0);
    }

    semicolon_used = false;
    semicolon_was_pressed = false;
  }

  HandleModifiers(mods);

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

void KbdRptParser::HandleModifiers(uint8_t in_mods)
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

  Keyboard.set_modifier(out_mods);

  if (!out_mods && old_mods) {
    if (!anything_pressed) {
      if (old_mods & MODIFIERKEY_LEFT_SHIFT) {
        Keyboard.set_key1(KEY_BACKSPACE);
      }

      if (old_mods & MODIFIERKEY_RIGHT_SHIFT) {
        Keyboard.set_key1(KEY_DELETE);
      }

      Keyboard.send_now();
      Keyboard.set_key1(0);
    }

    // reset anything pressed
    anything_pressed = false;
  }

  old_mods = out_mods;
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
