/**
 * Python bindings for ore_xccy_curve using pybind11.
 *
 * This module provides Python access to the C++ XCCYCurveBuilder with
 * full CrossCcyBasisMtMResetSwapHelper parameter support (all 32 parameters).
 *
 * Supports passing ORE Python (SWIG) curve handles to the C++ builder.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <ore_xccy_curve/market_data.hpp>
#include <ore_xccy_curve/curve_builder.hpp>
#include <ore_xccy_curve/curve_utils.hpp>

#include <ql/time/date.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>

namespace py = pybind11;
using namespace ore_xccy_curve;

// Helper to convert Python date to QuantLib Date
QuantLib::Date python_date_to_ql(int year, int month, int day) {
    return QuantLib::Date(day, static_cast<QuantLib::Month>(month), year);
}

// Helper to create a flat forward curve from Python
QuantLib::Handle<QuantLib::YieldTermStructure> create_flat_curve_py(
    int year, int month, int day,
    double rate
) {
    QuantLib::Date eval_date(day, static_cast<QuantLib::Month>(month), year);
    auto curve = QuantLib::ext::make_shared<QuantLib::FlatForward>(
        eval_date, rate, QuantLib::Actual365Fixed()
    );
    curve->enableExtrapolation();
    return QuantLib::Handle<QuantLib::YieldTermStructure>(curve);
}

/**
 * Convert an ORE/QuantLib Python curve (SWIG-wrapped) to a C++ Handle.
 *
 * This works by extracting discount factors from the Python curve at
 * standard tenors and rebuilding a DiscountCurve in C++.
 *
 * Accepts:
 * - ORE YieldTermStructureHandle (ore.YieldTermStructureHandle)
 * - ORE RelinkableYieldTermStructureHandle
 * - Any object with a discount(Date) method
 */
QuantLib::Handle<QuantLib::YieldTermStructure> convert_python_curve_to_handle(
    py::object py_curve,
    int ref_year, int ref_month, int ref_day
) {
    QuantLib::Date ref_date(ref_day, static_cast<QuantLib::Month>(ref_month), ref_year);

    // Check if it's already our pybind11 handle type
    try {
        auto handle = py_curve.cast<QuantLib::Handle<QuantLib::YieldTermStructure>>();
        return handle;
    } catch (const py::cast_error&) {
        // Not our type, continue with conversion
    }

    // Check if it's a float (flat rate)
    try {
        double rate = py_curve.cast<double>();
        auto curve = QuantLib::ext::make_shared<QuantLib::FlatForward>(
            ref_date, rate, QuantLib::Actual365Fixed()
        );
        curve->enableExtrapolation();
        return QuantLib::Handle<QuantLib::YieldTermStructure>(curve);
    } catch (const py::cast_error&) {
        // Not a float, continue
    }

    // Extract discount factors from Python ORE curve
    // Standard tenor grid for reconstruction
    std::vector<std::pair<int, QuantLib::TimeUnit>> tenors = {
        {1, QuantLib::Days}, {1, QuantLib::Weeks}, {2, QuantLib::Weeks},
        {1, QuantLib::Months}, {2, QuantLib::Months}, {3, QuantLib::Months},
        {6, QuantLib::Months}, {9, QuantLib::Months},
        {1, QuantLib::Years}, {2, QuantLib::Years}, {3, QuantLib::Years},
        {4, QuantLib::Years}, {5, QuantLib::Years}, {7, QuantLib::Years},
        {10, QuantLib::Years}, {15, QuantLib::Years}, {20, QuantLib::Years},
        {25, QuantLib::Years}, {30, QuantLib::Years}, {40, QuantLib::Years},
        {50, QuantLib::Years}
    };

    std::vector<QuantLib::Date> dates;
    std::vector<double> dfs;

    // Add reference date with DF=1
    dates.push_back(ref_date);
    dfs.push_back(1.0);

    // Try to get the discount method from the Python object
    // ORE handles have a discount() method that takes a Date
    py::object discount_method;

    // Check if it's a handle (has currentLink or similar)
    if (py::hasattr(py_curve, "discount")) {
        discount_method = py_curve.attr("discount");
    } else if (py::hasattr(py_curve, "currentLink")) {
        // RelinkableHandle - get the underlying curve
        py::object linked = py_curve.attr("currentLink")();
        if (py::hasattr(linked, "discount")) {
            discount_method = linked.attr("discount");
        }
    }

    if (discount_method.is_none()) {
        throw std::runtime_error(
            "Python curve object must have a 'discount' method. "
            "Pass an ORE YieldTermStructureHandle or similar."
        );
    }

    // Import ORE module to create Date objects for calling discount()
    py::module_ ore;
    try {
        ore = py::module_::import("ORE");
    } catch (const py::error_already_set&) {
        throw std::runtime_error(
            "Could not import ORE module. Make sure ORE Python bindings are installed."
        );
    }

    // Extract discount factors at each tenor
    for (const auto& [value, unit] : tenors) {
        QuantLib::Date target_date = ref_date + QuantLib::Period(value, unit);

        // Create ORE Date object for the Python call
        py::object ore_date = ore.attr("Date")(
            target_date.dayOfMonth(),
            static_cast<int>(target_date.month()),
            target_date.year()
        );

        try {
            double df = discount_method(ore_date).cast<double>();
            if (df > 0 && df <= 1.0) {  // Sanity check
                dates.push_back(target_date);
                dfs.push_back(df);
            }
        } catch (const std::exception&) {
            // Skip tenors that fail (e.g., beyond curve range)
            continue;
        }
    }

    if (dates.size() < 2) {
        throw std::runtime_error("Could not extract enough discount factors from Python curve");
    }

    // Build a DiscountCurve from the extracted data
    auto curve = QuantLib::ext::make_shared<QuantLib::DiscountCurve>(
        dates, dfs, QuantLib::Actual365Fixed()
    );
    curve->enableExtrapolation();

    return QuantLib::Handle<QuantLib::YieldTermStructure>(curve);
}

