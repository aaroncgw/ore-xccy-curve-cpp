"""
Python wrapper for ore_xccy_curve_cpp C++ module.

This module provides a high-level Python interface that integrates with
the existing ore-xccy-curve Python project while using the C++ backend
for full CrossCcyBasisMtMResetSwapHelper parameter support.

New API: Market data (quotes) and conventions are separated.
- XCCYMarketDataCpp: Pure market quotes (FX spot, forwards, basis swaps)
- XCCYConventionsCpp: Swap and curve building conventions
- XCCYCurveBuilderCpp: Accepts both separately

This allows you to:
1. Create market data in Python and pass to C++ builder
2. Create conventions in Python and pass to C++ builder
3. Reuse conventions across different market data sets
"""

from datetime import date
from typing import Dict, List, Optional, Tuple

try:
    from . import _core as cpp
except ImportError:
    raise ImportError(
        "ore_xccy_curve_cpp C++ module not found. Please build and install with:\n"
        "  uv pip install c:\\projects\\ore-xccy-curve-cpp\n"
        "Or build with CMake:\n"
        "  mkdir build && cd build\n"
        "  cmake .. -DBUILD_PYTHON_BINDINGS=ON\n"
        "  cmake --build ."
    )


class OISLegConventionsCpp:
    """
    OIS leg conventions for cross-currency swaps.

    Parameters:
    - payment_tenor: Payment frequency (e.g., "3M", "6M")
    - payment_lag: Payment lag in business days
    - fixing_days: Fixing days before period start
    - lookback: Lookback period for OIS (e.g., "2D")
    - rate_cutoff: Rate cutoff days before period end
    - include_spread: Include spread in compounding
    - is_averaged: Use averaged vs compounded rate
    """

    def __init__(
        self,
        payment_tenor: str = "3M",
        payment_lag: int = 2,
        fixing_days: int = 2,
        lookback: Optional[str] = None,
        rate_cutoff: int = 2,
        include_spread: bool = False,
        is_averaged: bool = False,
    ):
        self._cpp = cpp.OISLegConventions(
            payment_tenor, payment_lag, fixing_days, lookback,
            rate_cutoff, include_spread, is_averaged
        )

    @property
    def payment_tenor(self) -> str:
        return self._cpp.payment_tenor

    @property
    def payment_lag(self) -> int:
        return self._cpp.payment_lag

    @property
    def fixing_days(self) -> int:
        return self._cpp.fixing_days

    @property
    def lookback(self) -> Optional[str]:
        return self._cpp.lookback

    @property
    def rate_cutoff(self) -> int:
        return self._cpp.rate_cutoff

    @property
    def include_spread(self) -> bool:
        return self._cpp.include_spread

    @property
    def is_averaged(self) -> bool:
        return self._cpp.is_averaged


class CurrencyConventionsCpp:
    """
    Currency-specific conventions (index, calendar, day count).
    """

    def __init__(
        self,
        ccy: str,
        ois_index_name: str,
        calendar_name: str,
        day_count: str = "Actual360",
    ):
        self._cpp = cpp.CurrencyConventions(
            ccy, ois_index_name, calendar_name, day_count
        )

    @property
    def ccy(self) -> str:
        return self._cpp.ccy

    @property
    def ois_index_name(self) -> str:
        return self._cpp.ois_index_name

    @property
    def calendar_name(self) -> str:
        return self._cpp.calendar_name

    @property
    def day_count(self) -> str:
        return self._cpp.day_count


