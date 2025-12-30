#include "ore_xccy_curve/curve_utils.hpp"

#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/settings.hpp>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace ore_xccy_curve {

namespace {

std::string date_to_iso(const QuantLib::Date& date) {
    std::ostringstream oss;
    oss << date.year() << "-"
        << std::setfill('0') << std::setw(2) << static_cast<int>(date.month()) << "-"
        << std::setfill('0') << std::setw(2) << date.dayOfMonth();
    return oss.str();
}

QuantLib::Date iso_to_date(const std::string& iso_str) {
    int year = std::stoi(iso_str.substr(0, 4));
    int month = std::stoi(iso_str.substr(5, 2));
    int day = std::stoi(iso_str.substr(8, 2));
    return QuantLib::Date(day, static_cast<QuantLib::Month>(month), year);
}

QuantLib::Period tenor_to_period(const std::string& tenor) {
    char unit = tenor.back();
    int value = std::stoi(tenor.substr(0, tenor.size() - 1));
    if (unit == 'W') return QuantLib::Period(value, QuantLib::Weeks);
    if (unit == 'M') return QuantLib::Period(value, QuantLib::Months);
    if (unit == 'Y') return QuantLib::Period(value, QuantLib::Years);
    if (unit == 'D') return QuantLib::Period(value, QuantLib::Days);
    throw std::runtime_error("Unknown tenor format: " + tenor);
}

} // anonymous namespace

std::vector<CurvePoint> extract_curve_points(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::vector<std::string>& tenors,
    int max_years
) {
    std::vector<std::string> tenor_list = tenors;
    if (tenor_list.empty()) {
        // Default tenor grid
        tenor_list = {"1W", "2W", "1M", "2M", "3M", "6M", "9M"};
        for (int y = 1; y <= std::min(max_years, 50); ++y) {
            tenor_list.push_back(std::to_string(y) + "Y");
        }
    }

    QuantLib::Date ref_date = curve->referenceDate();
    QuantLib::DayCounter day_count = QuantLib::Actual365Fixed();

    std::vector<CurvePoint> points;
    for (const auto& tenor : tenor_list) {
        try {
            QuantLib::Period period = tenor_to_period(tenor);
            QuantLib::Date target_date = ref_date + period;
            double df = curve->discount(target_date);
            double zero = curve->zeroRate(target_date, day_count, QuantLib::Continuous).rate();

            points.push_back({date_to_iso(target_date), df, zero});
        } catch (...) {
            // Skip tenors that fail (e.g., beyond curve range)
            continue;
        }
    }

    return points;
}

void save_curve_to_csv(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::string& file_path,
    const std::vector<std::string>& tenors,
    const std::string& curve_name
) {
    auto points = extract_curve_points(curve, tenors);
    QuantLib::Date ref_date = curve->referenceDate();

    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    // Header with metadata
    file << "# curve_name," << curve_name << "\n";
    file << "# reference_date," << date_to_iso(ref_date) << "\n";
    file << "# day_count,Actual365Fixed\n";
    file << "# compounding,Continuous\n";
    file << "\n";
    file << "date,discount_factor,zero_rate\n";

    file << std::fixed;
    for (const auto& pt : points) {
        file << pt.date_str << ","
             << std::setprecision(15) << pt.discount_factor << ","
             << std::setprecision(10) << pt.zero_rate << "\n";
    }
}

void save_curve_to_json(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::string& file_path,
    const std::vector<std::string>& tenors,
    const std::string& curve_name
) {
    auto points = extract_curve_points(curve, tenors);
    QuantLib::Date ref_date = curve->referenceDate();

    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    file << "{\n";
    file << "  \"curve_name\": \"" << curve_name << "\",\n";
    file << "  \"reference_date\": \"" << date_to_iso(ref_date) << "\",\n";
    file << "  \"day_count\": \"Actual365Fixed\",\n";
    file << "  \"compounding\": \"Continuous\",\n";
    file << "  \"points\": [\n";

    file << std::fixed;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& pt = points[i];
        file << "    {\"date\": \"" << pt.date_str << "\", "
             << "\"discount_factor\": " << std::setprecision(15) << pt.discount_factor << ", "
             << "\"zero_rate\": " << std::setprecision(10) << pt.zero_rate << "}";
        if (i < points.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
}

QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure> load_curve_from_csv(
    const std::string& file_path,
    bool use_discount_factors
) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    QuantLib::Date ref_date;
    std::vector<QuantLib::Date> dates;
    std::vector<double> dfs;
    std::vector<double> zrs;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            // Parse metadata from comments
            if (line.substr(0, 17) == "# reference_date,") {
                ref_date = iso_to_date(line.substr(17));
            }
            continue;
        }
        if (line.substr(0, 4) == "date") continue;  // Skip header

        std::istringstream iss(line);
        std::string date_str, df_str, zr_str;
        std::getline(iss, date_str, ',');
        std::getline(iss, df_str, ',');
        std::getline(iss, zr_str, ',');

        dates.push_back(iso_to_date(date_str));
        dfs.push_back(std::stod(df_str));
        zrs.push_back(std::stod(zr_str));
    }

    if (ref_date == QuantLib::Date() && !dates.empty()) {
        ref_date = dates[0];
    }

    QuantLib::Settings::instance().evaluationDate() = ref_date;

    if (use_discount_factors) {
        // Add reference date point
        std::vector<QuantLib::Date> all_dates = {ref_date};
        std::vector<double> all_values = {1.0};
        all_dates.insert(all_dates.end(), dates.begin(), dates.end());
        all_values.insert(all_values.end(), dfs.begin(), dfs.end());

        auto curve = QuantLib::ext::make_shared<QuantLib::DiscountCurve>(
            all_dates, all_values, QuantLib::Actual365Fixed()
        );
        curve->enableExtrapolation();
        return curve;
    } else {
        // Add reference date point
        std::vector<QuantLib::Date> all_dates = {ref_date};
        std::vector<double> all_values = {zrs.empty() ? 0.0 : zrs[0]};
        all_dates.insert(all_dates.end(), dates.begin(), dates.end());
        all_values.insert(all_values.end(), zrs.begin(), zrs.end());

        auto curve = QuantLib::ext::make_shared<QuantLib::ZeroCurve>(
            all_dates, all_values, QuantLib::Actual365Fixed()
        );
        curve->enableExtrapolation();
        return curve;
    }
}

QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure> load_curve_from_json(
    const std::string& file_path,
    bool use_discount_factors
) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    // Simple JSON parsing (not using external library)
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Extract reference_date
    QuantLib::Date ref_date;
    size_t pos = content.find("\"reference_date\"");
    if (pos != std::string::npos) {
        pos = content.find("\"", pos + 16) + 1;
        size_t end = content.find("\"", pos);
        ref_date = iso_to_date(content.substr(pos, end - pos));
    }

    // Extract points
    std::vector<QuantLib::Date> dates;
    std::vector<double> dfs;
    std::vector<double> zrs;

    pos = content.find("\"points\"");
    if (pos != std::string::npos) {
        pos = content.find("[", pos);
        size_t end = content.find("]", pos);
        std::string points_str = content.substr(pos, end - pos);

        size_t obj_start = 0;
        while ((obj_start = points_str.find("{", obj_start)) != std::string::npos) {
            size_t obj_end = points_str.find("}", obj_start);
            std::string obj = points_str.substr(obj_start, obj_end - obj_start);

            // Extract date
            size_t date_pos = obj.find("\"date\"");
            date_pos = obj.find("\"", date_pos + 6) + 1;
            size_t date_end = obj.find("\"", date_pos);
            dates.push_back(iso_to_date(obj.substr(date_pos, date_end - date_pos)));

            // Extract discount_factor
            size_t df_pos = obj.find("\"discount_factor\"");
            df_pos = obj.find(":", df_pos) + 1;
            size_t df_end = obj.find(",", df_pos);
            dfs.push_back(std::stod(obj.substr(df_pos, df_end - df_pos)));

            // Extract zero_rate
            size_t zr_pos = obj.find("\"zero_rate\"");
            zr_pos = obj.find(":", zr_pos) + 1;
            size_t zr_end = obj.find("}", zr_pos);
            zrs.push_back(std::stod(obj.substr(zr_pos, zr_end - zr_pos)));

            obj_start = obj_end;
        }
    }

    QuantLib::Settings::instance().evaluationDate() = ref_date;

    if (use_discount_factors) {
        std::vector<QuantLib::Date> all_dates = {ref_date};
        std::vector<double> all_values = {1.0};
        all_dates.insert(all_dates.end(), dates.begin(), dates.end());
        all_values.insert(all_values.end(), dfs.begin(), dfs.end());

        auto curve = QuantLib::ext::make_shared<QuantLib::DiscountCurve>(
            all_dates, all_values, QuantLib::Actual365Fixed()
        );
        curve->enableExtrapolation();
        return curve;
    } else {
        std::vector<QuantLib::Date> all_dates = {ref_date};
        std::vector<double> all_values = {zrs.empty() ? 0.0 : zrs[0]};
        all_dates.insert(all_dates.end(), dates.begin(), dates.end());
        all_values.insert(all_values.end(), zrs.begin(), zrs.end());

        auto curve = QuantLib::ext::make_shared<QuantLib::ZeroCurve>(
            all_dates, all_values, QuantLib::Actual365Fixed()
        );
        curve->enableExtrapolation();
        return curve;
    }
}

std::vector<double> get_discount_factors(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::vector<QuantLib::Date>& dates
) {
    std::vector<double> dfs;
    dfs.reserve(dates.size());
    for (const auto& d : dates) {
        dfs.push_back(curve->discount(d));
    }
    return dfs;
}

std::vector<double> get_zero_rates(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::vector<QuantLib::Date>& dates,
    const QuantLib::DayCounter& day_count,
    QuantLib::Compounding compounding
) {
    std::vector<double> rates;
    rates.reserve(dates.size());
    for (const auto& d : dates) {
        rates.push_back(curve->zeroRate(d, day_count, compounding).rate());
    }
    return rates;
}

} // namespace ore_xccy_curve
