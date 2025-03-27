#include "usages.h"
#include "generic_desktop.h"
#include "keyboard_keypad.h"
#include "consumer.h"
#include "g7bts_generic_one.h"
#include "g7bts_generic_two.h"
#include "g7bts_generic_three.h"
#include "g7bts_generic_four.h"
#include "g7bts_ir.h"

const std::map<uint8_t, const UsagePage> USAGE_PAGES {
  {0x01, USAGE_PAGE_GENERIC_DESKTOP},
  {0x07, USAGE_PAGE_KEYBOARD_KEYPAD},
  {0x0C, USAGE_PAGE_CONSUMER},
  {0xC0, USAGE_PAGE_G7BTS_GENERIC_ONE},
  {0xBF, USAGE_PAGE_G7BTS_GENERIC_TWO},
  {0xBE, USAGE_PAGE_G7BTS_GENERIC_THREE},
  {0xBD, USAGE_PAGE_G7BTS_GENERIC_FOUR},
  {0xC2, USAGE_PAGE_G7BTS_IR},
};