class XCCYConventionsCpp:
    """
    Complete conventions for XCCY curve building.

    Combines currency conventions and OIS leg conventions
    for both domestic and foreign legs.
    """

    def __init__(
        self,
        domestic: CurrencyConventionsCpp,
        foreign: CurrencyConventionsCpp,
        domestic_leg: Optional[OISLegConventionsCpp] = None,
        foreign_leg: Optional[OISLegConventionsCpp] = None,
        settlement_days: int = 2,
    ):
        dom_leg = domestic_leg._cpp if domestic_leg else cpp.OISLegConventions()
        fgn_leg = foreign_leg._cpp if foreign_leg else cpp.OISLegConventions()
        self._cpp = cpp.XCCYConventions(
            domestic._cpp, foreign._cpp, dom_leg, fgn_leg, settlement_days
        )

    @classmethod
    def from_cpp(cls, cpp_obj) -> "XCCYConventionsCpp":
        """Create from C++ object."""
        obj = cls.__new__(cls)
        obj._cpp = cpp_obj
        return obj

    @classmethod
    def from_currencies(cls, domestic_ccy: str, foreign_ccy: str) -> "XCCYConventionsCpp":
        """Create conventions using standard defaults for the given currencies."""
        cpp_obj = cpp.ConventionsFactory.create_xccy_conventions(domestic_ccy, foreign_ccy)
        return cls.from_cpp(cpp_obj)

    @property
    def domestic(self) -> CurrencyConventionsCpp:
        obj = CurrencyConventionsCpp.__new__(CurrencyConventionsCpp)
        obj._cpp = self._cpp.domestic
        return obj

    @property
    def foreign(self) -> CurrencyConventionsCpp:
        obj = CurrencyConventionsCpp.__new__(CurrencyConventionsCpp)
        obj._cpp = self._cpp.foreign
        return obj

    @property
    def domestic_leg(self) -> OISLegConventionsCpp:
        obj = OISLegConventionsCpp.__new__(OISLegConventionsCpp)
        obj._cpp = self._cpp.domestic_leg
        return obj

    @property
    def foreign_leg(self) -> OISLegConventionsCpp:
        obj = OISLegConventionsCpp.__new__(OISLegConventionsCpp)
        obj._cpp = self._cpp.foreign_leg
        return obj

    @property
    def settlement_days(self) -> int:
        return self._cpp.settlement_days


class CurrencyConfigCpp:
    """
    Combined currency configuration (legacy, for backwards compatibility).

    New code should use CurrencyConventionsCpp + OISLegConventionsCpp separately.
    """

    def __init__(
        self,
        ccy: str,
        ois_index_name: str,
        calendar_name: str,
        day_count: str = "Actual360",
        payment_tenor: str = "3M",
        payment_lag: int = 2,
        fixing_days: int = 2,
        lookback: Optional[str] = None,
        rate_cutoff: int = 2,
        include_spread: bool = False,
        is_averaged: bool = False,
    ):
        self._cpp = cpp.CurrencyConfig(
            ccy, ois_index_name, calendar_name, day_count,
            payment_tenor, payment_lag, fixing_days, lookback,
            rate_cutoff, include_spread, is_averaged
        )

    @property
    def ccy(self) -> str:
        return self._cpp.ccy

    @property
    def ois_index_name(self) -> str:
        return self._cpp.ois_index_name

    @property
    def calendar_name(self) -> str:
        return self._cpp.calendar_name

    @property
    def day_count(self) -> str:
        return self._cpp.day_count

    @property
    def payment_tenor(self) -> str:
        return self._cpp.payment_tenor

    @property
    def payment_lag(self) -> int:
        return self._cpp.payment_lag

    @property
    def fixing_days(self) -> int:
        return self._cpp.fixing_days

    @property
    def lookback(self) -> Optional[str]:
        return self._cpp.lookback

    @property
    def rate_cutoff(self) -> int:
        return self._cpp.rate_cutoff

    @property
    def include_spread(self) -> bool:
        return self._cpp.include_spread

    @property
    def is_averaged(self) -> bool:
        return self._cpp.is_averaged

    def to_currency_conventions(self) -> CurrencyConventionsCpp:
        """Convert to CurrencyConventionsCpp."""
        return CurrencyConventionsCpp(
            self.ccy, self.ois_index_name, self.calendar_name, self.day_count
        )

    def to_leg_conventions(self) -> OISLegConventionsCpp:
        """Convert to OISLegConventionsCpp."""
        return OISLegConventionsCpp(
            self.payment_tenor, self.payment_lag, self.fixing_days,
            self.lookback, self.rate_cutoff, self.include_spread, self.is_averaged
        )


