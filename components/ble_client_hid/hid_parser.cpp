

#include <stack>
#include <map>
#include <bitset>

#include "HIDReportData.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "hid_parser.h"

namespace esphome
{
  namespace ble_client_hid
  {

    static const char *const TAG = "hid_parser";

    // requires at least C++11
    const std::string vformat(const char *const zcFormat, ...)
    {

      // initialize use of the variable argument array
      va_list vaArgs;
      va_start(vaArgs, zcFormat);

      // reliably acquire the size
      // from a copy of the variable argument array
      // and a functionally reliable call to mock the formatting
      va_list vaArgsCopy;
      va_copy(vaArgsCopy, vaArgs);
      const int iLen = std::vsnprintf(NULL, 0, zcFormat, vaArgsCopy);
      va_end(vaArgsCopy);

      // return a formatted string without risking memory mismanagement
      // and without assuming any compiler or platform specific behavior
      std::vector<char> zc(iLen + 1);
      std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
      va_end(vaArgs);
      return std::string(zc.data(), iLen);
    }
    void HIDReportMap::esp_logd_report_map(const uint8_t *report_map_data, uint16_t report_map_size)
    {
      ESP_LOGD(TAG, "Report Map:");
      while (report_map_size > 0)
      {
        uint8_t report_item_info = report_map_data[0];
        report_map_data++;
        switch (report_item_info & HID_RI_DATA_SIZE_MASK)
        {
        case HID_RI_DATA_BITS_32:
          ESP_LOGD(TAG, "%X, %X, %X, %X, %X", report_item_info, report_map_data[0], report_map_data[1], report_map_data[2], report_map_data[3]);
          report_map_data += 4;
          report_map_size -= 4;
          break;
        case HID_RI_DATA_BITS_16:
          ESP_LOGD(TAG, "%X, %X, %X", report_item_info, report_map_data[0], report_map_data[1]);
          report_map_data += 2;
          report_map_size -= 2;
          break;
        case HID_RI_DATA_BITS_8:
          ESP_LOGD(TAG, "%X, %X", report_item_info, report_map_data[0]);
          report_map_data += 1;
          report_map_size -= 1;
          break;
        case HID_RI_DATA_BITS_0:
          ESP_LOGD(TAG, "%X", report_item_info);
          break;
        }
      }
    }

    int32_t HIDReportMap::parse_item(const uint8_t **p_report_map_data, uint16_t *report_map_size, uint8_t report_item_info)
    {
      uint32_t report_item_data;

      switch (report_item_info & HID_RI_DATA_SIZE_MASK)
      {
      case HID_RI_DATA_BITS_32:
        report_item_data =
            (((uint32_t)(*p_report_map_data)[3] << 24) |
             ((uint32_t)(*p_report_map_data)[2] << 16) |
             ((uint16_t)(*p_report_map_data)[1] << 8) | (*p_report_map_data)[0]);
        (*report_map_size) -= 4;
        (*p_report_map_data) += 4;
        return report_item_data;

      case HID_RI_DATA_BITS_16:
        report_item_data =
            (((uint16_t)(*p_report_map_data)[1] << 8) | ((*p_report_map_data)[0]));
        (*report_map_size) -= 2;
        (*p_report_map_data) += 2;
        return report_item_data;

      case HID_RI_DATA_BITS_8:
        report_item_data = (*p_report_map_data)[0];
        (*report_map_size) -= 1;
        (*p_report_map_data) += 1;
        return report_item_data;

      default:
        report_item_data = 0;
        return report_item_data;
      }
    }

    static const HIDUsage parse_usage(uint32_t data, uint16_t usage_page)
    {
      if ((data & HID_RI_DATA_SIZE_MASK) == HID_RI_DATA_BITS_32)
      {
        return HIDUsage((uint16_t)data, (uint16_t)(data >> 16));
      }
      return HIDUsage((uint16_t)data, usage_page);
    }

    const HIDUsage HIDUsageRange::get_usage(uint16_t index) const
    {
      if (index > this->usage_max.usage - this->usage_min.usage)
      {
        ESP_LOGW(TAG, "Usage index out of range");
        return HIDUsage(0,0);
      }
      return HIDUsage(this->usage_min.usage + index, this->usage_page);
    }

    const HIDUsage HIDUsageList::get_usage(uint16_t index) const
    {
      if (index > this->usages.size())
      {
        ESP_LOGW(TAG, "Usage index out of range");
        return HIDUsage(0,0);;
      }
      return this->usages[index];
    }

