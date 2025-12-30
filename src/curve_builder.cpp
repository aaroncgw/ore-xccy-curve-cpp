#include "ore_xccy_curve/curve_builder.hpp"

#include <ql/indexes/ibor/sofr.hpp>
#include <ql/indexes/ibor/sonia.hpp>
#include <ql/indexes/ibor/estr.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/japan.hpp>
#include <ql/time/calendars/switzerland.hpp>
#include <ql/time/calendars/canada.hpp>
#include <ql/time/calendars/australia.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/bootstraptraits.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/quotes/simplequote.hpp>

#include <qle/indexes/ibor/jpytonar.hpp>
#include <qle/indexes/ibor/chfsaron.hpp>
#include <qle/indexes/ibor/audaonia.hpp>
#include <qle/indexes/ibor/cadcorra.hpp>

#include <boost/optional.hpp>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace ore_xccy_curve {

// OISIndexFactory implementation
std::map<std::string, OISIndexFactory::IndexCreator>& OISIndexFactory::get_registry() {
    static std::map<std::string, IndexCreator> registry = {
        {"SOFR", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h) {
            return h.empty() ? QuantLib::ext::make_shared<QuantLib::Sofr>()
                             : QuantLib::ext::make_shared<QuantLib::Sofr>(h);
        }},
        {"SONIA", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h) {
            return h.empty() ? QuantLib::ext::make_shared<QuantLib::Sonia>()
                             : QuantLib::ext::make_shared<QuantLib::Sonia>(h);
        }},
        {"ESTR", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h) {
            return h.empty() ? QuantLib::ext::make_shared<QuantLib::Estr>()
                             : QuantLib::ext::make_shared<QuantLib::Estr>(h);
        }},
        {"TONAR", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h) {
            return h.empty() ? QuantLib::ext::make_shared<QuantExt::JpyTonar>()
                             : QuantLib::ext::make_shared<QuantExt::JpyTonar>(h);
        }},
        {"SARON", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h) {
            return h.empty() ? QuantLib::ext::make_shared<QuantExt::ChfSaron>()
                             : QuantLib::ext::make_shared<QuantExt::ChfSaron>(h);
        }},
        {"AONIA", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h) {
            return h.empty() ? QuantLib::ext::make_shared<QuantExt::AudAonia>()
                             : QuantLib::ext::make_shared<QuantExt::AudAonia>(h);
        }},
        {"CORRA", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h) {
            return h.empty() ? QuantLib::ext::make_shared<QuantExt::CadCorra>()
                             : QuantLib::ext::make_shared<QuantExt::CadCorra>(h);
        }},
    };
    return registry;
}

QuantLib::ext::shared_ptr<QuantLib::OvernightIndex> OISIndexFactory::create(
    const std::string& index_name,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve
) {
    auto& registry = get_registry();
    auto it = registry.find(index_name);
    if (it == registry.end()) {
        throw std::runtime_error("Unknown OIS index: " + index_name);
    }
    return it->second(curve);
}

void OISIndexFactory::register_index(const std::string& name, IndexCreator creator) {
    get_registry()[name] = creator;
}

// CalendarFactory implementation
QuantLib::Calendar CalendarFactory::create(const std::string& calendar_name) {
    static std::map<std::string, std::function<QuantLib::Calendar()>> registry = {
        {"US-FederalReserve", []() { return QuantLib::UnitedStates(QuantLib::UnitedStates::FederalReserve); }},
        {"US-NYSE", []() { return QuantLib::UnitedStates(QuantLib::UnitedStates::NYSE); }},
        {"UK-Exchange", []() { return QuantLib::UnitedKingdom(QuantLib::UnitedKingdom::Exchange); }},
        {"TARGET", []() { return QuantLib::TARGET(); }},
        {"Japan", []() { return QuantLib::Japan(); }},
        {"Switzerland", []() { return QuantLib::Switzerland(); }},
        {"Australia", []() { return QuantLib::Australia(); }},
        {"Canada", []() { return QuantLib::Canada(); }},
    };

    auto it = registry.find(calendar_name);
    if (it == registry.end()) {
        throw std::runtime_error("Unknown calendar: " + calendar_name);
    }
    return it->second();
}