class XCCYMarketDataCpp:
    """
    Pure market data for XCCY curve building (quotes only, no conventions).

    This class contains only market quotes:
    - FX spot rate
    - FX forward points
    - XCCY basis swap spreads

    Conventions are provided separately via XCCYConventionsCpp.
    """

    def __init__(
        self,
        valuation_date: date,
        domestic_ccy: str,
        foreign_ccy: str,
        fx_spot: float,
        fx_forwards: List[Tuple[str, float]],
        xccy_basis_swaps: List[Tuple[str, float]],
        fx_base_ccy: Optional[str] = None,
    ):
        self._cpp = cpp.XCCYMarketData()
        self._cpp.set_valuation_date(
            valuation_date.year, valuation_date.month, valuation_date.day
        )
        self._cpp.domestic_ccy = domestic_ccy
        self._cpp.foreign_ccy = foreign_ccy
        self._cpp.fx_spot = fx_spot
        self._cpp.fx_forwards = [
            cpp.FXForwardQuote(tenor, points) for tenor, points in fx_forwards
        ]
        self._cpp.xccy_basis_swaps = [
            cpp.XCCYBasisSwapQuote(tenor, spread) for tenor, spread in xccy_basis_swaps
        ]
        if fx_base_ccy:
            self._cpp.fx_base_ccy = fx_base_ccy

    @classmethod
    def from_cpp(cls, cpp_obj) -> "XCCYMarketDataCpp":
        """Create from C++ object."""
        obj = cls.__new__(cls)
        obj._cpp = cpp_obj
        return obj

    @classmethod
    def from_dummy_gbpusd(cls, valuation_date: Optional[date] = None) -> "XCCYMarketDataCpp":
        """Create dummy GBPUSD market data (quotes only)."""
        if valuation_date is None:
            valuation_date = date(2024, 1, 15)
        data = cpp.MarketDataFactory.create_gbpusd_quotes(
            valuation_date.year, valuation_date.month, valuation_date.day
        )
        return cls.from_cpp(data)

    @classmethod
    def from_dummy_eurusd(cls, valuation_date: Optional[date] = None) -> "XCCYMarketDataCpp":
        """Create dummy EURUSD market data (quotes only)."""
        if valuation_date is None:
            valuation_date = date(2024, 1, 15)
        data = cpp.MarketDataFactory.create_eurusd_quotes(
            valuation_date.year, valuation_date.month, valuation_date.day
        )
        return cls.from_cpp(data)

    @classmethod
    def from_dummy_usdjpy(cls, valuation_date: Optional[date] = None) -> "XCCYMarketDataCpp":
        """Create dummy USDJPY market data (quotes only)."""
        if valuation_date is None:
            valuation_date = date(2024, 1, 15)
        data = cpp.MarketDataFactory.create_usdjpy_quotes(
            valuation_date.year, valuation_date.month, valuation_date.day
        )
        return cls.from_cpp(data)

    @property
    def ccy_pair(self) -> str:
        return self._cpp.ccy_pair()

    @property
    def domestic_ccy(self) -> str:
        return self._cpp.domestic_ccy

    @property
    def foreign_ccy(self) -> str:
        return self._cpp.foreign_ccy

    @property
    def fx_spot(self) -> float:
        return self._cpp.fx_spot

    def print_summary(self) -> None:
        self._cpp.print_summary()