    HIDReportMap *HIDReportMap::parse_report_map_data(
        const uint8_t *report_map_data, uint16_t report_map_size)
    {
      HIDStateTable state_table = {};
      std::stack<HIDStateTable> parser_states;
      HIDUsageRangeLimits usage_range = {};
      std::vector<HIDUsage> usages;
      std::map<uint8_t, HIDInputReport *> input_reports;

      while (report_map_size)
      {
        uint8_t report_item_info = report_map_data[0];

        report_map_data++;
        report_map_size--;

        uint32_t report_item_data = HIDReportMap::parse_item(&report_map_data, &report_map_size, report_item_info);
        switch (report_item_info & (HID_RI_TYPE_MASK | HID_RI_TAG_MASK))
        {
        case HID_RI_PUSH(0):
        {

          parser_states.push(state_table);
          break;
        }
        case HID_RI_POP(0):
        {
          if (parser_states.size() <= 0)
          {
            ESP_LOGW(TAG,
                     "No parser state in HID parser states stack, error in HID "
                     "report map");
            return nullptr;
          }
          state_table = parser_states.top();
          parser_states.pop();
          break;
        }

        case HID_RI_USAGE_PAGE(0):
        {
          state_table.usage_page = report_item_data;
          break;
        }

        case HID_RI_LOGICAL_MINIMUM(0):
        {
          state_table.logical_range.minimum = report_item_data;
          break;
        }

        case HID_RI_LOGICAL_MAXIMUM(0):
        {
          state_table.logical_range.maximum = report_item_data;
          break;
        }

        case HID_RI_PHYSICAL_MINIMUM(0):
          // Ignore for now
          break;

        case HID_RI_PHYSICAL_MAXIMUM(0):
          // Ignore for now
          break;

        case HID_RI_UNIT_EXPONENT(0):
          // Ignore for now
          break;

        case HID_RI_UNIT(0):
          // Ignore for now
          break;

        case HID_RI_REPORT_SIZE(0):
        {
          state_table.report_size = report_item_data;
          break;
        }

        case HID_RI_REPORT_COUNT(0):
        {
          state_table.report_count = report_item_data;
          break;
        }

        case HID_RI_REPORT_ID(0):
        {
          if (input_reports.count(report_item_data) == 0)
          {
            input_reports.emplace(report_item_data, new HIDInputReport(report_item_data));
          }
          state_table.report_id = report_item_data;
          break;
        }

        case HID_RI_USAGE(0):
        {
          usages.push_back(parse_usage(report_item_data, state_table.usage_page));
          break;
        }

        case HID_RI_USAGE_MINIMUM(0):
        {
          usage_range.minimum = parse_usage(report_item_data, state_table.usage_page);
          break;
        }

        case HID_RI_USAGE_MAXIMUM(0):
        {
          usage_range.maximum = parse_usage(report_item_data, state_table.usage_page);
          break;
        }

        case HID_RI_COLLECTION(0):
          // Ignore for now
          break;

        case HID_RI_END_COLLECTION(0):
          // Ignore for now
          break;

        case HID_RI_INPUT(0):

        {
          ESP_LOGD(TAG, "Found input main item");
          uint16_t item_flags = report_item_data;

          if (state_table.report_id == 0)
          {
            if (input_reports.count(0) == 0)
            {
              ESP_LOGD(TAG, "Not using report ids");
              input_reports.emplace(0, new HIDInputReport(0));
            }
          }

          HIDInputReport *input_report = input_reports.at(state_table.report_id);
          if (item_flags & HID_IOF_CONSTANT)
          {
            ESP_LOGD(TAG, "Parsed input report item of type: constant");
            input_report->add_padding(state_table.report_size);
            break;
          }
          HIDUsageCollection *usage_collection;
          if (usages.size() > 0)
          {
            usage_collection = new HIDUsageList(usages);
          }
          else
          {
            ESP_LOGD(TAG, "Creating usage range with min: %d, max: %d, page: %d", usage_range.minimum.usage, usage_range.maximum.usage, usage_range.minimum.page);
            usage_collection = new HIDUsageRange(usage_range.minimum, usage_range.maximum, usage_range.minimum.page);
          }
          if (item_flags & HID_IOF_VARIABLE)
          {
            input_report->push_back(new HIDInputReportVariable(usage_collection, state_table.report_count, state_table.report_id, state_table.logical_range, state_table.report_size, input_report->get_next_offset()));
            ESP_LOGD(TAG, "Parsed input report item of type: variable, report size: %d, report count: %d, report id: %d", state_table.report_size, state_table.report_count, state_table.report_id);
          }
          else
          {
            input_report->push_back(new HIDInputReportArray(usage_collection, state_table.report_count, state_table.report_id, state_table.logical_range, state_table.report_size, input_report->get_next_offset()));
            ESP_LOGD(TAG, "Parsed input report item of type: array, report size: %d, report count: %d, report id: %d", state_table.report_size, state_table.report_count, state_table.report_id);
          }
          break;
        }
        case HID_RI_OUTPUT(0):
          // Ignore for now
          break;
        case HID_RI_FEATURE(0):
          // Ignore for now
          break;

        default:
          break;
        }
        if ((report_item_info & HID_RI_TYPE_MASK) == HID_RI_TYPE_MAIN)
        {
          usages.clear();
          usage_range.maximum = HIDUsage(0, 0);
          usage_range.minimum = HIDUsage(0, 0);
        }
      }
      HIDReportMap *report_map = new HIDReportMap(input_reports);
      ESP_LOGD(TAG, "Parsed report map with %d input reports", input_reports.size());
      return report_map;
    }