// DayCountFactory implementation
QuantLib::DayCounter DayCountFactory::create(const std::string& day_count_name) {
    static std::map<std::string, std::function<QuantLib::DayCounter()>> registry = {
        {"Actual360", []() { return QuantLib::Actual360(); }},
        {"Actual365Fixed", []() { return QuantLib::Actual365Fixed(); }},
        {"ActualActual", []() { return QuantLib::ActualActual(QuantLib::ActualActual::ISDA); }},
        {"Thirty360", []() { return QuantLib::Thirty360(QuantLib::Thirty360::BondBasis); }},
    };

    auto it = registry.find(day_count_name);
    if (it == registry.end()) {
        throw std::runtime_error("Unknown day count: " + day_count_name);
    }
    return it->second();
}

// OISCurveBuilder implementation - new constructor with separated types
OISCurveBuilder::OISCurveBuilder(
    const QuantLib::Date& eval_date,
    const CurrencyConventions& ccy_conv,
    const std::vector<std::pair<std::string, double>>& ois_rates,
    QuantLib::Natural settlement_days
) : eval_date_(eval_date),
    ccy_conv_(ccy_conv),
    ois_rates_(ois_rates),
    settlement_days_(settlement_days)
{
    calendar_ = CalendarFactory::create(ccy_conv_.calendar_name);
    day_count_ = DayCountFactory::create(ccy_conv_.day_count);
}

// OISCurveBuilder - legacy constructor for backwards compatibility
OISCurveBuilder::OISCurveBuilder(
    const QuantLib::Date& eval_date,
    const CurrencyConfig& ccy_config,
    const std::vector<std::pair<std::string, double>>& ois_rates,
    QuantLib::Natural settlement_days
) : OISCurveBuilder(eval_date, ccy_config.to_currency_conventions(), ois_rates, settlement_days)
{
}

QuantLib::Handle<QuantLib::YieldTermStructure> OISCurveBuilder::build() {
    std::vector<QuantLib::ext::shared_ptr<QuantLib::RateHelper>> helpers;
    auto index = OISIndexFactory::create(ccy_conv_.ois_index_name);

    for (const auto& [tenor, rate] : ois_rates_) {
        auto quote = QuantLib::ext::make_shared<QuantLib::SimpleQuote>(rate);
        auto quote_handle = QuantLib::Handle<QuantLib::Quote>(quote);

        QuantLib::Period period;
        char unit = tenor.back();
        int value = std::stoi(tenor.substr(0, tenor.size() - 1));
        if (unit == 'W') period = QuantLib::Period(value, QuantLib::Weeks);
        else if (unit == 'M') period = QuantLib::Period(value, QuantLib::Months);
        else if (unit == 'Y') period = QuantLib::Period(value, QuantLib::Years);
        else throw std::runtime_error("Unknown tenor format: " + tenor);

        auto helper = QuantLib::ext::make_shared<QuantLib::OISRateHelper>(
            settlement_days_, period, quote_handle, index
        );
        helpers.push_back(helper);
    }

    auto curve = QuantLib::ext::make_shared<
        QuantLib::PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear>
    >(eval_date_, helpers, day_count_);
    curve->enableExtrapolation();

    return QuantLib::Handle<QuantLib::YieldTermStructure>(curve);
}

// XCCYCurveBuilder implementation - new constructor with separated types
XCCYCurveBuilder::XCCYCurveBuilder(
    const XCCYMarketData& market_data,
    const XCCYConventions& conventions,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_discount_curve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_index_curve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& foreign_index_curve
) : market_data_(market_data),
    conventions_(conventions),
    domestic_discount_curve_(domestic_discount_curve),
    domestic_index_curve_(domestic_index_curve),
    foreign_index_curve_(foreign_index_curve),
    settlement_days_(conventions.settlement_days)
{
    setup_conventions();
}