class XCCYCurveBuilderCpp:
    """
    XCCY Curve Builder using C++ backend with full parameter support.

    This uses CrossCcyBasisMtMResetSwapHelper with ALL 32 parameters,
    unlike the Python SWIG binding which only exposes 18.

    Additional parameters available:
    - foreignTenor / domesticTenor (payment frequency)
    - foreignPaymentLag / domesticPaymentLag
    - foreignIncludeSpread / domesticIncludeSpread
    - foreignLookback / domesticLookback
    - foreignFixingDays / domesticFixingDays
    - foreignRateCutoff / domesticRateCutoff
    - foreignIsAveraged / domesticIsAveraged

    New API: Accepts market data and conventions separately.
    - market_data: XCCYMarketDataCpp with pure quotes
    - conventions: XCCYConventionsCpp with swap and curve building conventions

    Accepts curves as:
    - ORE Python YieldTermStructureHandle (from ore-xccy-curve project)
    - Float values (creates flat forward curves)
    - C++ YieldCurveHandle (from this module)
    """

    def __init__(
        self,
        market_data: XCCYMarketDataCpp,
        conventions: XCCYConventionsCpp,
        domestic_discount_curve,  # ORE handle, C++ handle, or flat rate
        domestic_index_curve,
        foreign_index_curve,
    ):
        self._market_data = market_data
        self._conventions = conventions
        val_date = market_data._cpp.get_valuation_date()
        year, month, day = val_date

        # Check if curves are ORE Python objects (have 'discount' method but not our type)
        def is_ore_curve(obj):
            """Check if object is an ORE Python curve (SWIG-wrapped)."""
            if isinstance(obj, (int, float)):
                return False
            # Check if it's our C++ handle type
            if hasattr(obj, '__class__') and 'YieldCurveHandle' in obj.__class__.__name__:
                return False
            # Check if it has discount method (ORE curve characteristic)
            return hasattr(obj, 'discount') or hasattr(obj, 'currentLink')

        # Use the factory function for ORE curves
        if is_ore_curve(domestic_discount_curve) or is_ore_curve(domestic_index_curve) or is_ore_curve(foreign_index_curve):
            # Use the factory that converts ORE curves
            self._cpp = cpp.create_xccy_builder_from_ore_curves(
                market_data._cpp,
                conventions._cpp,
                domestic_discount_curve,
                domestic_index_curve,
                foreign_index_curve,
            )
        else:
            # Convert floats to flat curves, pass C++ handles directly
            if isinstance(domestic_discount_curve, (int, float)):
                domestic_discount_curve = cpp.create_flat_curve(
                    year, month, day, float(domestic_discount_curve)
                )
            if isinstance(domestic_index_curve, (int, float)):
                domestic_index_curve = cpp.create_flat_curve(
                    year, month, day, float(domestic_index_curve)
                )
            if isinstance(foreign_index_curve, (int, float)):
                foreign_index_curve = cpp.create_flat_curve(
                    year, month, day, float(foreign_index_curve)
                )

            self._cpp = cpp.XCCYCurveBuilder(
                market_data._cpp,
                conventions._cpp,
                domestic_discount_curve,
                domestic_index_curve,
                foreign_index_curve,
            )

        self._curve = None

    @property
    def conventions(self) -> XCCYConventionsCpp:
        """Get the conventions used by the builder."""
        return self._conventions

    def build(self):
        """Build the XCCY basis curve."""
        self._curve = self._cpp.build()
        return self._curve

    @property
    def ccy_pair(self) -> str:
        return self._cpp.ccy_pair()

    @property
    def domestic_ccy(self) -> str:
        return self._cpp.domestic_ccy()

    @property
    def foreign_ccy(self) -> str:
        return self._cpp.foreign_ccy()

    def get_discount_factor(self, target_date: date) -> float:
        """Get discount factor for a given date."""
        return self._cpp.get_discount_factor(
            target_date.year, target_date.month, target_date.day
        )

    def get_zero_rate(self, target_date: date) -> float:
        """Get zero rate for a given date."""
        return self._cpp.get_zero_rate(
            target_date.year, target_date.month, target_date.day
        )

    def get_implied_fx_forward(self, target_date: date) -> float:
        """Get implied FX forward rate for a given date."""
        return self._cpp.get_implied_fx_forward(
            target_date.year, target_date.month, target_date.day
        )

    def print_curve_summary(self) -> None:
        """Print a summary of the bootstrapped curve."""
        self._cpp.print_curve_summary()

    def save_to_csv(self, file_path: str, tenors: List[str] = None, curve_name: str = None) -> None:
        """Save the curve to CSV file."""
        if self._curve is None:
            raise RuntimeError("Curve not built. Call build() first.")
        cpp.save_curve_to_csv(
            self._curve,
            file_path,
            tenors or [],
            curve_name or f"{self.foreign_ccy}_XCCY"
        )

    def save_to_json(self, file_path: str, tenors: List[str] = None, curve_name: str = None) -> None:
        """Save the curve to JSON file."""
        if self._curve is None:
            raise RuntimeError("Curve not built. Call build() first.")
        cpp.save_curve_to_json(
            self._curve,
            file_path,
            tenors or [],
            curve_name or f"{self.foreign_ccy}_XCCY"
        )