    uint8_t HIDInputReport::get_next_offset()
    {
      return this->report_size;
    }

    void HIDInputReport::add_padding(uint8_t padding_size)
    {
      this->report_size += padding_size;
    }

    void HIDInputReport::push_back(HIDInputReportItem *item)
    {
      this->items.push_back(item);
      this->report_size += item->report_size;
    }

    std::vector<HIDReportItemValue> HIDReportMap::parse(uint8_t *hid_report_data)
    {
      if (this->input_reports.empty())
      {
        ESP_LOGW(TAG, "No input reports found");
        return std::vector<HIDReportItemValue>();
      }
      if (this->input_reports.count(0) == 0)
      {
        ESP_LOGD(TAG, "Parsing HID report with report ID (%d)", hid_report_data[0]);
        uint8_t report_id = hid_report_data[0];
        hid_report_data++;
        return this->input_reports.at(report_id)->parse(hid_report_data);
      }
      ESP_LOGD(TAG, "Parsing HID report without report ID");
      return this->input_reports.at(0)->parse(hid_report_data);
    }

    std::vector<HIDReportItemValue> HIDInputReport::parse(uint8_t *report_data)
    {
      std::vector<HIDReportItemValue> report_values;
      for (HIDInputReportItem *report_item : this->items)
      {
        std::vector<HIDReportItemValue> item_values = report_item->parse(report_data);
        for (HIDReportItemValue item_value : item_values)
        {
          report_values.push_back(item_value);
        }
      }
      return report_values;
    }

    int32_t HIDInputReportItem::parse_input_report_item(uint8_t *report_data, uint16_t bit_offset, uint16_t report_size, HIDLogicalRange logical_range)
    {
      int32_t value = 0;
      uint16_t data_bits_remaining = report_size;
      uint16_t current_bit = bit_offset;
      uint32_t bit_mask = (1 << 0);
      bool negative_range = logical_range.minimum < 0 || logical_range.maximum < 0;
      // scan through report data
      while (data_bits_remaining--)
      {
        if (report_data[current_bit / 8] & (1 << (current_bit % 8)))
        {
          if (negative_range && data_bits_remaining == 0)
          {
            value -= 1 << (current_bit - bit_offset);
          }
          else
          {
            value |= bit_mask;
          }
        }
        bit_mask <<= 1;
        current_bit++;
      }
      return value;
    }

    std::string HIDReportItemValue::to_string() const
    {
      return vformat("HIDReportItemValue(usage_page: %d, usage: %d, value: %d)",this->usage.page, this->usage.usage, this->value);
    }

    std::vector<HIDReportItemValue> HIDInputReportVariable::parse(uint8_t *report_data)
    {
      std::vector<HIDReportItemValue> values;
      for (uint8_t i = 0; i < this->report_count; i++)
      {
        int32_t value = parse_input_report_item(report_data, this->report_offset + i * this->report_size, this->report_size, this->logical_range);
        if (value > this->logical_range.maximum || value < this->logical_range.minimum)
        {
          ESP_LOGD(TAG, "Value out of range");
          continue;
        }
        if (this->last_values[i].raw_value == value)
          continue;
        values.push_back(HIDReportItemValue(this->usage_collection->get_usage(i), value, value));
        ESP_LOGD(TAG, values.back().to_string().c_str());

        this->last_values[i] = values.back();
      }
      return values;
    }

    std::vector<HIDReportItemValue> HIDInputReportArray::parse(uint8_t *report_data)
    {
      std::vector<HIDReportItemValue> values;

      for (uint8_t i = 0; i < this->report_count; i++)
      {
        int32_t value = parse_input_report_item(report_data, this->report_offset + i * this->report_size, this->report_size, this->logical_range);
        if (value > this->logical_range.maximum || value < this->logical_range.minimum)
        {
          ESP_LOGD(TAG, "Value out of range");
          continue;
        }
        if (value == 0)
        {
          continue;
        }
        values.push_back(HIDReportItemValue(this->usage_collection->get_usage(value), 1, value));
        ESP_LOGD(TAG, values.back().to_string().c_str());
      }

      for (HIDReportItemValue last_value : this->last_values)
      {
        if (std::count(values.begin(), values.end(), last_value) == 0)
        {
          values.push_back(HIDReportItemValue(last_value.usage, 0, last_value.raw_value));
          ESP_LOGD(TAG, values.back().to_string().c_str());
        }
      }

      this->last_values.clear();
      for (HIDReportItemValue value : values)
      {
        if (value.value != 0)
        {
          this->last_values.push_back(value);
        }
      }
      return values;
    }
  } // namespace ble_client_hid
} // namespace esphome