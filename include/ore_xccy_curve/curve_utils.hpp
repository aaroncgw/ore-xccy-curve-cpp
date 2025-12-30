#pragma once

#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/handle.hpp>
#include <ql/time/date.hpp>

#include <string>
#include <vector>
#include <tuple>

namespace ore_xccy_curve {

/**
 * Curve point data (date, discount factor, zero rate).
 */
struct CurvePoint {
    std::string date_str;
    double discount_factor;
    double zero_rate;
};

/**
 * Extract curve points from a yield term structure.
 *
 * @param curve Handle to the yield term structure
 * @param tenors Optional list of tenors to extract (e.g., {"1M", "3M", "1Y"})
 *               If empty, uses a default grid
 * @param max_years Maximum years for default tenor grid
 * @return Vector of curve points
 */
std::vector<CurvePoint> extract_curve_points(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::vector<std::string>& tenors = {},
    int max_years = 50
);

/**
 * Save curve to CSV file.
 *
 * @param curve Handle to the yield term structure
 * @param file_path Path to output file
 * @param tenors Optional list of tenors to save
 * @param curve_name Name to include in file header
 */
void save_curve_to_csv(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::string& file_path,
    const std::vector<std::string>& tenors = {},
    const std::string& curve_name = "curve"
);

/**
 * Save curve to JSON file.
 *
 * @param curve Handle to the yield term structure
 * @param file_path Path to output file
 * @param tenors Optional list of tenors to save
 * @param curve_name Name to include in file metadata
 */
void save_curve_to_json(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::string& file_path,
    const std::vector<std::string>& tenors = {},
    const std::string& curve_name = "curve"
);

/**
 * Load curve from CSV file.
 *
 * @param file_path Path to input file
 * @param use_discount_factors If true, build from DFs; if false, from zero rates
 * @return Shared pointer to the loaded curve
 */
QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure> load_curve_from_csv(
    const std::string& file_path,
    bool use_discount_factors = true
);

/**
 * Load curve from JSON file.
 *
 * @param file_path Path to input file
 * @param use_discount_factors If true, build from DFs; if false, from zero rates
 * @return Shared pointer to the loaded curve
 */
QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure> load_curve_from_json(
    const std::string& file_path,
    bool use_discount_factors = true
);

/**
 * Get discount factors for multiple dates.
 */
std::vector<double> get_discount_factors(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::vector<QuantLib::Date>& dates
);

/**
 * Get zero rates for multiple dates.
 */
std::vector<double> get_zero_rates(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
    const std::vector<QuantLib::Date>& dates,
    const QuantLib::DayCounter& day_count = QuantLib::Actual365Fixed(),
    QuantLib::Compounding compounding = QuantLib::Continuous
);

} // namespace ore_xccy_curve
