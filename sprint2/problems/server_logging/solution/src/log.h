#pragma once
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/core/record_view.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/formatting_ostream_fwd.hpp>
#include <boost/json/object.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <utility>

namespace logging = boost::log;
namespace json = boost::json;

// Форматтер для Boost.Log, создающий JSON-строку
inline void MyFormatter(logging::record_view const& req, logging::formatting_ostream& osrm) {
    json::object obj;
    // Получаем текущее время в формате ISO
    boost::posix_time::ptime time_now = boost::posix_time::microsec_clock::universal_time();
    obj["timestamp"] = boost::posix_time::to_iso_extended_string(time_now);

    // Извлекаем атрибут "data" из записи лога
    logging::value_ref<json::object> data = logging::extract<json::object>("data", req);
    if (!data.empty()) {
        obj["data"] = std::move(data.get());
    } else {
        obj["data"] = json::object{};
    }

    // Извлекаем атрибут "msg" из записи лога
    logging::value_ref<std::string> msg = logging::extract<std::string>("msg", req);
    if (!msg.empty()) {
        obj["message"] = std::move(msg.get());
    } else {
        obj["message"] = "";
    }

    osrm << obj;
}