void XCCYCurveBuilder::setup_conventions() {
    domestic_calendar_ = CalendarFactory::create(conventions_.domestic.calendar_name);
    foreign_calendar_ = CalendarFactory::create(conventions_.foreign.calendar_name);
    joint_calendar_ = QuantLib::JointCalendar(domestic_calendar_, foreign_calendar_);
    foreign_day_count_ = DayCountFactory::create(conventions_.foreign.day_count);
    eval_date_ = market_data_.valuation_date;
    QuantLib::Settings::instance().evaluationDate() = eval_date_;
}

QuantLib::Period XCCYCurveBuilder::tenor_to_period(const std::string& tenor) const {
    char unit = tenor.back();
    int value = std::stoi(tenor.substr(0, tenor.size() - 1));
    if (unit == 'W') return QuantLib::Period(value, QuantLib::Weeks);
    if (unit == 'M') return QuantLib::Period(value, QuantLib::Months);
    if (unit == 'Y') return QuantLib::Period(value, QuantLib::Years);
    if (unit == 'D') return QuantLib::Period(value, QuantLib::Days);
    throw std::runtime_error("Unknown tenor format: " + tenor);
}

QuantLib::Handle<QuantLib::YieldTermStructure> XCCYCurveBuilder::build() {
    // FX spot quote
    auto fx_spot_quote = QuantLib::ext::make_shared<QuantLib::SimpleQuote>(market_data_.fx_spot);
    auto fx_spot_handle = QuantLib::Handle<QuantLib::Quote>(fx_spot_quote);
    fx_spot_handle_ = fx_spot_handle;

    // Relinkable handle for the curve being bootstrapped
    QuantLib::RelinkableHandle<QuantLib::YieldTermStructure> xccy_curve_handle;

    std::vector<QuantLib::ext::shared_ptr<QuantLib::RateHelper>> helpers;

    // Determine if FX base equals collateral (domestic) currency
    bool is_fx_base_collateral = market_data_.is_fx_base_domestic();

    // Add FX forward helpers for short end
    for (const auto& fwd : market_data_.fx_forwards) {
        double fwd_points = fwd.forward_points / 10000.0;
        auto fwd_points_quote = QuantLib::ext::make_shared<QuantLib::SimpleQuote>(fwd_points);
        auto fwd_points_handle = QuantLib::Handle<QuantLib::Quote>(fwd_points_quote);
        QuantLib::Period period = tenor_to_period(fwd.tenor);

        auto helper = QuantLib::ext::make_shared<QuantExt::FxSwapRateHelper>(
            fwd_points_handle,
            fx_spot_handle,
            period,
            settlement_days_,
            joint_calendar_,
            QuantLib::ModifiedFollowing,
            true,  // end of month
            is_fx_base_collateral,  // isFxBaseCurrencyCollateralCurrency
            domestic_discount_curve_
        );
        helpers.push_back(helper);
    }

    // Create indices for XCCY swap helpers
    auto domestic_index = OISIndexFactory::create(
        conventions_.domestic.ois_index_name,
        domestic_index_curve_
    );
    auto foreign_index = OISIndexFactory::create(
        conventions_.foreign.ois_index_name,
        foreign_index_curve_
    );

    // Get swap leg conventions from XCCYConventions
    const auto& foreign_leg = conventions_.foreign_leg;
    const auto& domestic_leg = conventions_.domestic_leg;

    QuantLib::Period foreign_tenor = tenor_to_period(foreign_leg.payment_tenor);
    QuantLib::Period domestic_tenor = tenor_to_period(domestic_leg.payment_tenor);

    // Optional lookback periods (ORE uses boost::optional)
    boost::optional<QuantLib::Period> foreign_lookback;
    if (foreign_leg.lookback.has_value()) {
        foreign_lookback = tenor_to_period(foreign_leg.lookback.value());
    }
    boost::optional<QuantLib::Period> domestic_lookback;
    if (domestic_leg.lookback.has_value()) {
        domestic_lookback = tenor_to_period(domestic_leg.lookback.value());
    }

    // Add cross-currency basis swap helpers for long end
    // Using CrossCcyBasisMtMResetSwapHelper with ALL parameters available in C++
    for (const auto& swap : market_data_.xccy_basis_swaps) {
        double spread = swap.basis_spread / 10000.0;
        auto spread_quote = QuantLib::ext::make_shared<QuantLib::SimpleQuote>(spread);
        auto spread_handle = QuantLib::Handle<QuantLib::Quote>(spread_quote);
        QuantLib::Period period = tenor_to_period(swap.tenor);

        // Full CrossCcyBasisMtMResetSwapHelper with ALL parameters
        auto helper = QuantLib::ext::make_shared<QuantExt::CrossCcyBasisMtMResetSwapHelper>(
            spread_handle,                       // spreadQuote
            fx_spot_handle,                      // spotFX
            settlement_days_,                    // settlementDays
            joint_calendar_,                     // settlementCalendar
            period,                              // swapTenor
            QuantLib::ModifiedFollowing,         // rollConvention
            foreign_index,                       // foreignCcyIndex
            domestic_index,                      // domesticCcyIndex
            xccy_curve_handle,                   // foreignCcyDiscountCurve (being bootstrapped)
            domestic_discount_curve_,            // domesticCcyDiscountCurve
            true,                                // foreignIndexGiven
            true,                                // domesticIndexGiven
            false,                               // foreignDiscountCurveGiven (bootstrapping)
            true,                                // domesticDiscountCurveGiven
            foreign_index_curve_,                // foreignCcyFxFwdRateCurve
            domestic_index_curve_,               // domesticCcyFxFwdRateCurve
            false,                               // eom (end of month)
            true,                                // spreadOnForeignCcy
            foreign_tenor,                       // foreignTenor (payment frequency)
            domestic_tenor,                      // domesticTenor (payment frequency)
            foreign_leg.payment_lag,             // foreignPaymentLag
            domestic_leg.payment_lag,            // domesticPaymentLag
            foreign_leg.include_spread,          // foreignIncludeSpread
            foreign_lookback,                    // foreignLookback
            foreign_leg.fixing_days,             // foreignFixingDays
            foreign_leg.rate_cutoff,             // foreignRateCutoff
            foreign_leg.is_averaged,             // foreignIsAveraged
            domestic_leg.include_spread,         // domesticIncludeSpread
            domestic_lookback,                   // domesticLookback
            domestic_leg.fixing_days,            // domesticFixingDays
            domestic_leg.rate_cutoff,            // domesticRateCutoff
            domestic_leg.is_averaged,            // domesticIsAveraged
            false,                               // telescopicValueDates
            QuantLib::Pillar::LastRelevantDate   // pillarChoice
        );
        helpers.push_back(helper);
    }

    // Bootstrap the curve
    auto curve = QuantLib::ext::make_shared<
        QuantLib::PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear>
    >(eval_date_, helpers, foreign_day_count_);
    curve->enableExtrapolation();
    xccy_curve_handle.linkTo(curve);

    foreign_xccy_curve_ = QuantLib::Handle<QuantLib::YieldTermStructure>(curve);
    return foreign_xccy_curve_;
}

