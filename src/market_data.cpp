#include "ore_xccy_curve/market_data.hpp"

#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace ore_xccy_curve {

//=============================================================================
// XCCYMarketData implementation
//=============================================================================

std::string XCCYMarketData::ccy_pair() const {
    std::string base = fx_base_ccy.value_or(foreign_ccy);
    std::string quote = (base == foreign_ccy) ? domestic_ccy : foreign_ccy;
    return base + quote;
}

bool XCCYMarketData::is_fx_base_domestic() const {
    std::string base = fx_base_ccy.value_or(foreign_ccy);
    return base == domestic_ccy;
}

double XCCYMarketData::get_forward_rate(const std::string& tenor) const {
    for (const auto& fwd : fx_forwards) {
        if (fwd.tenor == tenor) {
            return fx_spot + (fwd.forward_points / 10000.0);
        }
    }
    throw std::runtime_error("Unknown tenor: " + tenor);
}

double XCCYMarketData::get_basis_spread_bps(const std::string& tenor) const {
    for (const auto& swap : xccy_basis_swaps) {
        if (swap.tenor == tenor) {
            return swap.basis_spread;
        }
    }
    throw std::runtime_error("Unknown tenor: " + tenor);
}

void XCCYMarketData::print_summary() const {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << ccy_pair() << " Market Data Summary - " << valuation_date << "\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << "\nFX Spot: " << std::fixed << std::setprecision(4) << fx_spot << "\n";

    std::cout << "\n" << std::setfill('-') << std::setw(40) << "FX Forwards" << std::setfill(' ') << "\n";
    std::cout << std::left << std::setw(10) << "Tenor"
              << std::setw(12) << "Points"
              << std::setw(12) << "Outright" << "\n";
    for (const auto& fwd : fx_forwards) {
        double outright = fx_spot + (fwd.forward_points / 10000.0);
        std::cout << std::left << std::setw(10) << fwd.tenor
                  << std::setw(12) << std::fixed << std::setprecision(1) << fwd.forward_points
                  << std::setw(12) << std::fixed << std::setprecision(4) << outright << "\n";
    }

    std::cout << "\n" << std::setfill('-') << std::setw(40) << "XCCY Basis Swaps" << std::setfill(' ') << "\n";
    std::cout << std::left << std::setw(10) << "Tenor"
              << std::setw(15) << "Spread (bps)" << "\n";
    for (const auto& swap : xccy_basis_swaps) {
        std::cout << std::left << std::setw(10) << swap.tenor
                  << std::setw(15) << std::fixed << std::setprecision(1) << swap.basis_spread << "\n";
    }
    std::cout << "\n";
}

//=============================================================================
// ConventionsFactory implementation
//=============================================================================

const std::map<std::string, CurrencyConventions>& ConventionsFactory::get_currency_conventions() {
    static std::map<std::string, CurrencyConventions> conventions = {
        {"USD", CurrencyConventions("USD", "SOFR", "US-FederalReserve", "Actual360")},
        {"GBP", CurrencyConventions("GBP", "SONIA", "UK-Exchange", "Actual365Fixed")},
        {"EUR", CurrencyConventions("EUR", "ESTR", "TARGET", "Actual360")},
        {"JPY", CurrencyConventions("JPY", "TONAR", "Japan", "Actual365Fixed")},
        {"CHF", CurrencyConventions("CHF", "SARON", "Switzerland", "Actual360")},
        {"AUD", CurrencyConventions("AUD", "AONIA", "Australia", "Actual365Fixed")},
        {"CAD", CurrencyConventions("CAD", "CORRA", "Canada", "Actual365Fixed")},
    };
    return conventions;
}

OISLegConventions ConventionsFactory::get_default_leg_conventions(const std::string& ccy) {
    // Default OIS leg conventions - can be customized per currency if needed
    // Currently all currencies use the same defaults
    return OISLegConventions("3M", 2, 2, std::nullopt, 2, false, false);
}

XCCYConventions ConventionsFactory::create_xccy_conventions(
    const std::string& domestic_ccy,
    const std::string& foreign_ccy
) {
    const auto& ccy_convs = get_currency_conventions();

    auto dom_it = ccy_convs.find(domestic_ccy);
    auto fgn_it = ccy_convs.find(foreign_ccy);

    if (dom_it == ccy_convs.end()) {
        throw std::runtime_error("Unknown domestic currency: " + domestic_ccy);
    }
    if (fgn_it == ccy_convs.end()) {
        throw std::runtime_error("Unknown foreign currency: " + foreign_ccy);
    }

    return XCCYConventions(
        dom_it->second,
        fgn_it->second,
        get_default_leg_conventions(domestic_ccy),
        get_default_leg_conventions(foreign_ccy),
        2  // settlement days
    );
}

//=============================================================================
// MarketDataFactory implementation
//=============================================================================