PYBIND11_MODULE(ore_xccy_curve_cpp, m) {
    m.doc() = R"pbdoc(
        ore_xccy_curve_cpp - C++ XCCY Curve Builder with full parameter support
        -----------------------------------------------------------------------

        This module provides Python bindings for the C++ XCCYCurveBuilder,
        which has access to all 32 parameters of CrossCcyBasisMtMResetSwapHelper.

        The Python SWIG binding in ORE only exposes 18 parameters.
        This module exposes all parameters including:
        - Payment tenor (foreignTenor, domesticTenor)
        - Payment lag
        - Fixing days
        - Lookback period
        - Rate cutoff
        - Include spread flag
        - Is averaged flag
    )pbdoc";

    // FXForwardQuote
    py::class_<FXForwardQuote>(m, "FXForwardQuote")
        .def(py::init<const std::string&, double>(),
             py::arg("tenor"), py::arg("forward_points"))
        .def_readwrite("tenor", &FXForwardQuote::tenor)
        .def_readwrite("forward_points", &FXForwardQuote::forward_points);

    // XCCYBasisSwapQuote
    py::class_<XCCYBasisSwapQuote>(m, "XCCYBasisSwapQuote")
        .def(py::init<const std::string&, double>(),
             py::arg("tenor"), py::arg("basis_spread"))
        .def_readwrite("tenor", &XCCYBasisSwapQuote::tenor)
        .def_readwrite("basis_spread", &XCCYBasisSwapQuote::basis_spread);

    // OISLegConventions - OIS leg conventions for swaps
    py::class_<OISLegConventions>(m, "OISLegConventions")
        .def(py::init<const std::string&, int, int, std::optional<std::string>, int, bool, bool>(),
             py::arg("payment_tenor") = "3M",
             py::arg("payment_lag") = 2,
             py::arg("fixing_days") = 2,
             py::arg("lookback") = std::nullopt,
             py::arg("rate_cutoff") = 2,
             py::arg("include_spread") = false,
             py::arg("is_averaged") = false)
        .def_readwrite("payment_tenor", &OISLegConventions::payment_tenor)
        .def_readwrite("payment_lag", &OISLegConventions::payment_lag)
        .def_readwrite("fixing_days", &OISLegConventions::fixing_days)
        .def_readwrite("lookback", &OISLegConventions::lookback)
        .def_readwrite("rate_cutoff", &OISLegConventions::rate_cutoff)
        .def_readwrite("include_spread", &OISLegConventions::include_spread)
        .def_readwrite("is_averaged", &OISLegConventions::is_averaged);

    // CurrencyConventions - currency-specific conventions (index, calendar, day count)
    py::class_<CurrencyConventions>(m, "CurrencyConventions")
        .def(py::init<const std::string&, const std::string&, const std::string&, const std::string&>(),
             py::arg("ccy") = "",
             py::arg("ois_index_name") = "",
             py::arg("calendar_name") = "",
             py::arg("day_count") = "Actual360")
        .def_readwrite("ccy", &CurrencyConventions::ccy)
        .def_readwrite("ois_index_name", &CurrencyConventions::ois_index_name)
        .def_readwrite("calendar_name", &CurrencyConventions::calendar_name)
        .def_readwrite("day_count", &CurrencyConventions::day_count);

    // XCCYConventions - complete conventions for XCCY curve building
    py::class_<XCCYConventions>(m, "XCCYConventions")
        .def(py::init<>())
        .def(py::init<const CurrencyConventions&, const CurrencyConventions&,
                      const OISLegConventions&, const OISLegConventions&, int>(),
             py::arg("domestic"),
             py::arg("foreign"),
             py::arg("domestic_leg") = OISLegConventions(),
             py::arg("foreign_leg") = OISLegConventions(),
             py::arg("settlement_days") = 2)
        .def_readwrite("domestic", &XCCYConventions::domestic)
        .def_readwrite("foreign", &XCCYConventions::foreign)
        .def_readwrite("domestic_leg", &XCCYConventions::domestic_leg)
        .def_readwrite("foreign_leg", &XCCYConventions::foreign_leg)
        .def_readwrite("settlement_days", &XCCYConventions::settlement_days);

    // CurrencyConfig (legacy) - combined conventions for backwards compatibility
    py::class_<CurrencyConfig>(m, "CurrencyConfig")
        .def(py::init<const std::string&, const std::string&, const std::string&,
                      const std::string&, const std::string&, int, int,
                      std::optional<std::string>, int, bool, bool>(),
             py::arg("ccy") = "",
             py::arg("ois_index_name") = "",
             py::arg("calendar_name") = "",
             py::arg("day_count") = "Actual360",
             py::arg("payment_tenor") = "3M",
             py::arg("payment_lag") = 2,
             py::arg("fixing_days") = 2,
             py::arg("lookback") = std::nullopt,
             py::arg("rate_cutoff") = 2,
             py::arg("include_spread") = false,
             py::arg("is_averaged") = false)
        .def_readwrite("ccy", &CurrencyConfig::ccy)
        .def_readwrite("ois_index_name", &CurrencyConfig::ois_index_name)
        .def_readwrite("calendar_name", &CurrencyConfig::calendar_name)
        .def_readwrite("day_count", &CurrencyConfig::day_count)
        .def_readwrite("payment_tenor", &CurrencyConfig::payment_tenor)
        .def_readwrite("payment_lag", &CurrencyConfig::payment_lag)
        .def_readwrite("fixing_days", &CurrencyConfig::fixing_days)
        .def_readwrite("lookback", &CurrencyConfig::lookback)
        .def_readwrite("rate_cutoff", &CurrencyConfig::rate_cutoff)
        .def_readwrite("include_spread", &CurrencyConfig::include_spread)
        .def_readwrite("is_averaged", &CurrencyConfig::is_averaged)
        .def("to_currency_conventions", &CurrencyConfig::to_currency_conventions)
        .def("to_leg_conventions", &CurrencyConfig::to_leg_conventions);

    // XCCYMarketData - pure market data (quotes only, no conventions)
    py::class_<XCCYMarketData>(m, "XCCYMarketData")
        .def(py::init<>())
        .def_readwrite("domestic_ccy", &XCCYMarketData::domestic_ccy)  // Now just string
        .def_readwrite("foreign_ccy", &XCCYMarketData::foreign_ccy)    // Now just string
        .def_readwrite("fx_spot", &XCCYMarketData::fx_spot)
        .def_readwrite("fx_forwards", &XCCYMarketData::fx_forwards)
        .def_readwrite("xccy_basis_swaps", &XCCYMarketData::xccy_basis_swaps)
        .def_readwrite("fx_base_ccy", &XCCYMarketData::fx_base_ccy)
        .def("ccy_pair", &XCCYMarketData::ccy_pair)
        .def("is_fx_base_domestic", &XCCYMarketData::is_fx_base_domestic)
        .def("get_forward_rate", &XCCYMarketData::get_forward_rate)
        .def("get_basis_spread_bps", &XCCYMarketData::get_basis_spread_bps)
        .def("print_summary", &XCCYMarketData::print_summary)
        .def("set_valuation_date", [](XCCYMarketData& self, int year, int month, int day) {
            self.valuation_date = QuantLib::Date(day, static_cast<QuantLib::Month>(month), year);
        })
        .def("get_valuation_date", [](const XCCYMarketData& self) {
            return py::make_tuple(
                self.valuation_date.year(),
                static_cast<int>(self.valuation_date.month()),
                self.valuation_date.dayOfMonth()
            );
        });

    // ConventionsFactory - factory for creating conventions
    py::class_<ConventionsFactory>(m, "ConventionsFactory")
        .def_static("get_currency_conventions", &ConventionsFactory::get_currency_conventions,
                    py::return_value_policy::reference)
        .def_static("get_default_leg_conventions", &ConventionsFactory::get_default_leg_conventions)
        .def_static("create_xccy_conventions", &ConventionsFactory::create_xccy_conventions,
                    py::arg("domestic_ccy"), py::arg("foreign_ccy"));

    // LegacyXCCYMarketData - for backwards compatibility (quotes + conventions bundled)
    py::class_<MarketDataFactory::LegacyXCCYMarketData>(m, "LegacyXCCYMarketData")
        .def_readonly("quotes", &MarketDataFactory::LegacyXCCYMarketData::quotes)
        .def_readonly("conventions", &MarketDataFactory::LegacyXCCYMarketData::conventions);

    // MarketDataFactory
    py::class_<MarketDataFactory>(m, "MarketDataFactory")
        .def_static("get_currency_configs", &MarketDataFactory::get_currency_configs,
                    py::return_value_policy::reference)
        // New API: quotes only (pure market data)
        .def_static("create_gbpusd_quotes", [](int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return MarketDataFactory::create_gbpusd_quotes(date);
        }, py::arg("year") = 2024, py::arg("month") = 1, py::arg("day") = 15,
           "Create GBPUSD market data (quotes only, no conventions)")
        .def_static("create_eurusd_quotes", [](int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return MarketDataFactory::create_eurusd_quotes(date);
        }, py::arg("year") = 2024, py::arg("month") = 1, py::arg("day") = 15,
           "Create EURUSD market data (quotes only, no conventions)")
        .def_static("create_usdjpy_quotes", [](int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return MarketDataFactory::create_usdjpy_quotes(date);
        }, py::arg("year") = 2024, py::arg("month") = 1, py::arg("day") = 15,
           "Create USDJPY market data (quotes only, no conventions)")
        // Legacy API: quotes + conventions bundled (for backwards compatibility)
        .def_static("create_gbpusd", [](int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return MarketDataFactory::create_gbpusd(date);
        }, py::arg("year") = 2024, py::arg("month") = 1, py::arg("day") = 15,
           "Create GBPUSD market data with conventions (legacy)")
        .def_static("create_eurusd", [](int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return MarketDataFactory::create_eurusd(date);
        }, py::arg("year") = 2024, py::arg("month") = 1, py::arg("day") = 15,
           "Create EURUSD market data with conventions (legacy)")
        .def_static("create_usdjpy", [](int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return MarketDataFactory::create_usdjpy(date);
        }, py::arg("year") = 2024, py::arg("month") = 1, py::arg("day") = 15,
           "Create USDJPY market data with conventions (legacy)");

    // CurvePoint for curve utilities
    py::class_<CurvePoint>(m, "CurvePoint")
        .def_readonly("date_str", &CurvePoint::date_str)
        .def_readonly("discount_factor", &CurvePoint::discount_factor)
        .def_readonly("zero_rate", &CurvePoint::zero_rate);

    // Opaque handle wrapper for YieldTermStructure
    // We wrap the Handle as an opaque type since QuantLib types aren't directly exposed
    py::class_<QuantLib::Handle<QuantLib::YieldTermStructure>>(m, "YieldCurveHandle")
        .def("empty", &QuantLib::Handle<QuantLib::YieldTermStructure>::empty)
        .def("discount", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h,
                            int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return h->discount(date);
        }, py::arg("year"), py::arg("month"), py::arg("day"))
        .def("zero_rate", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& h,
                             int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return h->zeroRate(date, QuantLib::Actual365Fixed(), QuantLib::Continuous).rate();
        }, py::arg("year"), py::arg("month"), py::arg("day"));

    // XCCYCurveBuilder - the main class with full parameter support
    py::class_<XCCYCurveBuilder>(m, "XCCYCurveBuilder")
        .def(py::init<const XCCYMarketData&,
                      const XCCYConventions&,
                      const QuantLib::Handle<QuantLib::YieldTermStructure>&,
                      const QuantLib::Handle<QuantLib::YieldTermStructure>&,
                      const QuantLib::Handle<QuantLib::YieldTermStructure>&>(),
             py::arg("market_data"),
             py::arg("conventions"),
             py::arg("domestic_discount_curve"),
             py::arg("domestic_index_curve"),
             py::arg("foreign_index_curve"),
             R"pbdoc(
                Create an XCCYCurveBuilder.

                Args:
                    market_data: XCCYMarketData with FX and swap quotes (pure quotes)
                    conventions: XCCYConventions for swap and curve building conventions
                    domestic_discount_curve: USD discount curve (collateral)
                    domestic_index_curve: USD index curve (for domestic leg)
                    foreign_index_curve: Foreign index curve (for foreign leg)

                The builder uses CrossCcyBasisMtMResetSwapHelper with ALL 32 parameters.
             )pbdoc")
        .def("conventions", &XCCYCurveBuilder::conventions,
             py::return_value_policy::reference_internal,
             "Get the conventions used by the builder")
        .def("build", &XCCYCurveBuilder::build,
             "Build the XCCY basis curve. Returns the foreign XCCY discount curve handle.")
        .def("ccy_pair", &XCCYCurveBuilder::ccy_pair)
        .def("domestic_ccy", &XCCYCurveBuilder::domestic_ccy)
        .def("foreign_ccy", &XCCYCurveBuilder::foreign_ccy)
        .def("foreign_xccy_curve", &XCCYCurveBuilder::foreign_xccy_curve)
        .def("get_discount_factor", [](const XCCYCurveBuilder& builder,
                                       int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return builder.get_discount_factor(date);
        }, py::arg("year"), py::arg("month"), py::arg("day"))
        .def("get_zero_rate", [](const XCCYCurveBuilder& builder,
                                 int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return builder.get_zero_rate(date);
        }, py::arg("year"), py::arg("month"), py::arg("day"))
        .def("get_implied_fx_forward", [](const XCCYCurveBuilder& builder,
                                          int year, int month, int day) {
            QuantLib::Date date(day, static_cast<QuantLib::Month>(month), year);
            return builder.get_implied_fx_forward(date);
        }, py::arg("year"), py::arg("month"), py::arg("day"))
        .def("print_curve_summary", &XCCYCurveBuilder::print_curve_summary);

    // Factory function to create XCCYCurveBuilder from Python ORE curves
    m.def("create_xccy_builder_from_ore_curves",
        [](const XCCYMarketData& market_data,
           const XCCYConventions& conventions,
           py::object domestic_discount_curve,
           py::object domestic_index_curve,
           py::object foreign_index_curve) {

            // Get reference date from market data
            auto val_date = market_data.valuation_date;
            int year = val_date.year();
            int month = static_cast<int>(val_date.month());
            int day = val_date.dayOfMonth();

            // Convert Python ORE curves to C++ handles
            auto dom_disc = convert_python_curve_to_handle(
                domestic_discount_curve, year, month, day);
            auto dom_idx = convert_python_curve_to_handle(
                domestic_index_curve, year, month, day);
            auto for_idx = convert_python_curve_to_handle(
                foreign_index_curve, year, month, day);

            return XCCYCurveBuilder(market_data, conventions, dom_disc, dom_idx, for_idx);
        },
        py::arg("market_data"),
        py::arg("conventions"),
        py::arg("domestic_discount_curve"),
        py::arg("domestic_index_curve"),
        py::arg("foreign_index_curve"),
        R"pbdoc(
            Create an XCCYCurveBuilder from Python ORE/QuantLib curves.

            This function accepts curves from the ORE Python SWIG bindings
            (e.g., ore.YieldTermStructureHandle) and converts them to the
            C++ representations needed by the builder.

            Args:
                market_data: XCCYMarketData with FX and swap quotes (pure quotes)
                conventions: XCCYConventions for swap and curve building conventions
                domestic_discount_curve: ORE YieldTermStructureHandle or float (flat rate)
                domestic_index_curve: ORE YieldTermStructureHandle or float (flat rate)
                foreign_index_curve: ORE YieldTermStructureHandle or float (flat rate)

            Returns:
                XCCYCurveBuilder with full parameter support

            Example:
                import ORE as ore
                from ore_xccy_curve_cpp import create_xccy_builder_from_ore_curves

                # Create market data and conventions separately
                market_data = MarketDataFactory.create_gbpusd_quotes()
                conventions = ConventionsFactory.create_xccy_conventions("USD", "GBP")

                # Use your ORE Python curves directly
                builder = create_xccy_builder_from_ore_curves(
                    market_data,
                    conventions,
                    usd_ois_curve,     # ore.YieldTermStructureHandle
                    usd_ois_curve,
                    gbp_ois_curve,
                )
                xccy_curve = builder.build()
        )pbdoc"
    );

    // Convert Python ORE curve to C++ handle
    m.def("convert_ore_curve",
        [](py::object py_curve, int year, int month, int day) {
            return convert_python_curve_to_handle(py_curve, year, month, day);
        },
        py::arg("curve"),
        py::arg("year"),
        py::arg("month"),
        py::arg("day"),
        R"pbdoc(
            Convert an ORE Python curve to a C++ YieldCurveHandle.

            Args:
                curve: ORE YieldTermStructureHandle or float (flat rate)
                year, month, day: Reference date for the curve

            Returns:
                YieldCurveHandle that can be used with XCCYCurveBuilder
        )pbdoc"
    );

    // Utility functions
    m.def("create_flat_curve", &create_flat_curve_py,
          py::arg("year"), py::arg("month"), py::arg("day"), py::arg("rate"),
          "Create a flat forward curve for testing purposes.");

    m.def("extract_curve_points", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
                                     const std::vector<std::string>& tenors) {
        return extract_curve_points(curve, tenors);
    }, py::arg("curve"), py::arg("tenors") = std::vector<std::string>{},
       "Extract curve points (date, DF, zero rate) from a curve.");

    m.def("save_curve_to_csv", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
                                  const std::string& file_path,
                                  const std::vector<std::string>& tenors,
                                  const std::string& curve_name) {
        save_curve_to_csv(curve, file_path, tenors, curve_name);
    }, py::arg("curve"), py::arg("file_path"),
       py::arg("tenors") = std::vector<std::string>{},
       py::arg("curve_name") = "curve",
       "Save curve to CSV file.");

    m.def("save_curve_to_json", [](const QuantLib::Handle<QuantLib::YieldTermStructure>& curve,
                                   const std::string& file_path,
                                   const std::vector<std::string>& tenors,
                                   const std::string& curve_name) {
        save_curve_to_json(curve, file_path, tenors, curve_name);
    }, py::arg("curve"), py::arg("file_path"),
       py::arg("tenors") = std::vector<std::string>{},
       py::arg("curve_name") = "curve",
       "Save curve to JSON file.");

    // Version info
    m.attr("__version__") = "1.0.0";
}