def create_flat_curve(valuation_date: date, rate: float):
    """Create a flat forward curve for testing purposes."""
    return cpp.create_flat_curve(
        valuation_date.year, valuation_date.month, valuation_date.day, rate
    )


def convert_ore_curve(ore_curve, valuation_date: date):
    """
    Convert an ORE Python curve to a C++ YieldCurveHandle.

    This allows you to use curves from the ore-xccy-curve Python project
    with the C++ XCCYCurveBuilder.

    Args:
        ore_curve: ORE YieldTermStructureHandle or RelinkableYieldTermStructureHandle
        valuation_date: Reference date for the curve

    Returns:
        C++ YieldCurveHandle
    """
    return cpp.convert_ore_curve(
        ore_curve,
        valuation_date.year,
        valuation_date.month,
        valuation_date.day,
    )


# Example usage
if __name__ == "__main__":
    print("XCCY Curve Builder - C++ Backend with Full Parameter Support")
    print("=" * 60)
    print("\nNew API: Market data and conventions are separated.")
    print("This allows you to create and modify them independently in Python.\n")

    # Create GBPUSD market data (quotes only)
    market_data = XCCYMarketDataCpp.from_dummy_gbpusd()
    market_data.print_summary()

    # Create conventions using standard defaults for USD/GBP
    conventions = XCCYConventionsCpp.from_currencies("USD", "GBP")

    # Or create custom conventions:
    # usd_conv = CurrencyConventionsCpp("USD", "SOFR", "US-FederalReserve", "Actual360")
    # gbp_conv = CurrencyConventionsCpp("GBP", "SONIA", "UK-Exchange", "Actual365Fixed")
    # custom_leg = OISLegConventionsCpp(payment_tenor="3M", payment_lag=2, fixing_days=2)
    # conventions = XCCYConventionsCpp(usd_conv, gbp_conv, custom_leg, custom_leg)

    # Create flat curves for demonstration
    val_date = date(2024, 1, 15)
    usd_curve = create_flat_curve(val_date, 0.0525)  # 5.25% SOFR
    gbp_curve = create_flat_curve(val_date, 0.0475)  # 4.75% SONIA

    # Build XCCY curve with separated market data and conventions
    builder = XCCYCurveBuilderCpp(
        market_data,
        conventions,
        domestic_discount_curve=usd_curve,
        domestic_index_curve=usd_curve,
        foreign_index_curve=gbp_curve,
    )

    builder.build()
    builder.print_curve_summary()

    # Get some values
    from datetime import timedelta
    target = val_date + timedelta(days=365 * 5)  # 5Y
    print(f"\n5Y Discount Factor: {builder.get_discount_factor(target):.10f}")
    print(f"5Y Zero Rate: {builder.get_zero_rate(target) * 100:.4f}%")
    print(f"5Y Implied FX Forward: {builder.get_implied_fx_forward(target):.4f}")
