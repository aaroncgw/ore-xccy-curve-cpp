/**
 * Example: Cross-Currency Basis Curve Bootstrapping with ORE
 *
 * This example demonstrates:
 * 1. Creating market data for GBPUSD and USDJPY currency pairs
 * 2. Building OIS discount curves from swap rates
 * 3. Bootstrapping XCCY basis curves with full parameter support
 * 4. Saving curves to CSV/JSON files
 * 5. Loading curves from files
 *
 * The key difference from Python: C++ has access to all 32 parameters
 * of CrossCcyBasisMtMResetSwapHelper, whereas Python SWIG only exposes 18.
 */

#include <ore_xccy_curve/market_data.hpp>
#include <ore_xccy_curve/curve_builder.hpp>
#include <ore_xccy_curve/curve_utils.hpp>

#include <ql/settings.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

#include <iostream>
#include <iomanip>
#include <filesystem>

using namespace ore_xccy_curve;
using namespace QuantLib;

// Create a flat forward curve for demonstration purposes
// In production, you would bootstrap OIS curves from actual OIS swap rates
Handle<YieldTermStructure> create_flat_curve(
    const Date& eval_date,
    double rate,
    const DayCounter& dc = Actual365Fixed()
) {
    auto curve = ext::make_shared<FlatForward>(eval_date, rate, dc);
    curve->enableExtrapolation();
    return Handle<YieldTermStructure>(curve);
}

void example_gbpusd() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  GBPUSD Cross-Currency Basis Curve Example\n";
    std::cout << std::string(70, '=') << "\n";

    // Create GBPUSD market data
    Date valuation_date(15, January, 2024);
    Settings::instance().evaluationDate() = valuation_date;

    XCCYMarketData market_data = MarketDataFactory::create_gbpusd(valuation_date);
    market_data.print_summary();

    // Create input curves (using flat curves for demonstration)
    // In production, these would be bootstrapped from OIS swap rates
    double usd_rate = 0.0525;  // ~5.25% SOFR
    double gbp_rate = 0.0475;  // ~4.75% SONIA

    auto usd_discount_curve = create_flat_curve(valuation_date, usd_rate);
    auto usd_index_curve = usd_discount_curve;  // Same for OIS collateralized curves
    auto gbp_index_curve = create_flat_curve(valuation_date, gbp_rate);

    std::cout << "\nInput curves (flat for demonstration):\n";
    std::cout << "  USD (SOFR): " << std::fixed << std::setprecision(2)
              << usd_rate * 100 << "%\n";
    std::cout << "  GBP (SONIA): " << gbp_rate * 100 << "%\n";

    // Build XCCY curve
    XCCYCurveBuilder builder(
        market_data,
        usd_discount_curve,
        usd_index_curve,
        gbp_index_curve
    );

    Handle<YieldTermStructure> gbp_xccy_curve = builder.build();

    // Print curve summary
    builder.print_curve_summary();

    // Save curves to files
    std::string output_dir = "./output/";
    std::filesystem::create_directories(output_dir);

    save_curve_to_csv(gbp_xccy_curve, output_dir + "gbp_xccy_curve.csv",
                      {}, "GBP_XCCY_USD_Collateral");
    save_curve_to_json(gbp_xccy_curve, output_dir + "gbp_xccy_curve.json",
                       {}, "GBP_XCCY_USD_Collateral");

    std::cout << "\nCurves saved to:\n";
    std::cout << "  " << output_dir << "gbp_xccy_curve.csv\n";
    std::cout << "  " << output_dir << "gbp_xccy_curve.json\n";
}

