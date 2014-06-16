#include <hidboot.h>
#include <usbhub.h>

class MouseRptParser : public HIDReportParser {
public:
  virtual void Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};

void MouseRptParser::Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
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
  };

  virtual void Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);

protected:
  void HandleModifiers(uint8_t modifiers);
  void SetKey(uint8_t index, uint8_t key);

private:
  bool semicolon_down;
  bool semicolon_was_pressed;
  bool semicolon_used;
};

void KbdRptParser::Parse(HID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf)
{
  // On error - return
  if (buf[2] == 1)
    return;

  // KBDINFO *pki = (KBDINFO*)buf;

  HandleModifiers(buf[0]);

  // Keyboard.set_key1(buf[2]);
  // Keyboard.set_key2(buf[3]);
  // Keyboard.set_key3(buf[4]);
  // Keyboard.set_key4(buf[5]);
  // Keyboard.set_key5(buf[6]);
  // Keyboard.set_key6(buf[7]);

  semicolon_down = false;
  uint8_t free_index = 0;

  for (uint8_t i = 2; i < 8; i++) {
    uint8_t key = buf[i];
    if (semicolon_was_pressed) {
      switch(key) {
        case 0xD: key = KEY_LEFT; semicolon_used = true; break;
        case 0xE: key = KEY_DOWN; semicolon_used = true; break;
        case 0xF: key = KEY_RIGHT; semicolon_used = true; break;
        case 0xC: key = KEY_UP; semicolon_used = true; break;
        case 0x18: key = KEY_HOME; semicolon_used = true; break;
        case 0x12: key = KEY_END; semicolon_used = true; break;
        case 0xB: key = KEY_PAGE_UP; semicolon_used = true; break;
        case 0x11: key = KEY_PAGE_DOWN; semicolon_used = true; break;
      }
    }

    if (key == 0) {
      free_index = i;
    }

    if (key == 0x33) {
      free_index = i;
      SetKey(i, 0);

      semicolon_down = true;
      semicolon_was_pressed = true;
      // Serial.println("semicolon");
    } else {
      SetKey(i, key);
      // if (key != 0)
      //   Serial.printf("pressed %x\n", key);
      // Serial.printf("semicolo %x\n", KEY_SEMICOLON);
    }
  }

  if (!semicolon_down) {
    if (semicolon_was_pressed && !semicolon_used && free_index != 0) {
      SetKey(free_index, KEY_SEMICOLON);
      Keyboard.send_now();

      // After we send the semicolon
      for (uint8_t i = 2; i < 8; i++) {
        SetKey(i, 0);
      }
    }

    semicolon_used = false;
    semicolon_was_pressed = false;
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

void KbdRptParser::HandleModifiers(uint8_t mods)
{
  MODIFIERKEYS mod;
  *((uint8_t*)&mod) = mods;

  uint8_t modifiers = 0;

  if (mod.bmLeftCtrl) {
    modifiers |= MODIFIERKEY_LEFT_ALT;
  }

  if (mod.bmRightCtrl) {
    modifiers |= MODIFIERKEY_RIGHT_ALT;
  }

  if (mod.bmLeftAlt) {
    modifiers |= MODIFIERKEY_RIGHT_CTRL;
  }

  if (mod.bmRightAlt) {
    modifiers |= MODIFIERKEY_LEFT_CTRL;
  }

  if (mod.bmLeftShift) {
    modifiers |= MODIFIERKEY_LEFT_SHIFT;
  }

  if (mod.bmRightShift) {
    modifiers |= MODIFIERKEY_RIGHT_SHIFT;
  }

  if (mod.bmLeftGUI) {
    modifiers |= MODIFIERKEY_LEFT_GUI;
  }

  if (mod.bmRightGUI) {
    modifiers |= MODIFIERKEY_RIGHT_GUI;
  }

  Keyboard.set_modifier(modifiers);
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
