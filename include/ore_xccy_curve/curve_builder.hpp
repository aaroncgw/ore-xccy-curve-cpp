#pragma once

#include "market_data.hpp"

#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/handle.hpp>

#include <qle/termstructures/crossccybasismtmresetswaphelper.hpp>
#include <qle/termstructures/fxswaphelper.hpp>

#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace ore_xccy_curve {

/**
 * Factory for creating OIS indices based on currency configuration.
 */
class OISIndexFactory {
public:
    using IndexCreator = std::function<QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>(
        const QuantLib::Handle<QuantLib::YieldTermStructure>&
    )>;

    static QuantLib::ext::shared_ptr<QuantLib::OvernightIndex> create(
        const std::string& index_name,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& curve = {}
    );

    static void register_index(const std::string& name, IndexCreator creator);

private:
    static std::map<std::string, IndexCreator>& get_registry();
};

/**
 * Factory for creating calendars based on currency configuration.
 */
class CalendarFactory {
public:
    static QuantLib::Calendar create(const std::string& calendar_name);
};

/**
 * Factory for creating day count conventions.
 */
class DayCountFactory {
public:
    static QuantLib::DayCounter create(const std::string& day_count_name);
};

/**
 * Builds OIS discount curves from OIS swap rates.
 */
class OISCurveBuilder {
public:
    OISCurveBuilder(
        const QuantLib::Date& eval_date,
        const CurrencyConfig& ccy_config,
        const std::vector<std::pair<std::string, double>>& ois_rates,
        QuantLib::Natural settlement_days = 2
    );

    QuantLib::Handle<QuantLib::YieldTermStructure> build();

private:
    QuantLib::Date eval_date_;
    CurrencyConfig ccy_config_;
    std::vector<std::pair<std::string, double>> ois_rates_;
    QuantLib::Natural settlement_days_;
    QuantLib::Calendar calendar_;
    QuantLib::DayCounter day_count_;
};

/**
 * Builds cross-currency basis curves using ORE.
 *
 * This class builds the foreign XCCY basis curve from FX forwards
 * and cross-currency basis swap quotes.
 *
 * All CrossCcyBasisMtMResetSwapHelper parameters are exposed,
 * including those not available in the Python SWIG binding:
 * - Payment tenor (frequency)
 * - Payment lag
 * - Fixing days
 * - Lookback period
 * - Rate cutoff
 * - Include spread flag
 * - Is averaged flag
 */
class XCCYCurveBuilder {
public:
    XCCYCurveBuilder(
        const XCCYMarketData& market_data,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_discount_curve,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_index_curve,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& foreign_index_curve
    );

    /**
     * Build the cross-currency basis curve.
     * Returns handle to the foreign XCCY discount curve.
     */
    QuantLib::Handle<QuantLib::YieldTermStructure> build();

    // Accessors
    std::string ccy_pair() const { return market_data_.ccy_pair(); }
    std::string domestic_ccy() const { return market_data_.domestic_ccy.ccy; }
    std::string foreign_ccy() const { return market_data_.foreign_ccy.ccy; }

    QuantLib::Handle<QuantLib::YieldTermStructure> foreign_xccy_curve() const {
        return foreign_xccy_curve_;
    }

    /**
     * Get discount factor from the XCCY curve.
     */
    double get_discount_factor(const QuantLib::Date& target_date) const;

    /**
     * Get zero rate from the XCCY curve.
     */
    double get_zero_rate(
        const QuantLib::Date& target_date,
        QuantLib::Compounding compounding = QuantLib::Continuous
    ) const;

    /**
     * Calculate implied FX forward rate from the curves.
     * Uses covered interest rate parity: F = S * (DF_domestic / DF_foreign_xccy)
     */
    double get_implied_fx_forward(const QuantLib::Date& target_date) const;

    /**
     * Print a summary of the bootstrapped XCCY curve.
     */
    void print_curve_summary() const;

private:
    void setup_conventions();
    QuantLib::Period tenor_to_period(const std::string& tenor) const;

    XCCYMarketData market_data_;
    QuantLib::Handle<QuantLib::YieldTermStructure> domestic_discount_curve_;
    QuantLib::Handle<QuantLib::YieldTermStructure> domestic_index_curve_;
    QuantLib::Handle<QuantLib::YieldTermStructure> foreign_index_curve_;

    QuantLib::Calendar domestic_calendar_;
    QuantLib::Calendar foreign_calendar_;
    QuantLib::Calendar joint_calendar_;
    QuantLib::DayCounter foreign_day_count_;
    QuantLib::Natural settlement_days_;
    QuantLib::Date eval_date_;

    QuantLib::Handle<QuantLib::YieldTermStructure> foreign_xccy_curve_;
    QuantLib::Handle<QuantLib::Quote> fx_spot_handle_;
};

/**
 * Build XCCY curves from market data and pre-built input curves.
 *
 * Returns a map with curve handles:
 * - "domestic_discount": Domestic discount curve (pass-through)
 * - "domestic_index": Domestic index curve (pass-through)
 * - "foreign_index": Foreign index curve (pass-through)
 * - "foreign_xccy": Bootstrapped foreign XCCY basis curve
 */
std::map<std::string, QuantLib::Handle<QuantLib::YieldTermStructure>> build_xccy_curve(
    const XCCYMarketData& market_data,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_discount_curve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_index_curve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& foreign_index_curve
);

} // namespace ore_xccy_curve
