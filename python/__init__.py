"""
ore_xccy_curve_cpp - Python bindings for C++ XCCY Curve Builder.

This package provides Python access to the C++ XCCYCurveBuilder with
full CrossCcyBasisMtMResetSwapHelper parameter support (all 32 parameters).

Supports passing ORE Python curves (from ore-xccy-curve project) directly.

New API: Market data (quotes) and conventions are separated.
- XCCYMarketDataCpp: Pure market quotes (FX spot, forwards, basis swaps)
- XCCYConventionsCpp: Swap and curve building conventions
- XCCYCurveBuilderCpp: Accepts both separately

This allows you to:
1. Create market data in Python and pass to C++ builder
2. Create conventions in Python and pass to C++ builder
3. Reuse conventions across different market data sets
"""

from .ore_xccy_curve_cpp_wrapper import (
    # New separated types
    OISLegConventionsCpp,
    CurrencyConventionsCpp,
    XCCYConventionsCpp,
    XCCYMarketDataCpp,
    XCCYCurveBuilderCpp,
    # Legacy type (for backwards compatibility)
    CurrencyConfigCpp,
    # Utility functions
    create_flat_curve,
    convert_ore_curve,
)

__all__ = [
    # New separated types
    "OISLegConventionsCpp",
    "CurrencyConventionsCpp",
    "XCCYConventionsCpp",
    "XCCYMarketDataCpp",
    "XCCYCurveBuilderCpp",
    # Legacy type (for backwards compatibility)
    "CurrencyConfigCpp",
    # Utility functions
    "create_flat_curve",
    "convert_ore_curve",
]

__version__ = "1.1.0"  # Bumped for new separated types API
