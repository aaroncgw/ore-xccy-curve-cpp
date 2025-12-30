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
 * Configuration for a currency in XCCY curve building.
 * Includes all swap convention parameters.
 */
struct CurrencyConfig {
    std::string ccy;            // ISO currency code (e.g., "USD", "GBP", "EUR")
    std::string ois_index_name; // OIS index name (e.g., "SOFR", "SONIA", "ESTR")
    std::string calendar_name;  // Calendar identifier
    std::string day_count;      // Day count convention (e.g., "Actual360")

    // XCCY swap conventions - fully configurable in C++
    std::string payment_tenor;      // Payment frequency (e.g., "3M", "6M", "1Y")
    int payment_lag;                // Payment lag in business days
    int fixing_days;                // Fixing days before period start
    std::optional<std::string> lookback; // Lookback period for OIS (e.g., "2D")
    int rate_cutoff;                // Rate cutoff days before period end
    bool include_spread;            // Include spread in compounding
    bool is_averaged;               // Use averaged vs compounded rate

    // Default constructor with typical OIS swap conventions
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
};

/**
 * Market data for cross-currency curve bootstrapping.
 *
 * Contains all XCCY-specific market data needed to bootstrap
 * the foreign XCCY curve:
 * - Currency pair configuration (domestic/foreign)
 * - FX spot rate
 * - FX forward points for short end
 * - Cross-currency basis swap spreads for long end
 */
struct XCCYMarketData {
    QuantLib::Date valuation_date;
    CurrencyConfig domestic_ccy;  // Collateral/domestic currency (e.g., USD)
    CurrencyConfig foreign_ccy;   // Foreign currency (e.g., GBP, EUR, JPY)
    double fx_spot;               // FX spot rate in market convention
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

/**
 * Factory for creating market data with dummy values for various currency pairs.
 */
class MarketDataFactory {
public:
    // Predefined currency configurations
    static const std::map<std::string, CurrencyConfig>& get_currency_configs();

    // Create dummy market data for various pairs
    static XCCYMarketData create_gbpusd(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );

    static XCCYMarketData create_eurusd(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );

    static XCCYMarketData create_usdjpy(
        const QuantLib::Date& valuation_date = QuantLib::Date()
    );
};

} // namespace ore_xccy_curve