double XCCYCurveBuilder::get_discount_factor(const QuantLib::Date& target_date) const {
    if (foreign_xccy_curve_.empty()) {
        throw std::runtime_error("XCCY curve not built. Call build() first.");
    }
    return foreign_xccy_curve_->discount(target_date);
}

double XCCYCurveBuilder::get_zero_rate(
    const QuantLib::Date& target_date,
    QuantLib::Compounding compounding
) const {
    if (foreign_xccy_curve_.empty()) {
        throw std::runtime_error("XCCY curve not built. Call build() first.");
    }
    return foreign_xccy_curve_->zeroRate(target_date, foreign_day_count_, compounding).rate();
}

double XCCYCurveBuilder::get_implied_fx_forward(const QuantLib::Date& target_date) const {
    if (foreign_xccy_curve_.empty()) {
        throw std::runtime_error("XCCY curve not built. Call build() first.");
    }
    double df_domestic = domestic_discount_curve_->discount(target_date);
    double df_foreign = foreign_xccy_curve_->discount(target_date);
    return market_data_.fx_spot * (df_domestic / df_foreign);
}

void XCCYCurveBuilder::print_curve_summary() const {
    if (foreign_xccy_curve_.empty()) {
        std::cout << "XCCY curve not built. Call build() first.\n";
        return;
    }

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << ccy_pair() << " XCCY Basis Curve Summary\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "Valuation Date: " << market_data_.valuation_date << "\n";
    std::cout << std::fixed << std::setprecision(4) << "FX Spot: " << market_data_.fx_spot << "\n";
    std::cout << "Domestic: " << domestic_ccy() << " (" << conventions_.domestic.ois_index_name << ")\n";
    std::cout << "Foreign: " << foreign_ccy() << " (" << conventions_.foreign.ois_index_name << ")\n";

    std::vector<std::string> tenors = {"1M", "3M", "6M", "1Y", "2Y", "3Y", "5Y", "7Y", "10Y", "20Y", "30Y"};

    std::cout << "\n" << std::left
              << std::setw(8) << "Tenor"
              << std::setw(12) << (domestic_ccy() + " DF")
              << std::setw(12) << (foreign_ccy() + " DF")
              << std::setw(12) << "XCCY DF"
              << std::setw(12) << "FX Fwd"
              << std::setw(12) << "Basis(bps)" << "\n";
    std::cout << std::string(70, '-') << "\n";

    for (const auto& tenor : tenors) {
        QuantLib::Period period = tenor_to_period(tenor);
        QuantLib::Date target_date = joint_calendar_.advance(eval_date_, period);

        double df_domestic = domestic_discount_curve_->discount(target_date);
        double df_foreign_idx = foreign_index_curve_->discount(target_date);
        double df_xccy = foreign_xccy_curve_->discount(target_date);
        double fx_fwd = market_data_.fx_spot * (df_domestic / df_xccy);

        double year_frac = foreign_day_count_.yearFraction(eval_date_, target_date);
        double implied_basis = 0.0;
        if (year_frac > 0) {
            double foreign_zero = df_foreign_idx < 1 ? -1.0 / year_frac * std::log(df_foreign_idx) : 0;
            double xccy_zero = df_xccy < 1 ? -1.0 / year_frac * std::log(df_xccy) : 0;
            implied_basis = (xccy_zero - foreign_zero) * 10000;
        }

        std::cout << std::left << std::setw(8) << tenor
                  << std::fixed << std::setprecision(6) << std::setw(12) << df_domestic
                  << std::setw(12) << df_foreign_idx
                  << std::setw(12) << df_xccy
                  << std::setprecision(4) << std::setw(12) << fx_fwd
                  << std::setprecision(1) << std::setw(12) << implied_basis << "\n";
    }
    std::cout << "\n";
}

// Convenience function
std::map<std::string, QuantLib::Handle<QuantLib::YieldTermStructure>> build_xccy_curve(
    const XCCYMarketData& market_data,
    const XCCYConventions& conventions,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_discount_curve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& domestic_index_curve,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& foreign_index_curve
) {
    QuantLib::Settings::instance().evaluationDate() = market_data.valuation_date;

    XCCYCurveBuilder builder(
        market_data,
        conventions,
        domestic_discount_curve,
        domestic_index_curve,
        foreign_index_curve
    );
    auto xccy_curve = builder.build();

    return {
        {"domestic_discount", domestic_discount_curve},
        {"domestic_index", domestic_index_curve},
        {"foreign_index", foreign_index_curve},
        {"foreign_xccy", xccy_curve}
    };
}

} // namespace ore_xccy_curve