// Legacy currency configs (for backwards compatibility)
const std::map<std::string, CurrencyConfig>& MarketDataFactory::get_currency_configs() {
    static std::map<std::string, CurrencyConfig> configs = {
        {"USD", CurrencyConfig("USD", "SOFR", "US-FederalReserve", "Actual360", "3M", 2, 2, std::nullopt, 2)},
        {"GBP", CurrencyConfig("GBP", "SONIA", "UK-Exchange", "Actual365Fixed", "3M", 2, 2, std::nullopt, 2)},
        {"EUR", CurrencyConfig("EUR", "ESTR", "TARGET", "Actual360", "3M", 2, 2, std::nullopt, 2)},
        {"JPY", CurrencyConfig("JPY", "TONAR", "Japan", "Actual365Fixed", "3M", 2, 2, std::nullopt, 2)},
        {"CHF", CurrencyConfig("CHF", "SARON", "Switzerland", "Actual360", "3M", 2, 2, std::nullopt, 2)},
        {"AUD", CurrencyConfig("AUD", "AONIA", "Australia", "Actual365Fixed", "3M", 2, 2, std::nullopt, 2)},
        {"CAD", CurrencyConfig("CAD", "CORRA", "Canada", "Actual365Fixed", "3M", 2, 2, std::nullopt, 2)},
    };
    return configs;
}

// Pure market data factories (quotes only)
XCCYMarketData MarketDataFactory::create_gbpusd_quotes(const QuantLib::Date& val_date) {
    QuantLib::Date valuation_date = val_date;
    if (valuation_date == QuantLib::Date()) {
        valuation_date = QuantLib::Date(15, QuantLib::January, 2024);
    }

    XCCYMarketData data;
    data.valuation_date = valuation_date;
    data.domestic_ccy = "USD";
    data.foreign_ccy = "GBP";
    data.fx_spot = 1.2750;
    data.fx_base_ccy = std::nullopt;  // GBP is FX base (default)

    data.fx_forwards = {
        {"1W", -2.5}, {"2W", -5.0}, {"1M", -12.0}, {"2M", -24.0},
        {"3M", -38.0}, {"6M", -78.0}, {"9M", -115.0}, {"1Y", -155.0}
    };

    data.xccy_basis_swaps = {
        {"2Y", -12.5}, {"3Y", -15.0}, {"4Y", -16.5}, {"5Y", -17.5},
        {"7Y", -18.0}, {"10Y", -17.0}, {"15Y", -15.0}, {"20Y", -13.0}, {"30Y", -10.0}
    };

    return data;
}

XCCYMarketData MarketDataFactory::create_eurusd_quotes(const QuantLib::Date& val_date) {
    QuantLib::Date valuation_date = val_date;
    if (valuation_date == QuantLib::Date()) {
        valuation_date = QuantLib::Date(15, QuantLib::January, 2024);
    }

    XCCYMarketData data;
    data.valuation_date = valuation_date;
    data.domestic_ccy = "USD";
    data.foreign_ccy = "EUR";
    data.fx_spot = 1.0850;
    data.fx_base_ccy = std::nullopt;  // EUR is FX base (default)

    data.fx_forwards = {
        {"1W", 1.5}, {"2W", 3.0}, {"1M", 7.0}, {"2M", 14.0},
        {"3M", 22.0}, {"6M", 45.0}, {"9M", 68.0}, {"1Y", 92.0}
    };

    data.xccy_basis_swaps = {
        {"2Y", -8.0}, {"3Y", -10.0}, {"4Y", -11.5}, {"5Y", -12.5},
        {"7Y", -13.0}, {"10Y", -12.0}, {"15Y", -10.0}, {"20Y", -8.5}, {"30Y", -7.0}
    };

    return data;
}

XCCYMarketData MarketDataFactory::create_usdjpy_quotes(const QuantLib::Date& val_date) {
    QuantLib::Date valuation_date = val_date;
    if (valuation_date == QuantLib::Date()) {
        valuation_date = QuantLib::Date(15, QuantLib::January, 2024);
    }

    XCCYMarketData data;
    data.valuation_date = valuation_date;
    data.domestic_ccy = "USD";
    data.foreign_ccy = "JPY";
    data.fx_spot = 148.50;
    data.fx_base_ccy = "USD";  // USD is FX base (unlike GBPUSD)

    data.fx_forwards = {
        {"1W", -8.0}, {"2W", -16.0}, {"1M", -35.0}, {"2M", -72.0},
        {"3M", -110.0}, {"6M", -225.0}, {"9M", -340.0}, {"1Y", -460.0}
    };

    data.xccy_basis_swaps = {
        {"2Y", -25.0}, {"3Y", -28.0}, {"4Y", -30.0}, {"5Y", -32.0},
        {"7Y", -33.0}, {"10Y", -30.0}, {"15Y", -25.0}, {"20Y", -22.0}, {"30Y", -18.0}
    };

    return data;
}

// Legacy factories (return quotes + conventions bundled)
MarketDataFactory::LegacyXCCYMarketData MarketDataFactory::create_gbpusd(
    const QuantLib::Date& valuation_date
) {
    LegacyXCCYMarketData result;
    result.quotes = create_gbpusd_quotes(valuation_date);
    result.conventions = ConventionsFactory::create_xccy_conventions("USD", "GBP");
    return result;
}

MarketDataFactory::LegacyXCCYMarketData MarketDataFactory::create_eurusd(
    const QuantLib::Date& valuation_date
) {
    LegacyXCCYMarketData result;
    result.quotes = create_eurusd_quotes(valuation_date);
    result.conventions = ConventionsFactory::create_xccy_conventions("USD", "EUR");
    return result;
}

MarketDataFactory::LegacyXCCYMarketData MarketDataFactory::create_usdjpy(
    const QuantLib::Date& valuation_date
) {
    LegacyXCCYMarketData result;
    result.quotes = create_usdjpy_quotes(valuation_date);
    result.conventions = ConventionsFactory::create_xccy_conventions("USD", "JPY");
    return result;
}

} // namespace ore_xccy_curve
