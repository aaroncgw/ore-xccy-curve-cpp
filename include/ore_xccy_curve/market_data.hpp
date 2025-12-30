#pragma once

#include <ql/time/date.hpp>
#include <ql/time/period.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/calendar.hpp>

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace ore_xccy_curve {

//=============================================================================
// MARKET DATA (Pure quotes - no conventions)
//=============================================================================

/**
 * FX Forward quote data.
 */
struct FXForwardQuote {
    std::string tenor;      // e.g., "1M", "3M", "6M", "1Y"
    double forward_points;  // Forward points in pips

    FXForwardQuote(const std::string& t, double pts)
        : tenor(t), forward_points(pts) {}
};

/**
 * Cross-currency basis swap quote data.
 */
struct XCCYBasisSwapQuote {
    std::string tenor;      // e.g., "2Y", "5Y", "10Y"
    double basis_spread;    // Basis spread in basis points

    XCCYBasisSwapQuote(const std::string& t, double spread)
        : tenor(t), basis_spread(spread) {}
};

/**
 * Pure market data for cross-currency curve bootstrapping.
 *
 * Contains ONLY market quotes - no conventions.
 * Conventions are provided separately via XCCYConventions.
 */
struct XCCYMarketData {
    QuantLib::Date valuation_date;
    std::string domestic_ccy;             // Domestic currency code (e.g., "USD")
    std::string foreign_ccy;              // Foreign currency code (e.g., "GBP")
    double fx_spot;                       // FX spot rate in market convention
    std::vector<FXForwardQuote> fx_forwards;
    std::vector<XCCYBasisSwapQuote> xccy_basis_swaps;
    std::optional<std::string> fx_base_ccy;  // FX base currency (first in pair)

    /**
     * Get the currency pair string in market convention.
     * E.g., "GBPUSD", "USDJPY"
     */
    std::string ccy_pair() const;

    /**
     * Return true if FX base currency equals domestic (collateral) currency.
     * This affects how FX spot is interpreted in curve helpers.
     */
    bool is_fx_base_domestic() const;

    /**
     * Get outright forward rate for a given tenor.
     */
    double get_forward_rate(const std::string& tenor) const;

    /**
     * Get cross-currency basis spread in basis points for a tenor.
     */
    double get_basis_spread_bps(const std::string& tenor) const;

    /**
     * Print a summary of the market data.
     */
    void print_summary() const;
};

//=============================================================================
// CONVENTIONS (Swap and curve building conventions)
//=============================================================================

/**
 * OIS leg conventions for a single currency.
 *
 * These parameters map to CrossCcyBasisMtMResetSwapHelper:
 * - payment_tenor -> foreignTenor/domesticTenor
 * - payment_lag -> foreignPaymentLag/domesticPaymentLag
 * - fixing_days -> foreignFixingDays/domesticFixingDays
 * - lookback -> foreignLookback/domesticLookback
 * - rate_cutoff -> foreignRateCutoff/domesticRateCutoff
 * - include_spread -> foreignIncludeSpread/domesticIncludeSpread
 * - is_averaged -> foreignIsAveraged/domesticIsAveraged
 */
struct OISLegConventions {
    std::string payment_tenor;              // Payment frequency (e.g., "3M", "6M", "1Y")
    int payment_lag;                        // Payment lag in business days
    int fixing_days;                        // Fixing days before period start
    std::optional<std::string> lookback;    // Lookback period for OIS (e.g., "2D")
    int rate_cutoff;                        // Rate cutoff days before period end
    bool include_spread;                    // Include spread in compounding
    bool is_averaged;                       // Use averaged vs compounded rate

    OISLegConventions(
        const std::string& pay_tenor = "3M",
        int pay_lag = 2,
        int fix_days = 2,
        std::optional<std::string> lb = std::nullopt,
        int cutoff = 2,
        bool inc_spread = false,
        bool averaged = false
    ) : payment_tenor(pay_tenor),
        payment_lag(pay_lag),
        fixing_days(fix_days),
        lookback(lb),
        rate_cutoff(cutoff),
        include_spread(inc_spread),
        is_averaged(averaged) {}
};

/**
 * Currency-specific conventions (index, calendar, day count).
 */
struct CurrencyConventions {
    std::string ccy;                // ISO currency code
    std::string ois_index_name;     // OIS index name (e.g., "SOFR", "SONIA")
    std::string calendar_name;      // Calendar identifier
    std::string day_count;          // Day count convention

    CurrencyConventions(
        const std::string& currency = "",
        const std::string& index = "",
        const std::string& cal = "",
        const std::string& dc = "Actual360"
    ) : ccy(currency),
        ois_index_name(index),
        calendar_name(cal),
        day_count(dc) {}
};

/**
 * Complete conventions for XCCY curve building.
 *
 * Combines currency conventions and OIS leg conventions
 * for both domestic and foreign legs.
 */
struct XCCYConventions {
    CurrencyConventions domestic;       // Domestic currency conventions
    CurrencyConventions foreign;        // Foreign currency conventions
    OISLegConventions domestic_leg;     // Domestic OIS leg conventions
    OISLegConventions foreign_leg;      // Foreign OIS leg conventions
    int settlement_days;                // Settlement days for swaps

    XCCYConventions() : settlement_days(2) {}

    XCCYConventions(
        const CurrencyConventions& dom,
        const CurrencyConventions& fgn,
        const OISLegConventions& dom_leg = OISLegConventions(),
        const OISLegConventions& fgn_leg = OISLegConventions(),
        int settle_days = 2
    ) : domestic(dom),
        foreign(fgn),
        domestic_leg(dom_leg),
        foreign_leg(fgn_leg),
        settlement_days(settle_days) {}
};

//=============================================================================
// LEGACY SUPPORT - CurrencyConfig (combines conventions, for backwards compat)
//=============================================================================

/**
 * Combined currency configuration (legacy, for backwards compatibility).
 * New code should use CurrencyConventions + OISLegConventions separately.
 */
struct CurrencyConfig {
    std::string ccy;
    std::string ois_index_name;
    std::string calendar_name;
    std::string day_count;
    std::string payment_tenor;
    int payment_lag;
    int fixing_days;
    std::optional<std::string> lookback;
    int rate_cutoff;
    bool include_spread;
    bool is_averaged;

    CurrencyConfig(
        const std::string& currency = "",
        const std::string& index = "",
        const std::string& cal = "",
        const std::string& dc = "Actual360",
        const std::string& pay_tenor = "3M",
        int pay_lag = 2,
        int fix_days = 2,
        std::optional<std::string> lb = std::nullopt,
        int cutoff = 2,
        bool inc_spread = false,
        bool averaged = false
    ) : ccy(currency),
        ois_index_name(index),
        calendar_name(cal),
        day_count(dc),
        payment_tenor(pay_tenor),
        payment_lag(pay_lag),
        fixing_days(fix_days),
        lookback(lb),
        rate_cutoff(cutoff),
        include_spread(inc_spread),
        is_averaged(averaged) {}

    // Convert to new separated types
    CurrencyConventions to_currency_conventions() const {
        return CurrencyConventions(ccy, ois_index_name, calendar_name, day_count);
    }

    OISLegConventions to_leg_conventions() const {
        return OISLegConventions(payment_tenor, payment_lag, fixing_days,
                                  lookback, rate_cutoff, include_spread, is_averaged);
    }
};

//=============================================================================
// FACTORIES
//=============================================================================

/**
 * Factory for creating conventions for various currencies.
 */
class ConventionsFactory {
public:
    // Get predefined currency conventions
    static const std::map<std::string, CurrencyConventions>& get_currency_conventions();

    // Get default OIS leg conventions for a currency
    static OISLegConventions get_default_leg_conventions(const std::string& ccy);

    // Create XCCY conventions for a currency pair
    static XCCYConventions create_xccy_conventions(
        const std::string& domestic_ccy,
        const std::string& foreign_ccy
    );
};

/**
 * Factory for creating market data with dummy values for various currency pairs.
 */
class MarketDataFactory {
public:
    // Legacy: get combined currency configs (for backwards compatibility)
    static const std::map<std::string, CurrencyConfig>& get_currency_configs();

    // Create dummy market data (quotes only)
    static XCCYMarketData create_gbpusd_quotes(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );

    static XCCYMarketData create_eurusd_quotes(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );

    static XCCYMarketData create_usdjpy_quotes(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );

    // Legacy: create market data with embedded conventions (for backwards compatibility)
    // These return a struct that contains both quotes and conventions for the old API
    struct LegacyXCCYMarketData {
        XCCYMarketData quotes;
        XCCYConventions conventions;
    };

    static LegacyXCCYMarketData create_gbpusd(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );

    static LegacyXCCYMarketData create_eurusd(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );

    static LegacyXCCYMarketData create_usdjpy(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );
};

} // namespace ore_xccy_curve
