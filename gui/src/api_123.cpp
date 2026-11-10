/**************************************************************************
 *   Copyright (C) 2026 by Jan Rheinländer                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   along with this program; if not, see <https://www.gnu.org/licenses/>. *
 **************************************************************************/

/**
 * \file
 *
 * ocpn_plugin.h HostApi123 implementation
 */

#include <ocpn_plugin.h>
#include <grib_record_set.h>
#include "grib_record.h"

namespace {
// This class is required for the GribRecord constructor
class GribSolRecord : public GribRecord {
public:
  GribSolRecord(const timestamp issue_time, const timestamp frame_time,
                const double lat_min, const double lon_min,
                const double lat_max, const double lon_max, const int lat_num,
                const int lon_num, const double lat_inc, const double lon_inc,
                const HostApi123::MeteoRecord::MeteoParameters type,
                double* p_data)
      : GribRecord() {
    ref_date = issue_time.time_since_epoch().count();
    std::time_t tt = std::chrono::system_clock::to_time_t(issue_time);
    std::tm utc_tm = *std::gmtime(&tt);
    refyear = utc_tm.tm_year;
    refmonth = utc_tm.tm_mon;
    refday = utc_tm.tm_mday;
    refhour = utc_tm.tm_hour;
    refminute = utc_tm.tm_min;
    cur_date = frame_time.time_since_epoch().count();
    period_p1 = cur_date;
    period_p2 = cur_date + 3 * 60 * 60; // 3 hours, TODO pass as parameter?
    periodsec = cur_date - ref_date;
    Ni = lon_num;
    Nj = lat_num;
    this->lat_min = lat_min;
    this->lon_min = lon_min;
    this->lat_max = lat_max;
    this->lon_max = lon_max;
    La1 = lat_min;
    Lo1 = lon_min;
    La2 = lat_max;
    Lo2 = lon_max;
    Di = lon_inc;
    Dj = lat_inc;

    switch (type) {
      case HostApi123::MeteoRecord::WIND_VX:
        data_type = GRB_WIND_VX; break;
      case HostApi123::MeteoRecord::WIND_VY:
        data_type = GRB_WIND_VY; break;
      case HostApi123::MeteoRecord::CURRENT_VX:
        data_type = GRB_UOGRD; break;
      case HostApi123::MeteoRecord::CURRENT_VY:
        data_type = GRB_VOGRD; break;
      case HostApi123::MeteoRecord::PRECIPITATION:
        data_type = GRB_PRECIP_TOT; break;
      case HostApi123::MeteoRecord::CLOUD_COVER:
        data_type = GRB_CLOUD_TOT; break;
      case HostApi123::MeteoRecord::AIR_TEMP:
        data_type = GRB_TEMP; break;
      case HostApi123::MeteoRecord::SEA_TEMP:
        data_type = GRB_WTMP; break;
      case HostApi123::MeteoRecord::CAPE:
        data_type = GRB_CAPE; break;
      case HostApi123::MeteoRecord::RADAR_REFL:
        data_type = GRB_COMP_REFL; break;
      case HostApi123::MeteoRecord::WAVE_HEIGHT:
        data_type = GRB_HTSGW; break;
      case HostApi123::MeteoRecord::WAVE_DIRECTION:
        data_type = GRB_WVDIR; break;
      case HostApi123::MeteoRecord::WAVE_PERIOD:
        data_type = GRB_WVPER; break;
      case HostApi123::MeteoRecord::PRESSURE:
        data_type = GRB_PRESSURE; break;
      case HostApi123::MeteoRecord::HUMIDITY_REL:
        data_type = GRB_HUMID_REL; break;
      case HostApi123::MeteoRecord::GEOPOT_HEIGHT:
        data_type = GRB_GEOPOT_HGT; break;
      default:
        data_type = -1;
    }
    level_type = LV_ABOV_GND; // TODO extract from data
    level_value = 0;

    data = p_data;
    hasBMS = false;
    bms_bits = nullptr;
    is_filled = true; // TODO Pass as flag, SOL records are always filled
    ok = true;
  }

  virtual ~GribSolRecord() override {}
};
}  // namespace

// TODO Is there a possible memory crash in the existing
// GRIB_TIMELINE_RECORD_REQUEST because it transfers a copy
// of the GribRecordSet pointer, both copies having ownership of the data?

HostApi123::MeteoRecord::MeteoRecord(GribRecord* p_record) {
  m_pRecord = new GribRecord{*p_record};
}

HostApi123::MeteoRecord::MeteoRecord(
    const timestamp issue_time, const timestamp frame_time,
    const double lat_min, const double lon_min, const double lat_max,
    const double lon_max, const int lat_num, const int lon_num,
    const double lat_inc, const double lon_inc,
    const HostApi123::MeteoRecord::MeteoParameters type, double* p_data) {
  m_pRecord = new GribSolRecord{issue_time, frame_time, lat_min, lon_min,
                                lat_max,    lon_max,    lat_num, lon_num,
                                lat_inc,    lon_inc,    type,    p_data};
}

HostApi123::MeteoRecord::~MeteoRecord(void) {
  // Will delete m_pRecord->data
}

timestamp HostApi123::MeteoRecord::GetFrameTime() const {
  return timestamp(std::chrono::seconds(m_pRecord->GetRecordCurrentDate()));
}

GribRecord* HostApi123::MeteoRecord::AsGribRecord() {
  GribRecord* result = m_pRecord;
  m_pRecord = nullptr;
  return result;
}

HostApi123::MeteoRecordSet::MeteoRecordSet(GribRecordSet* p_set) {
  for (int p = 0; p < Idx_COUNT; ++p) {
    GribRecord* p_record = p_set->m_GribRecordPtrArray[p];
    if (p_record != nullptr) {
      m_records.emplace(MeteoRecord::MeteoParameters(p), MeteoRecord(p_record));
    }
  }
}

HostApi123::MeteoRecordSet::~MeteoRecordSet(void) {}

void HostApi123::MeteoRecordSet::Add(
    const HostApi123::MeteoRecord::MeteoParameters type, MeteoRecord record) {
  m_records.emplace(type, std::move(record));
}

GribRecordSet* HostApi123::MeteoRecordSet::AsGribRecordSet() {
  if (m_records.empty()) return nullptr;

  GribRecordSet* result =
      new GribRecordSet(m_Reference_Time.time_since_epoch().count());
  result->m_Reference_Time = m_Reference_Time.time_since_epoch().count();

  for (auto record : m_records)
    result->SetUnRefGribRecord(record.first, record.second.AsGribRecord());

  // Note: Pointers to GribRecords have already been nulled by AsGribRecord()
  m_records.clear();
  return result;
}