void example_usdjpy() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  USDJPY Cross-Currency Basis Curve Example\n";
    std::cout << std::string(70, '=') << "\n";

    // Create USDJPY market data
    Date valuation_date(15, January, 2024);
    Settings::instance().evaluationDate() = valuation_date;

    XCCYMarketData market_data = MarketDataFactory::create_usdjpy(valuation_date);
    market_data.print_summary();

    // Create input curves
    double usd_rate = 0.0525;  // ~5.25% SOFR
    double jpy_rate = 0.0010;  // ~0.1% TONAR

    auto usd_discount_curve = create_flat_curve(valuation_date, usd_rate);
    auto usd_index_curve = usd_discount_curve;
    auto jpy_index_curve = create_flat_curve(valuation_date, jpy_rate);

    std::cout << "\nInput curves (flat for demonstration):\n";
    std::cout << "  USD (SOFR): " << std::fixed << std::setprecision(2)
              << usd_rate * 100 << "%\n";
    std::cout << "  JPY (TONAR): " << jpy_rate * 100 << "%\n";

    // Build XCCY curve
    XCCYCurveBuilder builder(
        market_data,
        usd_discount_curve,
        usd_index_curve,
        jpy_index_curve
    );

    Handle<YieldTermStructure> jpy_xccy_curve = builder.build();

    // Print curve summary
    builder.print_curve_summary();

    // Save curves
    std::string output_dir = "./output/";
    std::filesystem::create_directories(output_dir);

    save_curve_to_csv(jpy_xccy_curve, output_dir + "jpy_xccy_curve.csv",
                      {}, "JPY_XCCY_USD_Collateral");
    save_curve_to_json(jpy_xccy_curve, output_dir + "jpy_xccy_curve.json",
                       {}, "JPY_XCCY_USD_Collateral");

    std::cout << "\nCurves saved to:\n";
    std::cout << "  " << output_dir << "jpy_xccy_curve.csv\n";
    std::cout << "  " << output_dir << "jpy_xccy_curve.json\n";
}

void example_load_curve() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  Load Curve from File Example\n";
    std::cout << std::string(70, '=') << "\n";

    try {
        // Load a previously saved curve
        auto curve = load_curve_from_csv("./output/gbp_xccy_curve.csv");

        Date ref_date = curve->referenceDate();
        std::cout << "\nLoaded curve:\n";
        std::cout << "  Reference date: " << ref_date << "\n";

        // Sample some points
        std::vector<std::string> tenors = {"1Y", "5Y", "10Y", "30Y"};
        std::cout << "\n" << std::setw(10) << "Tenor"
                  << std::setw(18) << "Discount Factor"
                  << std::setw(15) << "Zero Rate" << "\n";
        std::cout << std::string(43, '-') << "\n";

        for (const auto& tenor : tenors) {
            char unit = tenor.back();
            int value = std::stoi(tenor.substr(0, tenor.size() - 1));

            Period period;
            if (unit == 'Y') period = Period(value, Years);
            else if (unit == 'M') period = Period(value, Months);
            else continue;

            Date target = ref_date + period;
            double df = curve->discount(target);
            double zr = curve->zeroRate(target, Actual365Fixed(), Continuous).rate();

            std::cout << std::setw(10) << tenor
                      << std::setw(18) << std::fixed << std::setprecision(10) << df
                      << std::setw(15) << std::setprecision(6) << zr * 100 << "%\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Could not load curve: " << e.what() << "\n";
        std::cout << "(Run GBPUSD example first to create the file)\n";
    }
}

void example_with_ois_rates() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  Full Pipeline: OIS + XCCY Curve Building\n";
    std::cout << std::string(70, '=') << "\n";

    Date valuation_date(15, January, 2024);
    Settings::instance().evaluationDate() = valuation_date;

    // USD OIS rates (SOFR)
    std::vector<std::pair<std::string, double>> usd_ois_rates = {
        {"1M", 0.0530},
        {"3M", 0.0528},
        {"6M", 0.0520},
        {"1Y", 0.0505},
        {"2Y", 0.0475},
        {"3Y", 0.0450},
        {"5Y", 0.0420},
        {"7Y", 0.0410},
        {"10Y", 0.0400},
        {"15Y", 0.0395},
        {"20Y", 0.0390},
        {"30Y", 0.0385},
    };

    // GBP OIS rates (SONIA)
    std::vector<std::pair<std::string, double>> gbp_ois_rates = {
        {"1M", 0.0480},
        {"3M", 0.0478},
        {"6M", 0.0470},
        {"1Y", 0.0455},
        {"2Y", 0.0425},
        {"3Y", 0.0400},
        {"5Y", 0.0370},
        {"7Y", 0.0360},
        {"10Y", 0.0350},
        {"15Y", 0.0345},
        {"20Y", 0.0340},
        {"30Y", 0.0335},
    };

    auto& configs = MarketDataFactory::get_currency_configs();

    // Build USD OIS curve
    std::cout << "\nBuilding USD OIS (SOFR) curve...\n";
    OISCurveBuilder usd_builder(valuation_date, configs.at("USD"), usd_ois_rates);
    Handle<YieldTermStructure> usd_curve = usd_builder.build();

    // Build GBP OIS curve
    std::cout << "Building GBP OIS (SONIA) curve...\n";
    OISCurveBuilder gbp_builder(valuation_date, configs.at("GBP"), gbp_ois_rates);
    Handle<YieldTermStructure> gbp_curve = gbp_builder.build();

    // Create GBPUSD market data
    XCCYMarketData market_data = MarketDataFactory::create_gbpusd(valuation_date);

    // Build XCCY curve using bootstrapped OIS curves
    std::cout << "Building GBP XCCY curve...\n";
    XCCYCurveBuilder xccy_builder(
        market_data,
        usd_curve,      // USD discount (collateral)
        usd_curve,      // USD index (for domestic leg)
        gbp_curve       // GBP index (for foreign leg)
    );

    Handle<YieldTermStructure> gbp_xccy = xccy_builder.build();
    xccy_builder.print_curve_summary();

    // Compare OIS vs XCCY discount factors
    std::cout << "\nComparison: GBP OIS vs GBP XCCY Discount Factors\n";
    std::cout << std::string(60, '-') << "\n";
    std::cout << std::setw(10) << "Tenor"
              << std::setw(18) << "GBP OIS DF"
              << std::setw(18) << "GBP XCCY DF"
              << std::setw(12) << "Diff (bps)" << "\n";
    std::cout << std::string(60, '-') << "\n";

    std::vector<std::string> tenors = {"1Y", "2Y", "5Y", "10Y", "30Y"};
    Date ref = gbp_curve->referenceDate();

    for (const auto& tenor : tenors) {
        int years = std::stoi(tenor.substr(0, tenor.size() - 1));
        Date target = ref + Period(years, Years);

        double ois_df = gbp_curve->discount(target);
        double xccy_df = gbp_xccy->discount(target);

        // Convert to zero rate difference in bps
        double year_frac = Actual365Fixed().yearFraction(ref, target);
        double ois_zr = -std::log(ois_df) / year_frac;
        double xccy_zr = -std::log(xccy_df) / year_frac;
        double diff_bps = (xccy_zr - ois_zr) * 10000;

        std::cout << std::setw(10) << tenor
                  << std::setw(18) << std::fixed << std::setprecision(10) << ois_df
                  << std::setw(18) << xccy_df
                  << std::setw(12) << std::setprecision(2) << diff_bps << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << std::string(70, '*') << "\n";
    std::cout << "*  ORE Cross-Currency Basis Curve Bootstrapping - C++ Version       *\n";
    std::cout << "*  Full CrossCcyBasisMtMResetSwapHelper parameter support           *\n";
    std::cout << std::string(70, '*') << "\n";

    try {
        // Run examples
        example_gbpusd();
        example_usdjpy();
        example_load_curve();
        example_with_ois_rates();

        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "  All examples completed successfully!\n";
        std::cout << std::string(70, '=') << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
